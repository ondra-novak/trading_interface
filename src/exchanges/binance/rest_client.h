#pragma once
#include "websocket_client.h"
#include "../../trading_ifc/log.h"
#include "../../trading_ifc/function.h"
#include <variant>
#include <json20.h>

#include "identity.h"
#include <list>

///context for rest client
/**
 * You can have multiple rest clients under single context
 * The context contains background thread, which handles IO operations
 */
class RestClientContext: protected WSEventListener {
public:

    ///Received empty response from the network
    static constexpr int status_empty_response = 0;
    ///Reading timeout
    static constexpr int status_timeout = -1;
    ///Operation was canceled (destructor/stop)
    static constexpr int status_canceled = -2;
    ///An error happened
    static constexpr int status_error = -3;


    ///Contains result of the call
    struct Result {
        ///status code or special status
        int status = status_empty_response;
        ///json content - if not json, contains string content
        json::value_t content = {};

        ///tests whether there were timeout
        constexpr bool is_timeout() const {return status == status_timeout;}
        ///tests whether is empty result
        constexpr bool is_empty() const {return status == status_empty_response;}
        ///tests whether canceled
        constexpr bool is_canceled() const {return status == status_canceled;};
        ///tests whether result is error
        constexpr bool is_error() const {return status < 200 || status > 203;}
    };

    using Log = trading_api::Log;

    ///construct the context
    RestClientContext(WebSocketContext &wsctx, Log log_obj = {}):_wsctx(wsctx),_log(log_obj) {}

    ///enqueue request
    /**
     * This method is called from RestClient instance - calling it
     * directly has no benefit
     *
     * @param request_factory a function, which constructs HttpClientRequest instance
     * @param callback a function which is called when result is available
     * @param timeout request timeout
     */
    template<std::invocable<const Result &> _Callback,
             std::invocable<WebSocketContext &, Log &> _ReqFactory,
             typename A,
             typename B>
    void enqueue_request(_ReqFactory &&request_factory,
                         _Callback &&callback,
                         std::chrono::duration<A,B> timeout) {

        static_assert(std::is_same_v<std::invoke_result_t<_ReqFactory,WebSocketContext &, Log &>,
                                     HttpClientRequest>);
        std::unique_lock lk(_mx);
        auto x = ++_reqcnt;
        _active_requests.emplace_back(_wsctx, Log(_log, "REST({})",x), request_factory,
                std::forward<_Callback>(callback),
                std::chrono::milliseconds(timeout)
        );
        if (!_thr.joinable()) {
            start();
            lk.unlock();
        }
        else {
            lk.unlock();
            this->signal(1);
        }

        _active_requests.back().req.notify_data_available(*this, 0);

    }

    ///dtor
    /** Rejects all requests and stops the thread
     * Can be called from inside of the IO thread, In this case,
     * it doesn't stop thread, only detaches it
     */
    ~RestClientContext() {
        stop();
    }


protected:
    struct Info {
        HttpClientRequest req;
        trading_api::Function<void(const Result res)> callback;
        std::chrono::system_clock::duration timeout;
        Log log;

        template<typename Fact, typename CB>
        Info(WebSocketContext &ctx, Log &&log, Fact &fact, CB &&cb,
                std::chrono::system_clock::duration timeout)
            :req(fact(ctx,log))
            ,callback(std::forward<CB>(cb))
            ,timeout(std::move(timeout))
            ,log(std::move(log)){}
    };

    WebSocketContext &_wsctx;
    Log _log;
    std::list<Info> _active_requests = {};
    std::jthread _thr = {};
    unsigned int _reqcnt = 0;

    void worker(std::stop_token stp) noexcept;
    void start();
    void stop();
    static thread_local bool _worker_thread;
};


///Rest client for single service
class RestClient {
public:


    ///constructs rest client
    /**
     * @param ctx rest client context
     * @param base_url base url of API
     * @param cred credentials
     * @param timeout_ms timeout for all requests in milliseconds
     */
    RestClient(RestClientContext &ctx, std::string base_url, unsigned int timeout_ms = 10000);

    ///defines types of values in params
    using Value = std::variant<std::string_view,
            int,
            unsigned int,
            long,
            unsigned long,
            long long,
            unsigned long long,
            double,
            bool>;


    ///Result
    using Result = RestClientContext::Result;

    static constexpr int status_empty_response = RestClientContext::status_empty_response;
    static constexpr int status_timeout = RestClientContext::status_timeout;
    static constexpr int status_canceled = RestClientContext::status_canceled;

    ///Param key-value definition
    using ParamKV =std::pair<std::string_view, Value>;
    ///Parameters definition
    using Params = std::initializer_list<ParamKV>;

    ///perform public request (not signed)
    /**
     * @param cmd API command
     * @param params parameters
     * @param cb callback function which is called with result
     */
    template<std::invocable<const Result &> _Callback>
    void public_call(std::string_view cmd, Params params, _Callback &&cb) const {
        _ctx.enqueue_request(FactoryPublic{this,&cmd, &params},
                std::forward<_Callback>(cb), std::chrono::milliseconds(_timeout_ms));
    }

    ///perform private signed request
    /**
     * @param method method (GET, POST, PUT, DELETE)
     * @param cmd API command
     * @param params parameters
     * @param cb callback function which is called with result
     */
    template<std::invocable<const Result &> _Callback>
    void signed_call(const Identity &ident, HttpMethod method, std::string_view cmd, Params params, _Callback &&cb) const {
        _ctx.enqueue_request(prepare_signed(ident, method, cmd, params),
              std::forward<_Callback>(cb), std::chrono::milliseconds(_timeout_ms));
    }

protected:

    using Log = RestClientContext::Log;

    struct UrlEncoded {
        const Value &v;
    };

    friend std::ostream &operator << (std::ostream &stream, const RestClient::UrlEncoded &v);

    RestClientContext &_ctx;
    std::string _base_url;
    unsigned int _timeout_ms;

    struct FactoryPublic { // @suppress("Miss copy constructor or assignment operator")
        const RestClient *owner;
        const std::string_view *cmd;
        const Params *params;
        HttpClientRequest operator()(WebSocketContext &wsctx, Log &log);
    };

    struct FactorySigned{
        HttpMethod method;
        std::string url;
        std::string body;
        HttpClientRequest::CustomHeaders hdrs;
        HttpClientRequest operator()(WebSocketContext &wsctx, Log &log);
    };

    FactorySigned prepare_signed(const Identity &ident, HttpMethod method, std::string_view cmd, Params params) const;


    template<typename Iter, typename ... Args>
    static std::string build_query(Iter beg, Iter end, const Args & ... prefixes);

    class AbstractWaitingForServerTime { // @suppress("Miss copy constructor or assignment operator")
        virtual ~AbstractWaitingForServerTime() = default;
        virtual void on_done() const noexcept = 0;
        AbstractWaitingForServerTime *_nx = nullptr;
    };


    mutable std::chrono::system_clock::duration _server_time_adjust = {};
    mutable std::atomic<int> _server_time_adjust_state = {-1};

    std::int64_t get_server_time() const;


};

std::ostream &operator << (std::ostream &stream, const RestClient::Value &v);
std::ostream &operator << (std::ostream &stream, const RestClient::UrlEncoded &v);

