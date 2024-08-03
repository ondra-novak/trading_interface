#include "websocket_client.h"
#include <coroserver/http_ws_client.h>

namespace trading_api {

WSClientImpl::WSClientImpl(coroserver::Context &ctx,coroserver::http::Client &httpc, 
                            IEvents &events, std::string url, WebSocketConfig cfg) 
    :_ctx(ctx)
    ,_httpc(httpc)
    ,_events(events)
    ,_url(std::move(url))
    ,_cfg(std::move(cfg))
{
}


WSClientImpl::~WSClientImpl()
{
    coroserver::ws::Stream w;
    bool c;
    {
        std::lock_guard _(_mx);
        c = _connecting;
        w = _stream;
    }
    if (!c) {
        w.shutdown();
    }
    _events.on_close();
}

void WSClientImpl::start() {
    std::lock_guard _(_mx);
    _connecting = true;
    _start_connect_time = std::chrono::system_clock::now();
    start_connect(weak_from_this());
}


coro::coroutine WSClientImpl::start_connect(std::weak_ptr<WSClientImpl> wkme) {
    while (true) {
        auto me = wkme.lock();
        if (!me) co_return;
        auto now = std::chrono::system_clock::now();
        auto &ctx = me->_ctx;
        try {
            auto &httpc = me->_httpc;
            auto url = me->_url;
            auto cfg = me->_cfg;
            me.reset();
            coroserver::http::ClientRequest httpr(co_await httpc.open(coroserver::http::Method::GET, url));
            if (!cfg.protocols.empty()) {
                httpr("Sec-WebSocket-Protocol",cfg.protocols);
            }
            coroserver::ws::Stream s = co_await coroserver::ws::Client::connect(httpr,
                    coroserver::TimeoutSettings(
                        std::chrono::seconds(cfg.ping_interval),std::chrono::seconds(60)));
            me = wkme.lock();
            if (me) me->set_connect_success(s);
            co_return;
        } catch (...) {
            auto me = wkme.lock();
            if (me) {
                me->set_connect_error(std::current_exception());
            } else {
                co_return;
            }
        }
        auto nx = now+std::chrono::seconds(2);
        co_await ctx.get_scheduler().sleep_until(nx);
    }

}

void WSClientImpl::set_connect_error(std::exception_ptr e) {
    //todo: log error
}

void WSClientImpl::set_connect_success(coroserver::ws::Stream s) {
    {
        std::lock_guard _(_mx);
        _stream = s;
        _connecting = false;
    }
    _events.on_open();
}    


}