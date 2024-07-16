#include <algorithm>
#include <condition_variable>
#include <deque>
#include <string>
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <vector>
#include <bitset>


///Websocket context - manages backround thread to perform IO operations above websockets
/**
 * The context must be created first and destroyed as last object. If
 * context is destroyed prior the each client, then every client receives
 * immediately close message
 *
 * context is movable
 */
class WebSocketContext {
public:

    ///create context
    WebSocketContext();
    ///destroy context
    ~WebSocketContext();
    ///start thread - thread is always started with first instance of the client
    void start();
    ///destroy the context, stop the thread
    void stop();

    ///retrieve lws context handle
    struct lws_context *get_context() const {return context.get();}

    ///events available for clients
    class Events {
    public:
        virtual void on_established() = 0;
        virtual void on_error() = 0;
        virtual void on_closed() = 0;
        virtual void on_receive(std::string_view data) = 0;
        virtual bool on_writable() = 0;
    };



protected:

    class Callback;
    struct ContextDeleter {
        void operator()(struct lws_context *) const;
    };

    using PContext =  std::unique_ptr<struct lws_context, ContextDeleter>;
    PContext context;

    static thread_local PContext destroy_on_exit;
    std::thread thr;
    std::atomic<bool> _started = {false};
    std::atomic<bool> _stopped = {false};


};

///Implements object which allows to wait on multiple clients
/**
 */

class WSEventListener {
public:

    using ClientID = std::uint16_t;

    std::basic_string<ClientID>::const_iterator begin() const {
        return _latch.begin();
    }
    std::basic_string<ClientID>::const_iterator end() const {
        return _latch.end();
    }

    ///Perform signal for given client (called from client)
    void signal(ClientID id) {
        std::lock_guard _(_mx);
        auto iter = std::find(_signals.begin(), _signals.end(), id);
        if (iter == _signals.end()) _signals.push_back(id);
        _cond.notify_all();
    }
    ///Wait for any signal
    /**
     * @note you need to wait to update bitmask of ready clients
     */
    void wait() {
        std::unique_lock lk(_mx);
        _cond.wait(lk, [&]{return !_signals.empty();});
        std::swap(_latch, _signals);
        _signals.clear();
    }
    ///Wait for any signal
    /**
     * @note you need to wait to update bitmask of ready clients
     */
    bool wait_until(std::chrono::system_clock::time_point tp) {
        std::unique_lock lk(_mx);
        if (!_cond.wait_until(lk, tp, [&]{return !_signals.empty();})) return false;
        std::swap(_latch, _signals);
        _signals.clear();
        return true;

    }
    ///Wait for any signal
    /**
     * @note you need to wait to update bitmask of ready clients
     */
    template<typename A, typename B>
    bool wait_for(std::chrono::duration<A,B> dur) {
        return wait_until(std::chrono::system_clock::now() + dur);
    }


protected:
    std::basic_string<std::uint16_t> _signals;
    std::basic_string<std::uint16_t> _latch;
    std::mutex _mx;
    std::condition_variable _cond;
};



///Websocket client instance
/**
 * Represents single connection
 *
 * Object is not movable nor copyable. It is best created dynamically
 *
 * The object handles two queues.
 * - send queue
 * - receive queue
 *
 * sending message is synchronous operation, messages are placed into
 * the queue and sent one connection allows it
 *
 * messages cannot be received asynchronously to protect IO thread (which means
 * you cannot receive immediately callback). Receiving message is blocking
 * operation. However you can perform blocking wait on multiple clients
 *
 *
 */
class WebSocketClient : protected WebSocketContext::Events{
public:
    ///type of message
    enum class MsgType {
        ///text message final
        text = 0,
        ///binary message final
        binary = 1,
        ///continuation message final
        continuation = 2,
        ///text message incomplete
        text_no_fin = 3,
        ///binary message incomplete
        binary_no_fin = 4,
        ///continuation message incomplete
        continuation_no_fin = 5,
        ///ping message
        ping = 6,
        ///pong message
        pong = 7,
        ///close message (last message in the stream)
        close = 8,
    };


    ///Message received from the websocket stream
    class RecvMessage : public std::vector<char> {
    public:
        ///sets content (called by client)
        void set_content(MsgType type, std::string_view text);
        ///retrieve type
        MsgType get_type() const {return _type;}
        bool is_text() const {return _type == MsgType::text || _type == MsgType::text_no_fin;}
        bool is_binary() const {return _type == MsgType::binary || _type == MsgType::binary_no_fin;}
        bool is_close() const {return _type == MsgType::close;}
        bool is_continuation() const {return _type == MsgType::continuation || _type == MsgType::continuation_no_fin;}
        bool is_final() const {return _type != MsgType::binary_no_fin && _type != MsgType::text_no_fin && _type != MsgType::continuation_no_fin;}
        operator std::string_view() const {return {data(), size()};}
        std::string_view to_string() const {return {data(), size()};}



    protected:
        MsgType _type = {};
    };

    ///Message to send
    /** Underlying library receives a different message layout for send
     * and for receive message. This class represents sending message.
     */
    class SendMessage {
    public:

        using value_type = std::vector<char>::value_type;

        using iterator = std::vector<char>::iterator;

        SendMessage();

        ///init message to given type
        /** To fill message, you can use back_inserter iterator */
        void init(MsgType type) {
            clear();
            _type = type;
        }

        ///retrieve data
        char *data();
        ///retrieve message size
        std::size_t size() const;
        ///begin of message
        std::vector<char>::iterator begin();
        ///end of message
        std::vector<char>::iterator end();
        ///push back byte
        void push_back(char x) {_data.push_back(x);}
        ///resize the message
        void resize(std::size_t sz);
        ///reserve space for push_back
        void reserve(std::size_t sz);
        ///clear content of message
        void clear() {resize(0);}
        ///retrieve current type
        MsgType get_type() const {return _type;}

    protected:
        std::vector<char> _data;
        MsgType _type;

    };

    ///initialize client
    /**
     * @param ctx context
     * @param url url in form wss://...
     */
    WebSocketClient(WebSocketContext &ctx, std::string_view url);
    ///destroy client
    /**
     * @note operation is synchronous and cause blocking. It requires to
     * communicate operation with the IO thread. It is better to
     * close connection explicitly and destroy the connection once
     * the reported state is closed
     */
    ~WebSocketClient();

    ///Send message
    /**
     * @param fn function receives back insert iterator, it can use iterator
     * to create message
     * @param type type of message
     * @retval true enqueued in buffer
     * @retval false connection is closing or closed
     */
    template<std::invocable<std::back_insert_iterator<SendMessage> > Fn>
    bool send(Fn &&fn, MsgType type = MsgType::text);

    ///Send message
    /**
     * @param sz size of message
     * @param fn function called with two iterators containg space for message
     * @param type type of message
     * @retval true enqueued in buffer
     * @retval false connection is closing or closed
     */
    template<std::invocable<SendMessage::iterator,SendMessage::iterator> Fn>
    bool send(std::size_t sz, Fn &&fn, MsgType type = MsgType::text);

    ///Send message
    /**
     * @param message content of message
     * @param type type of message
     * @retval true enqueued in buffer
     * @retval false connection is closing or closed
     */
    bool send(std::string_view message, MsgType type = MsgType::text);

    ///Send prepared message
    /**
     * @param msg message
     * @retval true enqueued in buffer
     * @retval false connection is closing or closed
     *
     * The function swaps the message buffer with a preallocated buffer. This
     * can be buffer used by previous send. You need to use
     * init() to reuse such message. This can help to reduce memory allocations
     */
    bool send(SendMessage &msg);

    ///Receive message
    /**
     * @param msg poiner to message instance
     * @retval true a message received
     * @retval false no message is ready
     *
     * @note function stores content of previous message in preallocation cache. Its
     * buffer can be used for next message (you can pass instance with preallocated
     * buffer). So it is better to declare one message instance and use it
     * to receive multiple messages one by one without destroying internal buffer
     */
    bool receive(RecvMessage &msg);


    ///Register WSEventListener
    /**
     * @param pl instance of event listened
     * @param id id of this client (will be identified on the poller)
     *
     * @note you can register only one WSEventListener instance. Multiple attemps
     * replaces each by other
     */
    void on_receive(WSEventListener &pl, WSEventListener::ClientID id);

    ///clear up any registered event listener
    /**
     * Don't forget to clear before WSEventListener is destroyed
     */
    void clear_on_receive();

    ///Register poller which signals everytime the output queue is consumed by one item
    /**
     * This can be useful to control size of the output queue and for example
     * to slow down sending.
     *
     * @param pl poller
     * @param id identifier of the client in mask
     */

    void on_send(WSEventListener &pl, WSEventListener::ClientID id);

    ///clear up any registered event listener
    /**
     * Don't forget to clear before WSEventListener is destroyed
     */
    void clear_on_send();

    ///Receive message synchronously
    /**
     * @note automatically clears any active event listener on_receive
     * @param msg message instance
     */
    void receive_sync(RecvMessage &msg);


    enum class State {
        ///object still connection
        /** However you can send messages, but they are stored in queue
         * and sent after connection is established
         */
        connecting,
        ///Connection is established
        established,
        ///connection has been closed by close() but it is still active
        closing,
        ///connection is no longer active (closed)
        closed
    };

    ///receive current state
    State get_state() const;

    ///close connection explicitly
    /** To ensure that connection is closed, you need to poll for
     * close message
     */
    void close();


    ///Retrieve count of messages in the output queue
    /**
     * @return count of messages
     * You can wait for decrease this number using on_send()
     */
    std::size_t get_output_queue_size() const;
    ///Retrieve count of messages in the input queue
    std::size_t get_input_queue_size() const;
    ///Retrieve output queue size in bytes
    std::size_t get_output_queue_size_bytes() const;
    ///Retrieve input queue size in bytes
    std::size_t get_input_queue_size_bytes() const;



protected:


    mutable std::mutex _mx;
    struct lws *_wsi;  //< connection handle
    std::deque<SendMessage> _send_queue; //queue of messages to send
    std::deque<RecvMessage> _recv_queue; //queue of messages received
    SendMessage _send_prealloc;   //preallocated send buffer
    RecvMessage _recv_prealloc;   //preallocated receive buffer
    bool _connecting = true;    //< sets to true when connection is established
    bool _send_pending = true;  //< true - finishing send, false - nothing to send
    bool _send_error = false;   //< previous send reported an error
    bool _recv_error = false;   //< receive reported an erro
    bool _closing = false;      //< close() issued
    bool _closed = false;       //< connection is closed
    WSEventListener *_recv_el = nullptr; //< receive event listener
    WSEventListener *_send_el = nullptr; //< send event listener
    WSEventListener::ClientID _recv_el_id = 0; //< receive event listener client id
    WSEventListener::ClientID _send_el_id = 0; //< send event listener client id


    //internal send message (locked - check for pending flags)
    bool send_lk(SendMessage &msg);
    //internal second phase of send message (locked - pending flag is false)
    void send_lk_2(SendMessage &msg);
    //notify about income data or information
    void notify();
    //handle errors
    void handle_errors();

    //event - connection established
    virtual void on_established() override;
    //event - received data
    virtual void on_receive(std::string_view data) override;
    //event - error
    virtual void on_error() override;
    //event - ready to send
    virtual bool on_writable() override;
    //event - closed
    virtual void on_closed() override;
    //generates close frame and pushes it to receive queue
    void push_close_frame();

};

template<std::invocable<std::back_insert_iterator<WebSocketClient::SendMessage> > Fn>
inline bool WebSocketClient::send(Fn &&fn, MsgType type) {
    std::lock_guard _(_mx);
    if (_closing) return false;
    SendMessage msg = std::move(_send_prealloc);
    msg.init(type);
    fn(std::back_inserter(msg));
    if (send_lk(msg)) _send_prealloc = std::move(msg);
    return true;
}

template<std::invocable<WebSocketClient::SendMessage::iterator,WebSocketClient::SendMessage::iterator> Fn>
inline bool WebSocketClient::send(std::size_t sz, Fn &&fn, MsgType type) {
    std::lock_guard _(_mx);
    if (_closing) return false;
    SendMessage msg = std::move(_send_prealloc);
    msg.init(type);
    msg.resize(sz);
    fn(msg.begin(), msg.end());
    if (send_lk(msg)) _send_prealloc = std::move(msg);
    return true;
}
