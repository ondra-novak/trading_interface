#pragma once

#include "websocket_client.h"
#include "../../trading_ifc/function.h"
#include <json20.h>

#include <map>
#include <variant>
#include <coroutine>


using trading_api::Function;

///Asynchronous RPC client
/**
 * @code
 * RPCClient client (...);
 * //async
 * client.call("method",{"args",...}) >> [=](RPCClient::Result result) {
 *  //process response
 * };
 *
 * //sync
 * RPCClient::Result res = client.call("method",{"args",...}) ;
 *
 * RPCClient::Result res = co_await call("method",{"args",...}) ;
 * @endcode
 *
 */

class RPCClient {
public:

    RPCClient(WebSocketContext &ctx, std::string url);
    ~RPCClient();

    using ID = std::size_t;

    static constexpr int status_connection_lost = -1;


    ///Contains result of rpc call
    struct Result {
        ///marks error
        bool is_error = false; ///<false = result, true = error response
        ///status code - if supported, otherwise zero
        /** when status == -1, then request has been dropped, because connection broken*/
        int status = 0;
        ///contains actuall result / error content
        json::value content = {};
    };


    ///This object is returned from the call and acts as future object
    /**
     * You should never discarded it.
     *
     * - convert it to Result - synchronous call
     * - attach callback through >> operator - asynchronous call
     * - use in co_await - asynchronou calls inside of coroutine
     */
    class [[nodiscard]] AsyncResult {
    public:
        AsyncResult(const AsyncResult &) = default;
        AsyncResult &operator=(const AsyncResult &) = delete;

        ///attach an callback
        /**
         * @param fn function, which accepts the result
         * @note only one callback at time can be attached
         * @note if the result is available, the callback is immediatelly
         * called (in the same thread)
         */
        template<std::invocable<Result> Fn>
        void operator>>(Fn &&fn) {
            owner.attach_callback(id, std::forward<Fn>(fn));
        }
        ///Get synchronously (blocking)
        Result get() const {
            return owner.wait(id);
        }
        ///Get synchronously (blocking)
        operator Result() const {
            return owner.wait(id);
        }

        ///coroutine support - result is never ready immediately
        static constexpr bool await_ready() noexcept {return false;}
        ///coroutine support - register suspended coroutine
        bool await_suspend(std::coroutine_handle<> h) {
            return owner.attach_coro(id, h);
        }
        ///coroutine support - retrieve result after resume
        Result await_resume() {
            return owner.get_no_wait(id);
        }
    private:

        RPCClient &owner;
        ID id;

        AsyncResult(RPCClient &owner,ID id)
            :owner(owner), id(id) {}

        friend class RPCClient;

    };

    ///call RPC
    /**
     * @param method method name
     * @param params parameters
     * @return instance of AsyncResult
     */
    AsyncResult call(std::string_view method, json::value params);

    ///call RPC
    /**
     * @param method method name
     * @param params parameters
     * @return instance of AsyncResult
     */
    AsyncResult operator()(std::string_view method, json::value params);

    ///Register WSEventListener "on_response" - wakes up thread when response is awaiable
    /**
     * @param listener listener instance
     * @param id assigned client id
     */
    void on_response(WSEventListener &listener, WSEventListener::ClientID id);
    ///Clear "on_response" listener
    void clear_on_response();
    ///Process responses
    void on_clear_to_send(WSEventListener &listener, WSEventListener::ClientID id);
    ///Clear "on_response" listener
    void clear_on_clear_to_send();
    ///Process responses
    /**
     * Processes all pending responses, resumes all awaiting coroutines, etc
     * @retval true processed ok
     * @retval false connection closed
     */
    bool process_responses();

    ///close connection explicitly
    /** allows to close connection and propagate state of connection to process_responses()
     */
    void close();

    ///try to reconnect (closes current connection and connects target again)
    void reconnect();


    ///retrieve web sockets last error
    std::string get_last_error() const;

    ///sends ping request
    /** when pong receives, it generates notification. You can check that pong
     * was received by get_pong_counter()
     *
     * @retval current pong counter
     */
    int send_ping();

    ///retrieves pong counter
    /**
     * everytime pong is received, counter is incremented
     * @return current pong counter
     */
    int get_pong_counter();


    class IThreadMonitor {
    public:
        virtual void on_reconnect(std::string reason) = 0;
        virtual void on_ping() = 0;
    };

    template<std::derived_from<RPCClient> T>
    static void run_thread_auto_reconnect(T &x, unsigned int ping_interval = 30, IThreadMonitor *mon = nullptr);

protected:

    using CallbackType = Function<void(Result), sizeof(Result)>;
    using CoroHandle = std::coroutine_handle<>;
    using PendingRequest = std::variant<std::monostate, CallbackType, Result, CoroHandle>;
    using PendingMap = std::map<ID, PendingRequest>;


    mutable std::mutex _mx;
    std::string _url;
    WebSocketContext &_ctx;
    union {
        WebSocketClient _client;
    };
    ID _next_id = 1;
    PendingMap _pending;
    WebSocketClient::SendMessage _send_msg_cache;
    WebSocketClient::RecvMessage _recv_msg_cache;
    std::function<bool(const json::value &)> _subclass_cb;
    std::unique_ptr<std::jthread> _thr;
    bool _explicit_close = false;

    void drop_all(std::unique_lock<std::mutex> &&lk);
    static void create_request(WebSocketClient::SendMessage &msg,
            ID id, const std::string_view &method, const json::value &params);

    void attach_callback(ID id, CallbackType cb);
    void attach_callback(std::unique_lock<std::mutex> &lk, ID id, CallbackType &cb);
    void attach_callback(std::unique_lock<std::mutex> &lk, ID id, CallbackType &&cb);
    bool attach_coro(ID id, CoroHandle h);
    Result wait(ID id);
    Result get_no_wait(ID id);
    bool is_ready(ID id);


    void reconnect(std::unique_lock<std::mutex> &&lk);

    ID call_lk(std::string_view method, json::value params);

    bool is_explicit_close() const;
};

template<std::derived_from<RPCClient> T>
inline void RPCClient::run_thread_auto_reconnect(T &inst, unsigned int ping_interval, IThreadMonitor *mon) {
    inst._thr = std::make_unique<std::jthread>([&inst, ping_interval, mon](std::stop_token stop){
     while (!stop.stop_requested()) {
        auto reconnect_after = std::chrono::system_clock::now()+std::chrono::seconds(5);
        WSEventListener lsn;
        inst.on_response(lsn, 0);
        std::stop_callback __cb(stop,[&]{
            lsn.signal(1);
        });
        bool try_ping = true;
        while (!stop.stop_requested() && inst.process_responses()) {
            if (!lsn.wait_for(std::chrono::seconds(ping_interval))) {
                if (try_ping) {
                    inst.send_ping();
                    try_ping = false;
                    if (mon) mon->on_ping();
                } else {
                    break;
                }
            }
            else {
                try_ping = true;
            }
        }
        if (stop.stop_requested()) break;
        if (mon) mon->on_reconnect(inst.get_last_error());
        std::this_thread::sleep_until(reconnect_after);
        if (stop.stop_requested()) break;
        inst.reconnect();
    }
    });
}
