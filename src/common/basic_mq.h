#pragma once

#include "../trading_ifc/mq.h"

#include <string>
#include <mutex>
#include <vector>
#include <unordered_map>
namespace trading_api {



class BasicMQ: public IMQBroker {
public:

    class MessageDef: public IMQBroker::IMessage {
    public:
        MessageDef(std::string_view sender, std::string_view channel, std::string_view message)
            :sender(sender),channel(channel),message(message) {}
        virtual std::string_view get_sender() const override {return sender;}
        virtual std::string_view get_channel() const override {return channel;}
        virtual MessageContent get_content() const override {return message;}

    protected:
        std::string sender;
        std::string channel;
        std::string message;
    };
    
    virtual void subscribe(IListener *listener, ChannelID channel) override;
    virtual void unsubscribe(IListener *listener, ChannelID channel) override;
    virtual void unsubscribe_all(IListener *listener) override;
    virtual void send_message(IListener *listener, ChannelID channel, MessageContent msg) override;



protected:

    struct ChanMapItem {
        std::vector<IListener *> _items;
        std::vector<char> _name;
    };


    using ListenerMap = std::unordered_map<IListener *, std::vector<std::string_view>  >;
    using ChannelMap = std::unordered_map<std::string_view, ChanMapItem>;
    ListenerMap _listeners;
    ChannelMap _channels; 
    std::unordered_map<IListener *, std::string> _mailboxes_by_ptr;
    std::unordered_map<std::string_view, IListener *> _mailboxes_by_name;
    std::recursive_mutex _mx;

    std::string_view find_mailbox(IListener *lsn) const;

    void remove_listener_from_channel(std::string_view channel, IListener *listener);
    void remove_channel_from_listener(std::string_view channel, IListener *listener);
    void erase_mailbox(IListener *listener);
    std::string_view create_mailbox(IListener *listener);
};

}