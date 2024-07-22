#include "rest_client.h"

#include <chrono>
#include <openssl/hmac.h>
#include <sstream>

RestClient::RestClient(WebSocketContext &ctx, std::string base_url, ApiCredents cred, unsigned int timeout_ms)
:_ctx(ctx)
,_base_url(std::move(base_url))
,_credents(std::move(cred))
,_timeout_ms(timeout_ms)
{
}

std::ostream &operator << (std::ostream &stream, const RestClient::Value &v) {
    std::visit([&](const auto &x){
        using T = std::decay_t<decltype(x)>;
        if constexpr(std::is_same_v<T, bool>) {
            stream << (x?"true":"false");
        } else {
            stream << x;
        }
    }, v);
}

template<typename InputIterator, typename OutputIterator>
auto url_encode(InputIterator begin, InputIterator end, OutputIterator out) {
    constexpr char *hex_chars = "0123456789ABCDEF";
    for (auto it = begin; it != end; ++it) {
        char c = *it;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '@' /*binance*/) {
            *out++ = c;
        } else {
            *out++ = '%';
            *out++ = hex_chars[(c >> 4) & 0x0F];
            *out++ = hex_chars[c & 0x0F];
        }
    }
    return out;
}

template<typename InputIterator, typename OutputIterator>
auto hex_encode(InputIterator begin, InputIterator end, OutputIterator out) {
    constexpr char *hex_chars = "0123456789abcdef";
    for (auto it = begin; it != end; ++it) {
        char c = *it;
        *out++ = hex_chars[(c >> 4) & 0x0F];
        *out++ = hex_chars[c & 0x0F];
    }
    return out;
}

std::ostream &operator << (std::ostream &stream, const RestClient::UrlEncoded &v) {
    if (std::holds_alternative<std::string_view>(v.v)) {
        auto input = std::get<std::string_view>(v.v);
        url_encode(input.begin(), input.end(), std::ostream_iterator<char>(stream));
    } else {
        stream << v.v;
    }
}

template<typename Iter, typename ... Args>
std::string RestClient::build_query(Iter beg, Iter end, const Args & ... prefixes) {
    std::ostringstream buff;
    if constexpr(sizeof...(prefixes) > 0) {
        (buff << ... << prefixes);
    }
    if (beg != end) {
        buff << beg->first << "=" << beg->second ;
        ++beg;
        while (beg != end) {
            buff << '&' << beg->first << "=" << beg->second;
            ++beg;
        }
    }
    return buff.str();
}


json::value_t RestClient::public_GET(std::string_view cmd, Params params) {
    std::string url = build_query(params.begin(), params.end(), _base_url, cmd);
    return load_response(HttpClientRequest(_ctx, url));
}

json::value_t RestClient::signed_req(HttpMethod method, std::string_view cmd, Params params) {
    std::vector<ParamKV> p(params.begin(), params.end());
    const std::string_view timestamp_str = "timestamp";
    const std::string_view signature_str = "signature";
    const std::string_view user_agent = "C++ Binance adapter";
    auto tm = get_server_time();
    p.push_back({timestamp_str, tm});
    std::string sign_msg = build_query(p.begin(),p.end());
    unsigned char md[100]; //should be enough (expected 32 bytes)
    unsigned int md_len =sizeof(md);
    HMAC(EVP_sha256(), reinterpret_cast<const unsigned char *>(_credents.secret.data()),
                _credents.secret.length(),
                reinterpret_cast<const unsigned char *>(sign_msg.data()),
                sign_msg.length()
                , md, &md_len);

    char *hex_sign = reinterpret_cast<char *>(alloca(md_len * 2));
    std::string_view sign(hex_sign, hex_encode(md, md+md_len, hex_sign));
    p.push_back({signature_str, sign});


    if (method == HttpMethod::GET) {
        return load_response(HttpClientRequest(_ctx, build_query(p.begin(), p.end(), _base_url, cmd, "?"), {
                    {"X-MBX-APIKEY", _credents.api_key},
                    {"User-Agent", std::string(user_agent)}
        }));
    } else {
        std::string whole_url(_base_url);
        whole_url.append(cmd);
        return load_response(
                HttpClientRequest(_ctx, method, whole_url, build_query(p.begin(),p.end()), {
                    {"X-MBX-APIKEY", _credents.api_key},
                    {"User-Agent", std::string(user_agent)},
                    {"Content-Type","application/x-www-form-urlencoded"}
        }));
    }

}

json::value_t RestClient::load_response(HttpClientRequest &&req) {
    HttpClientRequest::Data response;
    auto status = req.read_body_sync(response, true, _timeout_ms);
    if (status == HttpClientRequest::eof) throw std::runtime_error("Empty response");
    if (status == HttpClientRequest::eof) throw std::runtime_error("Timeout");
    try {
        return json::value_t::from_json({response.begin(), response.end()});
    } catch (...) {
        return std::string_view({response.begin(), response.end()});
    }
}

std::int64_t RestClient::get_server_time() const {
    //just mockup
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}
