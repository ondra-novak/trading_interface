
#include "rest_client.h"

#include <coroserver/chunked_stream.h>

namespace trading_api {

RestClientImpl::RestClientImpl(coroserver::Context &ctx, coroserver::ssl::Context &sslcontext, 
            IEvents &events, std::string_view url_base, unsigned int iotimeout_ms) 
:_ctx(ctx),_sslctx(sslcontext),_events(events),_iotimeout(iotimeout_ms)
,_task(worker())
{
    if (url_base.substr(0,8) == "https://") {
        _ssl = true;
        url_base=url_base.substr(8);
    } else if (url_base.substr(0,7) == "http://") {
        _ssl = false;
        url_base= url_base.substr(7);
    } else {
        throw std::runtime_error("Unsupported protocol (http:// or https:// is only supported)");
    }
    auto sep = url_base.find('/');
    _host = std::string(url_base.substr(0, sep));
    _base_path = std::string(url_base.substr(_host.size()));
}
RestClientImpl::~RestClientImpl() {
    _q.clear();
    _q.close();
    _task.wait();
    _events.on_destroy();
}



void RestClientImpl::request(std::string_view path, const HeadersIList &hdrs) {
    request(Method::GET, path, hdrs, {});
}

// Function to compare two std::string_view objects case insensitively
bool icase_compare(std::string_view str1, std::string_view str2) {
    // If the lengths are not the same, the strings can't be equal
    if (str1.size() != str2.size()) {
        return false;
    }

    // Compare each character after converting to lowercase
    return std::equal(
        str1.begin(), str1.end(),
        str2.begin(),
        [](char c1, char c2) {
            return std::tolower(c1) == std::tolower(c2);
        }
    );
}

namespace {

constexpr std::string_view forbidden_headers[] = {
    "Host", "Connection", "Transfer-Encoding","Content-Length","Upgrade","Trailer","TE","Date","Expect","User-Agent"
}; 

constexpr std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(s.back())) s = s.substr(0,s.size()-1);
    while (!s.empty() && std::isspace(s.front())) s = s.substr(1);
    return s;
}

template<bool key>
constexpr bool check_header(std::string_view s) {
    for (auto x: s) {
        if (x >= 0 && x < 32) return false;
        if (key && x == ':') return false;
    }
    return true;
}

}

void RestClientImpl::request(Method m, std::string_view path, const HeadersIList &hdrs, std::string_view body) {

    Request rq;
    Log::vector_streambuf vbuff(rq);
    std::ostream out(&vbuff);

    out << to_string(m) << " " << _base_path << path << " HTTP/1.1\r\n";
    out << "Host: " << _host << "\r\n";

    for (const auto &x : hdrs) {
        auto key = trim(x.first);
        auto value = trim(x.second);
        if (check_header<true>(key) && check_header<false>(value)
            && std::find_if(std::begin(forbidden_headers), std::end(forbidden_headers), [&](const std::string_view &k){
            return icase_compare(key,k);
        }) == std::end(forbidden_headers)) {
            out << key << ": " << value << "\r\n";
        }
    }
    if (m != Method::GET) {
        out << "Content-Length: " << body.size() << "\r\n";
    } else {
        body = {};
    }

    out << "\r\n" << body;
    _q.push(std::move(rq));
}

constexpr coroserver::kmp_pattern header_end = "\r\n\r\n";

coro::future<void> RestClientImpl::worker() {
    coroserver::Stream stream = {};
    coroserver::BinBuffer buffer;
    coroserver::BinBuffer buffer2;
    coroserver::http::HeaderMap hdrmap;
    std::chrono::system_clock::time_point last_activity = {};
    bool st;
    Request r;

    do {
        try {
            r = std::move(co_await _q.pop());
        } catch (const coro::await_canceled_exception &) {
            break;
        }
       
        bool fresh = false;
        auto now = std::chrono::system_clock::now();
rep:
        if (!stream || now > last_activity+std::chrono::seconds(5)) {
            stream = {};
            fresh = true;
            try {
                auto iplist = PeerName::lookup(_host, _ssl?"443":"80");
                if (iplist.empty()) {
                    _events.on_response({-1,"DNS Lookup failed"},{},{});
                }
                auto s = co_await _ctx.connect(iplist, std::chrono::milliseconds(_iotimeout), 
                    coroserver::TimeoutSettings(std::chrono::milliseconds(_iotimeout)));
                if (_ssl) {
                    s = coroserver::ssl::Stream::connect(s, _sslctx, _host);
                }
                stream = s;
            } catch (const std::exception &e) {
                _events.on_response({-1,e.what()},{},{});
                continue;
            }
        }

        try {
        } catch (...) {
            if (!fresh) {
                stream = {};
                goto rep;
            } else {
                _events.on_response({-1,stream.is_read_timeout()?"timeout":"connection reset"},{},{});
                continue;
            }
        }

        try {
            std::string_view tmp;
            st = co_await stream.write(std::string_view{r.data(),r.size()});
            if (st) {
                tmp = co_await stream.read();
            }
            if (tmp.empty()) {
                if (!fresh) {
                    stream = {};
                    goto rep;
                } else {
                    throw std::runtime_error("timeout");
                }
                stream.put_back(tmp);
            }
            std::string_view resp = co_await stream.read_until(buffer,header_end);
            if (resp.empty()) throw std::runtime_error("timeout");
            std::string_view first_line;
            hdrmap.headers(resp, hdrmap, first_line);
            auto splt = coroserver::splitAt(first_line, " ");
            std::string_view response_version = splt();
            std::string_view status_code_str = splt();
            auto status_code = coroserver::string2signed<int>(status_code_str.begin(), status_code_str.end(), 10);
            std::string_view status_message = splt;
            if (response_version != "HTTP/1.1") {
                throw std::runtime_error("unsupported http version");
            }

            auto hv = hdrmap["Connection"];
            bool close_con = hv == std::string_view("close");
            auto te = hdrmap["Transfer-Encoding"];
            bool chunked = te = "chunked";
            if (!chunked && te) throw std::runtime_error("Unsupported transfer encoding");
            auto cl = hdrmap["Content-Length"];
            if (cl && chunked) throw std::runtime_error("Invalid header combination");
            if (close_con && !cl && !chunked) {
                std::string_view data = co_await stream.block_read(buffer2, -1);
                if (stream.is_read_timeout()) throw std::runtime_error("timeout");
                stream = {};
                _events.on_response({status_code, status_message},std::move(hdrmap),data);
            } else if (cl) {
                std::size_t len = cl.get_uint();
                std::string_view data = co_await stream.block_read(buffer2, len);
                if (len != data.size()) throw std::runtime_error("timeout");
                _events.on_response({status_code, status_message},std::move(hdrmap),data);
            } else { //chunked
                auto chks = coroserver::ChunkedStream::read(stream);
                std::string_view data = co_await chks.block_read(buffer2, -1);
                if (chks.is_read_timeout()) {
                    throw std::runtime_error("timeout");                    
                }
                _events.on_response({status_code, status_message},std::move(hdrmap),data);
            }           
            last_activity = std::chrono::system_clock::now();
        } catch (coro::await_canceled_exception &e) {
            stream = {};
            _events.on_response({-1,"Connection lost"},{},{});
            continue;
        } catch (std::exception &e) {
            stream = {};
            _events.on_response({-1,e.what()},{},{});
            continue;
        }
   } while (true);
}
    
}