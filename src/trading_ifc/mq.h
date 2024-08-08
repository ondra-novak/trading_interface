#pragma once
#include <memory>
#include "common.h"

namespace trading_api {

class IMQBroker {
public:


    using ChannelID = std::string_view;
    using MessageContent = std::string_view;

    class IMessage {
    public:
        virtual std::string_view get_sender() const = 0;
        virtual std::string_view get_channel() const = 0;
        virtual MessageContent get_content() const = 0;
        virtual ~IMessage() = default;
    };


    class Message {
    public:
        Message(const std::shared_ptr<const IMessage> &ptr):_ptr(ptr) {}
        ///Retrieve sender
        /**
         * @return retrieves sender's mailbox address. If you need to send a response to
         * the sender directly, you simply use this address as channel.
         *
         * Every listener subscribes to its local mailbox by sending a message for the first
         * time
         */
        std::string_view get_sender() const {return _ptr->get_sender();}
        ///Retrieve channel name
        /**
         * @return name of channel, where the message was posted. If the message was posted
         * to private channel, this function returns empty string
         *  */
        std::string_view get_channel() const {return _ptr->get_channel();}

        /**Retrieve message content
         * @return message content
         */
        MessageContent get_content() const {return _ptr->get_content();}

    protected:
        std::shared_ptr<const IMessage> _ptr;
    };

    class IListener {
    public:
        virtual ~IListener() = default;
        virtual void on_message(Message message) = 0;
    };

    virtual void subscribe(IListener *listener, ChannelID channel) = 0;
    virtual void unsubscribe(IListener *listener, ChannelID channel) = 0;
    virtual void unsubscribe_all(IListener *listener) = 0;
    virtual void send_message(IListener *listener, ChannelID channel, MessageContent msg) = 0;
    virtual ~IMQBroker() = default;

    class Null;
};

class IMQBroker::Null: public IMQBroker {
public:
    virtual void subscribe(IListener *, ChannelID ) override {}
    virtual void unsubscribe(IListener *, ChannelID ) override {}
    virtual void unsubscribe_all(IListener *) override {}
    virtual void send_message(IListener *, ChannelID, MessageContent ) override {}
};

class MQBroker {
public:

    using ChannelID = IMQBroker::ChannelID;
    using Message = IMQBroker::Message;
    using MessageContent = IMQBroker::MessageContent;
    using IListener = IMQBroker::IListener;

    static constexpr IMQBroker::Null null_broker = {};

    MQBroker() {
        static std::shared_ptr<IMQBroker> null_ptr(const_cast<IMQBroker::Null *>(&null_broker),[](auto){});
        _ptr = null_ptr;
    }
    MQBroker(std::shared_ptr<IMQBroker> ptr):_ptr(std::move(ptr)) {}


    ///Subscribe the channel
    /** This allows to receive messages sent by other clients to this channel
     * @param listener listener object
     * @param channel channel name. The channel name must not be empty
     * @note there is no way to check, that channel exists or if there is someone
     * listening the channel. Messages sent to non-existing channels are lost
     * @note it is not error to call this function for channel which is already subscribed.
     * In this case, the channel stays subscribed
     */
    void subscribe(IListener *listener, ChannelID channel) {
        _ptr->subscribe(listener, channel);
    }
    ///Unsubscribe the channel
    /**
     * @param listener subscribed listener
     * @param channel channel
     * @note it is not error to call this function for channel which is not subscribed
     */
    void unsubscribe(IListener *listener, ChannelID channel){
        _ptr->unsubscribe(listener, channel);
    }
    ///Unsubscribe from all channels
    /**
     * @param listener listener to unsubscribe
     * @note after all unsubscribed, you can destroy the listener
     */
    void unsubscribe_all(IListener *listener) {
        _ptr->unsubscribe_all(listener);
    }

    ///send message
    /**
     * @param listener a listener with mailbox, can be nullptr. If this pointer is null,
     * then message will be send without sender's id. (will be empty)
     * @param channel target channel
     * @param message message to send
     * @note sending message to an empty named channel always drops the message
     *
     * @note function subscribes local mailbox for the first time of call with new listener.
     * You need to unsubscribe_all() before listener is destroyed. If you don't
     * want to manage a listener instance, you can pass nullptr as listener. In this
     * case, you cannot receive any response.
     */
    void send_message(IListener *listener, ChannelID channel, MessageContent message) {
        _ptr->send_message(listener, channel, message);
    }

    auto get_handle() const {return _ptr;}

    explicit operator bool() const {return _ptr.get() != &null_broker;}
    bool defined() const {return _ptr.get() != &null_broker;}
    bool operator!() const {return _ptr.get() == &null_broker;}


protected:
    std::shared_ptr<IMQBroker> _ptr;
};

class MQClient {
public:

    using ChannelID = MQBroker::ChannelID;
    using MessageContent = MQBroker::MessageContent;
    using Message = MQBroker::Message;

    MQClient(MQBroker broker, MQBroker::IListener *listener)
        :_broker(broker),_listener(listener) {   }
    ~MQClient() {_broker.unsubscribe_all(_listener);}


    ///Subscribe the channel
    /** This allows to receive messages sent by other clients to this channel
     * @param listener listener object
     * @param channel channel name. The channel name must not be empty
     * @note there is no way to check, that channel exists or if there is someone
     * listening the channel. Messages sent to non-existing channels are lost
     * @note it is not error to call this function for channel which is already subscribed.
     * In this case, the channel stays subscribed
     */
    void subscribe(ChannelID channel) {
        _broker.subscribe(_listener, channel);
    }
    ///Unsubscribe the channel
    /**
     * @param listener subscribed listener
     * @param channel channel
     * @note it is not error to call this function for channel which is not subscribed
     */
    void unsubscribe( ChannelID channel){
        _broker.unsubscribe(_listener, channel);
    }
    ///Unsubscribe from all channels
    /**
     * @param listener listener to unsubscribe
     * @note after all unsubscribed, you can destroy the listener
     */
    void unsubscribe_all() {
        _broker.unsubscribe_all(_listener);
    }

    ///send message
    /**
     * @param channel target channel
     * @param message message to send
     * @note sending message to an empty named channel always drops the message
     */
    void send_message(ChannelID channel, MessageContent message) {
        _broker.send_message(_listener, channel, message);
    }

protected:
    MQBroker _broker;
    MQBroker::IListener *_listener;
};

namespace _details {
    template<std::output_iterator<char> Iter, typename T>
    inline Iter to_binary(Iter iter, const T &item) {

        if constexpr(std::is_same_v<T, bool>) {
            return to_binary(iter, item?1:0);
        } else if constexpr(std::is_integral_v<T>) {
            if constexpr(std::is_unsigned_v<T>) {
                T x = item;
                do {
                    *iter = static_cast<unsigned char>(x&0x7F) | ((x>=0x80)?0x80:0);
                    ++iter;
                    x>>=7;
                } while (x);
                return iter;
            } else {
                auto x = (static_cast<std::make_unsigned_t<T> >(item<0?-(item+1):item) << 1) | (item<0?1:0);
                return to_binary(iter, x);
            }
        } else if constexpr(is_container<T>) {
            auto sz = static_cast<std::size_t>(std::distance(item.begin(), item.end()));
            iter = to_binary(iter, sz);
            for (const auto &x: item) {
                iter = to_binary(iter, x);
            }
            return iter;
        } else if constexpr(std::is_trivially_copy_constructible_v<T>) {
            return std::copy(reinterpret_cast<const char *>(&item),
                             reinterpret_cast<const char *>(&item)+sizeof(item), iter);
        } else if constexpr(is_variant_type<T>) {
            iter = to_binary(iter, static_cast<unsigned int>(item.index()));
            return std::visit([&](const auto &x){return to_binary(iter, x);});
        } else if constexpr(is_optional_type<T>) {
            bool hv = item.has_value();
            iter = to_binary(iter, hv);
            if (hv) iter =  to_binary(iter, *item);
            return iter;
        } else {
            static_assert(assert_error<T>, "This type cannot be stored to a message");
            return iter;
        }
    }

    template<std::size_t idx>
    struct valid_constant {
        static constexpr std::size_t value = idx;
        static constexpr bool valid = true;
    };

    struct invalid_constant {
        static constexpr bool valid = false;
    };


    struct make_valueless_helper {
        template<typename T>
        operator T() const {throw false;}
    };

    template<std::size_t count, typename Fn,  std::size_t curidx = 0>
    auto to_constant(std::size_t idx, Fn &&fn) {
        if constexpr(curidx >= count) return fn(invalid_constant{});
        else if (curidx == idx) return fn(valid_constant<curidx>{});
        else return to_constant<count, curidx+1>(idx, std::forward<Fn>(fn));
    }


    template<typename T, typename Iter>
    T from_binary(Iter &itr, Iter end) {
        if (itr != end) {
            if constexpr(std::is_same_v<T, bool>) {
                return from_binary<int>(itr, end) != 0;
            } else if constexpr(std::is_integral_v<T>) {
                if constexpr(std::is_unsigned_v<T>) {
                    T x = {};
                    int shift = 0;
                    bool c = true;
                    while (c && itr != end) {
                        T v = static_cast<T>(static_cast<unsigned char>(*itr));
                        ++itr;
                        x |= ((v & 0x7F) << shift);
                        shift += 7;
                        c = (v & 0x80) != 0;
                    }
                    return x;
                } else {
                    auto x = from_binary<std::make_unsigned_t<T> >(itr, end);
                    auto r = static_cast<T>(x >> 1);
                    if (x & 0x1) r = -(r+1);
                    return r;
                }
            }
            if constexpr(is_container<T>) {
                auto sz = from_binary<std::size_t>(itr, end);
                T out;
                auto ins = std::inserter(out, out.end());
                while (sz && itr != end) {
                    *ins = from_binary<typename T::value_type>(itr, end);
                    ++ins;
                    --sz;
                }
                return out;
            } else if constexpr(std::is_trivially_copy_constructible_v<T>) {
                T out;
                char *c = reinterpret_cast<char *>(&out);
                char *ce = c + sizeof(out);
                while (c != ce && itr != end) {
                    *c = *itr;
                    ++c; ++itr;
                }
            } else if constexpr(is_variant_type<T>) {
                unsigned int idx = from_binary<unsigned int>(itr, end);
                return to_constant<std::variant_size_v<T> >(idx,[&](auto c){
                    if (c.valid) {
                        return T(from_binary<std::variant_alternative_t<c.value, T> >(itr, end));
                    } else {
                        return T();
                    }
                });
            } else if constexpr(is_optional_type<T>) {
                T out;
                bool hv = from_binary<bool>(itr,end);
                if (hv) {
                    out = from_binary<T::value_type>(itr,end);
                }
                return out;
            } else {
                static_assert(assert_error<T>, "This type cannot be stored to a message");
            }
        };
        return T();
    }

    template<std::output_iterator<char> Iter >
    Iter to_binary_args(Iter iter) {return iter;}

    template<std::output_iterator<char> Iter,typename T, typename ... Args>
    Iter to_binary_args(Iter iter, const T &x, const Args &...args) {
        return to_binary_args<Iter, Args...>(to_binary(iter, x), args ...);
    }


}
template<typename ... Args>
class MessageFormat{
public:


    static std::string compose(const Args & ... args) {
        std::string buff;
        _details::to_binary_args(std::back_inserter(buff), args...);
        return buff;
    }


    static std::tuple<Args...> parse(std::string_view s) {
        auto b = s.begin();
        auto e = s.end();
        return std::tuple<Args...>{_details::from_binary<Args>(b, e) ...};
    }

};


}
