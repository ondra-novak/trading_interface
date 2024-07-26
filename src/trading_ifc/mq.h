#pragma once
#include <memory>

namespace trading_api {

class IMQBroker {
public:

    using ChannelID = std::string_view;
    using Message  = std::string_view;

    static constexpr ChannelID mailbox_channel = "";

    class IListener {
    public:
        virtual ~IListener() = default;
        virtual void on_message(ChannelID channel, ChannelID sender, Message message) = 0;
    };

    virtual void subscribe(IListener *listener, ChannelID channel) = 0;
    virtual void unsubscribe(IListener *listener, ChannelID channel) = 0;
    virtual void unsubscribe_all(IListener *listener) = 0;
    virtual void send_message(ChannelID channel, Message msg) = 0;
    virtual ~IMQBroker() = default;

    class Null;
};

class IMQBroker::Null: public IMQBroker {
public:
    virtual void subscribe(IListener *, ChannelID ) override {}
    virtual void unsubscribe(IListener *, ChannelID ) override {}
    virtual void unsubscribe_all(IListener *) override {}
    virtual void send_message(ChannelID, Message ) override {}
};

class MQBroker {
public:

    using ChannelID = IMQBroker::ChannelID;
    using Message = IMQBroker::Message;
    using IListener = IMQBroker::IListener;

    static constexpr IMQBroker::Null null_broker = {};
    static constexpr ChannelID mailbox_channel = IMQBroker::mailbox_channel;

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
     * @param channel target channel
     * @param message message to send
     * @note sending message to an empty named channel always drops the message
     */
    void send_message(ChannelID channel, Message message) {
        _ptr->send_message(channel, msg);
    }

    ///subscribes local mailbox
    /**
     * A local mailbox is a channel, which is created for current listener, it
     * has unique and typically random name. This name is then part of the
     * message, so other side can send a response which is delivered to back to the mailbox
     *
     * The caller typically cannot get its own ID.
     *
     * @param listener listener
     * @note it is not error to call this function after the local mailbox is already
     * installed. In this case, no new mailbox is created, it reuses existing one
     */
    void subscribe_mailbox(IListener *listener) {
        _ptr->subscribe(listener, mailbox_channel);
    }

    ///unsubscribe the local mailbox
    /**
     * Unsubscribes mailbox and destroyes it, so its UID is no longer exists
     *
     * @param listener listener
     *
     */
    void unsubscribe_mailbox(IListener *listener) {
        _ptr->unsubscribe(listener, mailbox_channel);
    }


protected:
    std::shared_ptr<IMQBroker> _ptr;
};


}
