#pragma once
#include "websocket_client.h"

#include <variant>
#include <json20.h>

class RestClient {
public:

    struct ApiCredents {
        std::string api_key;
        std::string secret;
    };

    RestClient(WebSocketContext &ctx, std::string base_url, ApiCredents cred, unsigned int timeout_ms = 10000);

    using Value = std::variant<std::string_view,
            int,
            unsigned int,
            long,
            unsigned long,
            long long,
            unsigned long long,
            double,
            bool>;

    struct UrlEncoded {
        const Value &v;
    };

    using ParamKV =std::pair<std::string_view, Value>;
    using Params = std::initializer_list<ParamKV>;

    json::value_t public_GET(std::string_view cmd, Params params);

    json::value_t signed_req(HttpMethod method, std::string_view cmd, Params params);

protected:

    WebSocketContext &_ctx;
    std::string _base_url;
    ApiCredents _credents;
    unsigned int _timeout_ms;


    template<typename Iter, typename ... Args>
    std::string build_query(Iter beg, Iter end, const Args & ... prefixes);

    json::value_t load_response(HttpClientRequest &&req);
    std::int64_t get_server_time() const;
};

std::ostream &operator << (std::ostream &stream, const RestClient::Value &v);
std::ostream &operator << (std::ostream &stream, const RestClient::UrlEncoded &v);

