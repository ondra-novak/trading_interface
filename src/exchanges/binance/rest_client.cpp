#include "rest_client.h"

#include <chrono>
#include <openssl/hmac.h>
#include <sstream>

thread_local bool RestClientContext::_worker_thread = false;

const std::string_view user_agent = "C++ Binance adapter";


void RestClientContext::start() {
    _thr = std::jthread([this](auto stp){worker(std::move(stp));});
}


void RestClientContext::worker(std::stop_token stp) noexcept {

    _worker_thread = true;
    pthread_setname_np(pthread_self(), "REST/Dispatch");
    std::unique_lock lk(this->_mx);
    std::stop_callback __(stp, [&]{
        this->signal(1);
    });
    HttpClientRequest::Data data;
    while (!stp.stop_requested()) {
        std::chrono::system_clock::time_point next_stop = std::chrono::system_clock::time_point::max();
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        for (auto iter = _active_requests.begin(); iter != _active_requests.end();) {
            auto &ar = *iter;
            lk.unlock(); //prevent deadlock acessing locked methods of clients...
            if (ar.req.is_finished()) {
                Result rs = {};
                ar.req.read_body(data);
                rs.status = ar.req.get_status();
                std::string_view sv{data.begin(), data.end()};
                std::string_view err =ar.req.get_last_error();
                if (!err.empty()) {
                    ar.log.error("{}", ar.req.get_last_error());
                }
                ar.log.trace("<< {} {}", rs.status, sv.substr(0, 255));
                try {
                    rs.content = json::value_t::from_json(sv);
                } catch (/*parse error*/...) {
                    rs.content = {data.begin(), data.end()};
                }
                ar.callback(rs);
                if (_worker_thread == false) return;
                lk.lock();  //lock when erase from list
                iter = _active_requests.erase(iter);
            } else {
                auto nstp = ar.req.get_last_activity() + ar.timeout;
                if (nstp < now) {
                    ar.log.trace("<< (TIMEOUT)");
                    ar.callback(Result{status_timeout,{}});
                    lk.lock(); //lock when erase from list
                    iter = _active_requests.erase(iter);
                } else {
                    next_stop = std::min(next_stop, nstp);
                    lk.lock(); //lock when advancing list
                    ++iter;
                }
            }
        }
        if (next_stop == std::chrono::system_clock::time_point::max()){
            this->wait(std::move(lk));
        } else {
            this->wait_until(std::move(lk), next_stop);
        }
    }
    auto _tmp = std::move(_active_requests);
    lk.unlock();
    _worker_thread = false;
    for (auto &ar: _tmp) {
        ar.log.trace("<< (CANCELED)");
        ar.callback(Result{status_canceled,{}});
    }
}

void RestClientContext::stop() {
    if (_worker_thread) {
        _worker_thread = false;
        _thr.detach();
    } else if (_thr.joinable()){
        _thr.request_stop();
        _thr.join();
    }
}



RestClient::RestClient(RestClientContext &ctx, std::string base_url,  unsigned int timeout_ms)
:_ctx(ctx)
,_base_url(std::move(base_url))
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
    return stream;
}

constexpr const char *hex_chars = "0123456789ABCDEF";

template<typename InputIterator, typename OutputIterator>
auto url_encode(InputIterator begin, InputIterator end, OutputIterator out) {
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
    return stream;
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


std::int64_t RestClient::get_server_time() const {

    if (_server_time_adjust_state != 1) {
        int need= -1;
        if (_server_time_adjust_state.compare_exchange_strong(need, 0)) {
            public_call("/v1/time", {}, [&](const Result &res){
               if (res.is_error()) {
                   _server_time_adjust_state = -1;
                   _server_time_adjust_state.notify_all();
               } else {
                   std::int64_t cur_server_time = res.content["serverTime"].get();
                   std::int64_t cur_local_time =
                           std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch()
                           ).count();
                   auto diff = cur_server_time - cur_local_time;
                   _server_time_adjust = std::chrono::milliseconds(diff);
                   _server_time_adjust_state = 1;
                   _server_time_adjust_state.notify_all();
               }
            });
        }
        need = 0;
        while (need == 0) {
            _server_time_adjust_state.wait(need);
            need = _server_time_adjust_state;
        }
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(
            (std::chrono::system_clock::now()+_server_time_adjust).time_since_epoch()).count();
}

HttpClientRequest RestClient::FactoryPublic::operator ()(WebSocketContext &wsctx, Log &log) {
    std::string url = owner->build_query(params->begin(), params->end(), owner->_base_url, *cmd, "?");
    log.trace(">> Public : GET {}", url);
    return HttpClientRequest(wsctx,std::move(url),{{"User-Agent", std::string(user_agent)}});
}

HttpClientRequest RestClient::FactorySigned::operator()(WebSocketContext &wsctx, Log &log) {
    log.trace(">> Signed : {} {} {}", method, url, body);
    return HttpClientRequest(wsctx,method,std::move(url),std::move(body),std::move(hdrs));
}

RestClient::FactorySigned RestClient::prepare_signed(const Identity &ident, HttpMethod method, std::string_view cmd, Params params)  const {
    std::vector<ParamKV> p(params.begin(), params.end());
    const std::string_view timestamp_str = "timestamp";
    const std::string_view signature_str = "signature";
    auto tm = get_server_time();
    p.push_back({timestamp_str, tm});
    std::string sign_msg = build_query(p.begin(),p.end());
    unsigned char md[100]; //should be enough (expected 32 bytes)
    unsigned int md_len =sizeof(md);
    HMAC(EVP_sha256(), reinterpret_cast<const unsigned char *>(ident.secret.data()),
                ident.secret.length(),
                reinterpret_cast<const unsigned char *>(sign_msg.data()),
                sign_msg.length()
                , md, &md_len);

    char *hex_sign = reinterpret_cast<char *>(alloca(md_len * 2));
    std::string_view sign(hex_sign, hex_encode(md, md+md_len, hex_sign));
    p.push_back({signature_str, sign});


    if (method == HttpMethod::GET) {
        return FactorySigned{method,build_query(p.begin(), p.end(), _base_url, cmd, "?"), {},
            {{"X-MBX-APIKEY", ident.name},
             {"User-Agent", std::string(user_agent)}}
        };
    } else {
        std::string whole_url(_base_url);
        whole_url.append(cmd);
        return FactorySigned(method, whole_url, build_query(p.begin(),p.end()), {
                    {"X-MBX-APIKEY", ident.name},
                    {"User-Agent", std::string(user_agent)},
                    {"Content-Type","application/x-www-form-urlencoded"}
        });
    }

}
