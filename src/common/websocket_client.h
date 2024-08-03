#include "../trading_ifc/network.h"

#include <coroserver/websocket_stream.h>
#include <coroserver/https_client.h>



namespace trading_api {


class WSClientImpl: public IWebSocketClient, public std::enable_shared_from_this<WSClientImpl> {
public:

    WSClientImpl(coroserver::Context &ctx, coroserver::http::Client httpc, IEvents &events, std::string url, WebSocketConfig cfg);

    void start();
    void reconnect();

    virtual bool send(std::string_view msg) override;
    virtual bool send(binary_string_view msg) override;
    virtual bool close() override;

    ~WSClientImpl() override;


protected:
    coroserver::Context &_ctx;
    coroserver::http::Client _httpc;    
    IEvents &_events;
    std::string _url;
    WebSocketConfig _cfg;
    std::mutex _mx;
    coroserver::ws::Stream _stream;

    static coro::coroutine start_connect(std::weak_ptr<WSClientImpl> wkme);
    static coro::coroutine reconnect(coroserver::Context &ctx,
                                     std::weak_ptr<WSClientImpl> wkme, 
                                     std::chrono::system_clock::time_point tp_start);
    void set_connect_error(std::exception_ptr e);
    void set_connect_success(coroserver::ws::Stream s);    
    coro::coroutine reader();
    coroserver::ws::Stream lock();

};


}