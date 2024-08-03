#pragma once

#include "../trading_ifc/network.h"
#include "../trading_ifc/log.h"
#include <coroserver/https_client.h>

namespace trading_api {

class RestClientImpl: public IRestClient {
public:


    RestClientImpl(coroserver::Context &ctx, coroserver::ssl::Context &sslcontext, 
                IEvents &events, std::string_view url_base, unsigned int iotimeout_ms);
    virtual void request(std::string_view path, const HeadersIList &hdrs) override;
    virtual void request(Method m, std::string_view path, const HeadersIList &hdrs, std::string_view body) override;

    ~RestClientImpl();

protected:

    using Request = std::vector<char>; 
    using RequestQueue = coro::queue<Request>;

    coroserver::Context &_ctx;
    coroserver::ssl::Context &_sslctx;
    IEvents &_events;
    unsigned int _iotimeout;
    coro::future<void> _task;

    RequestQueue _q;
    std::string _base_path;
    std::string _host;
    bool _ssl;



    coro::future<void> worker();
};

}