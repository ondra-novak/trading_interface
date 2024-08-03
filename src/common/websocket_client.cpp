#include "websocket_client.h"
#include <coroserver/http_ws_client.h>

namespace trading_api {

WSClientImpl::WSClientImpl(coroserver::Context &ctx,coroserver::http::Client httpc, 
                            IEvents &events, std::string url, WebSocketConfig cfg) 
    :_ctx(ctx)
    ,_httpc(httpc)
    ,_events(events)
    ,_url(std::move(url))
    ,_cfg(std::move(cfg))
{
}


coroserver::ws::Stream WSClientImpl::lock() {
    std::lock_guard _(_mx);
    return _stream;
}


WSClientImpl::~WSClientImpl()
{
    coroserver::ws::Stream w = lock();
    if (w) w.shutdown();
    _events.on_destroy();
}

void WSClientImpl::start() {
    std::lock_guard _(_mx);
    _stream = {};
    start_connect(weak_from_this());
}

void WSClientImpl::reconnect() {
    std::lock_guard _(_mx);
    _stream = {};
    if (_cfg.reconnect) {
        start_connect(weak_from_this());
    }
}


coro::coroutine WSClientImpl::start_connect(std::weak_ptr<WSClientImpl> wkme) {
    while(true) {
        auto me = wkme.lock();
        if (!me) co_return;
        auto now = std::chrono::system_clock::now();
        auto &ctx = me->_ctx;
        try {
            auto httpc = me->_httpc;
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

void WSClientImpl::set_connect_error(std::exception_ptr ) {
    //todo: log error
}

void WSClientImpl::set_connect_success(coroserver::ws::Stream s) {
    {
        std::lock_guard _(_mx);
        _stream = s;
    }
    reader();
    _events.on_open();
}    

coro::coroutine WSClientImpl::reader() {
    using namespace coroserver::ws;
    try {
        while (true) {
            Message msg = co_await _stream.receive();
            switch (msg.type) {
                case Type::text:
                    _events.on_message(msg.payload);
                    break;
                case Type::binary:
                    _events.on_message(binary_string_view(
                        reinterpret_cast<const unsigned char *>(msg.payload.data()),
                        msg.payload.size()));
                    break;
                case Type::connClose:
                    _events.on_close();
                    reconnect();
                    co_return;
                default:
                    break;
            }
        }
    } catch (...) {
        _events.on_close();
        reconnect();
    }
} 

bool WSClientImpl::send(std::string_view msg) {
    auto s = lock();
    return s?s.send({msg, coroserver::ws::Type::text}):false;
}
bool WSClientImpl::send(binary_string_view msg) {
    auto s = lock();
    return s?s.send({{reinterpret_cast<const char *>(msg.data()), msg.size()}, coroserver::ws::Type::binary}):false;
}
bool WSClientImpl::close() {
    using namespace coroserver::ws;
    Stream s;
    {
        std::lock_guard _(_mx);
        s = std::move(_stream);
        _cfg.reconnect = false;
    }
    if (s) {
        auto c = [](Stream s) -> coro::coroutine {
            co_await s.send_close();
        };
        c(std::move(s));
        return true;
    } else {
        return false;
    }
}


}