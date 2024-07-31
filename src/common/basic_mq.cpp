#include "basic_mq.h"
#include <algorithm>
#include <random>

#include <atomic>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> 
#else
#include <unistd.h>
#endif


namespace trading_api {



template<typename Iter>
Iter to_base62(std::uint64_t x, Iter iter, int digits = 1) {
    if (x>0 || digits>0) {
        iter = to_base62(x/62, iter, digits-1);
        auto rm = x%62;
        char c = rm < 10?'0'+rm:rm<36?'A'+rm-10:'a'+rm-36;
        *iter = c;
        ++iter;
        return iter;
    }
    return iter;
} 

static std::string generate_mailbox_id() {
    static std::atomic<std::uint64_t> counter = {0};
    static constexpr std::string_view mbx_prefix = "mbx_"; 
    char buff[64];
    std::random_device dev;
    auto rnd = dev();
    auto now = std::chrono::system_clock::now();
    char *iter = buff;
    #ifdef _WIN32
        auto pid = GetCurrentProcessId(); 
    #else
        auto pid= ::getpid();
    #endif
    iter = std::copy(mbx_prefix.begin(), mbx_prefix.end(), iter);
    iter = to_base62(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(), iter);
    iter = to_base62(pid, iter);
    iter = to_base62(counter++,iter,1);
    iter = to_base62(rnd,iter, 1);
    return std::string(buff, iter);
}



void BasicMQ::subscribe(IListener *listener, ChannelID channel)
{
    std::lock_guard _(_mx);
    auto iter = _channels.find(channel);
    if (iter == _channels.end()) {
            std::vector<char> name(channel.begin(), channel.end());
            std::string_view sname(name.data(), name.size());
            iter = _channels.emplace(sname, ChanMapItem{{},std::move(name)}).first;
    }
    iter->second._items.push_back(listener);

    _listeners[listener].push_back(std::string(iter->first));
    
}

void BasicMQ::unsubscribe(IListener *listener, ChannelID channel)
{
    std::lock_guard _(_mx);
    remove_channel_from_listener(channel,listener);
    remove_listener_from_channel(channel, listener);

}

void BasicMQ::unsubscribe_all(IListener *listener)
{
    std::lock_guard _(_mx);
    erase_mailbox(listener);
    auto iter = _listeners.find(listener);
    if (iter != _listeners.end()) {
        auto &chans = iter->second;
        for (auto &ch: chans) {
            remove_listener_from_channel(ch, listener);
        }
        _listeners.erase(iter);
    }
}

void BasicMQ::erase_mailbox(IListener *listener) {
        auto iter = _mailboxes_by_ptr.find(listener);
        if (iter == _mailboxes_by_ptr.end()) return;
        _mailboxes_by_name.erase(iter->second);
        _mailboxes_by_ptr.erase(iter);
}

std::string_view BasicMQ::create_mailbox(IListener *listener)
{
    auto iter = _mailboxes_by_ptr.find(listener);
    if (iter != _mailboxes_by_ptr.end()) return iter->second;
    std::string id = generate_mailbox_id(); 
    iter = _mailboxes_by_ptr.emplace(listener, std::move(id)).first;
    return _mailboxes_by_name.emplace(iter->second, listener).first->first;
}

void BasicMQ::send_message(IListener *listener, ChannelID channel, MessageContent message)
{
    std::lock_guard _(_mx);
    std::string_view sender = create_mailbox(listener);

    {
        auto iter = _mailboxes_by_name.find(channel);
        if (iter != _mailboxes_by_name.end()) {
            auto msg = std::make_shared<MessageDef>(
                std::string(sender), std::string(), std::string(message)
            );
            iter->second->on_message(Message(msg));
            return;
        }
    }

    {
        auto iter = _channels.find(std::string(channel));
        if (iter == _channels.end()) return; //unknown channel, drop message

        Message msg ( std::make_shared<MessageDef>(sender, channel, message));
        for (auto l: iter->second._items) {
            l->on_message(msg);
        }
    }
}

void BasicMQ::remove_listener_from_channel(std::string_view channel, IListener *listener) {
    auto iter = _channels.find(channel);
    if (iter == _channels.end()) return;
    auto l = std::remove(iter->second._items.begin(),
                        iter->second._items.end(), listener);
    iter->second._items.erase(l, iter->second._items.end());

    if (iter->second._items.empty()) {
        _channels.erase(iter);
    }
}

void BasicMQ::remove_channel_from_listener(std::string_view channel, IListener *listener)
{
    auto iter = _listeners.find(listener);
    if (iter == _listeners.end()) return;
    iter->second.erase(std::find(iter->second.begin(),
                                 iter->second.end(), channel), iter->second.end());    
    if (iter->second.empty()) {
        _listeners.erase(iter);
    }
}
}