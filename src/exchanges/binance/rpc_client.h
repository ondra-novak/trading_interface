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

protected:

    using CallbackType = Function<void(Result), sizeof(Result)>;
    using CoroHandle = std::coroutine_handle<>;
    using PendingRequest = std::variant<std::monostate, CallbackType, Result, CoroHandle>;
    using PendingMap = std::map<ID, PendingRequest>;


    std::mutex _mx;
    WebSocketClient _client;
    ID _next_id = 1;
    PendingMap _pending;
    WebSocketClient::SendMessage _send_msg_cache;
    WebSocketClient::RecvMessage _recv_msg_cache;

    void drop_all();
    static void create_request(WebSocketClient::SendMessage &msg,
            ID id, const std::string_view &method, const json::value &params);

    void attach_callback(ID id, CallbackType cb);
    bool attach_coro(ID id, CoroHandle h);
    Result wait(ID id);
    Result get_no_wait(ID id);
    bool is_ready(ID id);
    ///called before json message is processed
    /**
     * @param message
     * @retval true processed, no futher process
     * @retval false not processed, continue in default processing
     */
    virtual bool on_json_message(const json::value &) {return false;}
};
