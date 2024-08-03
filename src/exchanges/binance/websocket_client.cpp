
#include "websocket_client.h"
#include <libwebsockets.h>
#include <numeric>
#include <utility>

class HeaderContext: public WebSocketContext::HeaderEmit{

    lws *_wsi;
    unsigned char **_p;
    unsigned char *_end;
public:
    HeaderContext(lws *wsi,unsigned char **p,unsigned char *end)
        :_wsi(wsi),_p(p),_end(end) {}


    virtual void operator()(const char *name, std::string_view value) const override {
        std::ignore=lws_add_http_header_by_name(_wsi,
                reinterpret_cast<const unsigned char *>(name),
                reinterpret_cast<const unsigned char *>(value.data()),
                value.size(), _p, _end);
    }
};


class WebSocketContext::Callback {
public:
    static int callback(lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len) {

//        std::cout << "callback: " << static_cast<int>(reason) << ":" << user << ":" << in << ":" << len << std::endl;

        Events *ev = reinterpret_cast<Events *>(user);

        switch (reason) {
            case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
                if (ev) ev->on_http_established();
                break;
            case LWS_CALLBACK_CLIENT_ESTABLISHED:
                if (ev) ev->on_established();
                break;
            case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
            case LWS_CALLBACK_CLIENT_RECEIVE:
                if (ev) ev->on_receive({reinterpret_cast<char*>(in), len});
                break;
            case LWS_CALLBACK_CLIENT_WRITEABLE:
            case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
                if (!ev || !ev->on_writable()) return -1;
                break;
            case LWS_CALLBACK_TIMER:
                if (ev) ev->on_timer();
                break;
            case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
                if (ev) {
                    std::string_view msg;
                    if (in != nullptr) msg = {reinterpret_cast<char*>(in), len};
                    else msg = "generic IO error";
                    ev->on_error(msg);
                } else {
                    return -1;
                }
                break;
            case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
                if (ev) {
                    char buff[4096+LWS_PRE];
                    char *p = buff+LWS_PRE;
                    int l = sizeof(buff)-LWS_PRE;
                    if (lws_http_client_read(wsi, &p, &l)) {
                        ev->on_error("Http read failed");
                    }
                } else {
                    return -1;
                }
                break;
            case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
            case LWS_CALLBACK_CLIENT_CLOSED:
                if (ev) ev->on_closed();
                break;
            case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
                if (ev) ev->on_pong();
                break;
            case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
                if (ev) {
                    ev->on_add_custom_headers(HeaderContext (
                        wsi, reinterpret_cast<unsigned char **>(in),
                             *reinterpret_cast<unsigned char **>(in)+len-1
                    ));
                }
                break;
            case LWS_CALLBACK_GET_THREAD_ID: {
                    std::hash<std::thread::id> hasher;
                    return hasher(std::this_thread::get_id());
            }
            default:
                break;
        }
        return lws_callback_http_dummy(wsi, reason, user, in, len);
    }

};


WebSocketContext::WebSocketContext() {
    lws_context_creation_info context_info = {};
    lws_protocols protocols[2] = {
            {"default",&WebSocketContext::Callback::callback, 0, 0, 0, nullptr, 0},
            {nullptr, nullptr, 0, 0, 0,  nullptr, 0}
    };
    context_info.port = CONTEXT_PORT_NO_LISTEN;
    context_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    context_info.protocols = protocols;
    context_info.gid = -1;
    context_info.uid = -1;
    context_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    context = PContext(lws_create_context(&context_info));
    if (!context) throw std::runtime_error("lws context creating failed");
}


WebSocketContext::~WebSocketContext() {
    stop();
}

void WebSocketContext::start() {
    if (!_started.exchange(true)) {
        thr = std::thread([&]{
            pthread_setname_np(pthread_self(), "lws/IO");
            while (!_stopped) {
                lws_service(context.get(), 100);
            }
            destroy_on_exit.reset();
        });
    }
}

thread_local WebSocketContext::PContext WebSocketContext::destroy_on_exit = {};

void WebSocketContext::stop() {
    if (_started && !_stopped.exchange(true)) {
        if (std::this_thread::get_id() == thr.get_id()) {
            destroy_on_exit = std::move(context);
            thr.detach();
        } else {
            lws_cancel_service(context.get());
            thr.join();
        }
    }
}


void WebSocketContext::ContextDeleter::operator ()(struct lws_context* c) const {
    lws_context_destroy(c);
}

WebSocketClient::SendMessage::SendMessage() {
    _data.resize(LWS_PRE, 0);
}
std::vector<char>::iterator WebSocketClient::SendMessage::begin() {
    auto iter = _data.begin();
    std::advance(iter, LWS_PRE);
    return iter;
}

char* WebSocketClient::SendMessage::data() {
    return _data.data()+LWS_PRE;
}

std::size_t WebSocketClient::SendMessage::size() const {
    return _data.size()-LWS_PRE;
}

std::vector<char>::iterator WebSocketClient::SendMessage::end() {
    return _data.end();
}

void WebSocketClient::SendMessage::resize(std::size_t sz) {
    _data.resize(sz + LWS_PRE);
}

void WebSocketClient::SendMessage::reserve(std::size_t sz) {
    _data.reserve(sz + LWS_PRE);
}


bool WebSocketClient::send(std::string_view message, MsgType type) {
    return send(message.size(),[&](auto iter, auto ){
        std::copy(message.begin(), message.end(), iter);
    }, type);
}



bool WebSocketClient::send(SendMessage &msg) {
    std::lock_guard _(_mx);
    if (_closed || _closing) return false;
    if (!send_lk(msg)) {
        msg = std::move(_send_prealloc);
    }
    return !_closing;

}


bool WebSocketClient::send_lk(SendMessage &msg) {
    if (_send_pending) {
        _send_queue.push_back(std::move(msg));
        return false;
    } else {
        send_lk_2(msg);
        msg.clear();
        return true;
    }
}

void WebSocketClient::send_lk_2(SendMessage &msg) {
    lws_write_protocol protos[] = {LWS_WRITE_TEXT,
                    LWS_WRITE_BINARY,
                    LWS_WRITE_CONTINUATION,
                    static_cast<lws_write_protocol>(LWS_WRITE_TEXT|LWS_WRITE_NO_FIN),
                    static_cast<lws_write_protocol>(LWS_WRITE_BINARY|LWS_WRITE_NO_FIN),
                    static_cast<lws_write_protocol>(LWS_WRITE_CONTINUATION|LWS_WRITE_NO_FIN),
                    LWS_WRITE_PING,
                    LWS_WRITE_PONG};
    if (msg.get_type() == MsgType::close) {
        close();
        return;
    }

    int r = lws_write(_wsi,reinterpret_cast<unsigned char *>(msg.data()), msg.size(), protos[static_cast<int>(msg.get_type())]);
    if (r == -1) {
        _closing = true;
        _last_error = "lsw_write() failed";
    } else {
        _send_pending = true;
        lws_callback_on_writable(_wsi);
    }

}

bool WebSocketClient::receive(RecvMessage &msg) {
    std::lock_guard _(_mx);
    _stalled = false;
    if (_recv_queue.empty()) {
        if (_closed) {
            msg.set_content(MsgType::close, {});
            return true;
        } else {
            return false;
        }
    }
    if (msg.size() > _recv_prealloc.size()) {
        _recv_prealloc = std::move(msg);
    }
    msg = std::move(_recv_queue.front());
    _recv_queue.pop_front();
    return true;
}


void WebSocketClient::RecvMessage::set_content(MsgType type, std::string_view data) {
    this->resize(data.size());
    std::copy(data.begin(), data.end(), begin());
    _type = type;
}

bool WebSocketClient::check_stalled(unsigned int interval_sec) {
    std::lock_guard _(_mx);
    auto now = std::chrono::system_clock::now();
    auto expr = _last_activity_time+std::chrono::seconds(interval_sec);
    if (expr >= now) return false;
    if (!_connecting) {
        _send_ping = true;
        lws_callback_on_writable(_wsi);
    }
    _last_activity_time = now;
    return std::exchange(_stalled, true);;
}


constexpr std::pair<std::string_view, std::pair<bool, bool> > protocol_prefix[] = {
        {"https://", {true, false}},
        {"http://", {false, false}},
        {"wss://", {true,true}},
        {"ws://", {false,true}},
};

static lws *do_connect(void *user, WebSocketContext &ctx, std::string_view url,
        std::string_view protocol, const char *force_HTTP_method) {
    ctx.start();
    lws_client_connect_info connect_info = {};
    bool use_ssl = false;
    bool is_websocket = false;
    bool is_known = false;

    //lws_parse_uri is broken, so let use our solution

    for (const auto &[proto, mode]: protocol_prefix) {
        if (url.substr(0,proto.size()) == proto) {
            use_ssl = mode.first;
            is_websocket = mode.second;
            is_known = true;
            url = url.substr(proto.size());
            break;
        }
    }
    if (!is_known) {
        throw std::runtime_error("Unsupported protocol: "+ std::string(url));
    }
    if ((force_HTTP_method== nullptr) != is_websocket) {
        throw std::runtime_error("Protocol mismatch (use wss for websocket and https for http protocol): "+ std::string(url));
    }

    auto pddot = url.find(':');
    auto pathsep = url.find('/');
    std::string_view addr_str;
    unsigned long port = 0;
    std::string_view path_str;
    if (pddot < pathsep) {
        auto portstr = url.substr(pddot+1, pathsep-pddot-1);
        for (char c: portstr) {
            if (!std::isdigit(c)) {
                throw std::runtime_error("Invalid port: " + std::string(url));
            }
            port = port * 10 + (c - '0');
        }
        addr_str =  url.substr(0, pddot);
        path_str = url.substr(std::min(pathsep, url.size()));
    } else {
        if (use_ssl) port = 443; else port = 80;
        addr_str =  url.substr(0, pathsep);
        path_str = url.substr(std::min(pathsep, url.size()));
    }
    if (path_str.empty()) path_str = "/";

    auto caddr = reinterpret_cast<char *>(alloca(addr_str.size()+1));
    auto cpath = reinterpret_cast<char *>(alloca(path_str.size()+1));
    auto cprotocol = protocol.empty()?nullptr:reinterpret_cast<char *>(alloca(protocol.size()+1));
    *std::copy(addr_str.begin(), addr_str.end(), caddr) = 0;
    *std::copy(path_str.begin(), path_str.end(), cpath) = 0;
    if (!protocol.empty()) {
        *std::copy(protocol.begin(), protocol.end(), cprotocol) = 0;
    }


    connect_info.context = ctx.get_context();;
    connect_info.address = caddr;
    connect_info.port = port;
    connect_info.ssl_connection = use_ssl?LCCSCF_USE_SSL: 0;
    connect_info.path = cpath;
    connect_info.host = connect_info.address;
//    connect_info.origin = connect_info.address;
    connect_info.protocol = cprotocol;
    connect_info.pwsi = nullptr;
    connect_info.userdata = user;
    connect_info.method = force_HTTP_method;

    auto x = lws_client_connect_via_info(&connect_info);
    if (x == nullptr) {
        throw std::runtime_error("Connect failed:" + std::string(connect_info.address)+":"+std::to_string(connect_info.port));
    }
    return x;
}





WebSocketClient::WebSocketClient(WebSocketContext &ctx, std::string_view url,
        std::string_view protocol, CustomHeaders headers)
:_headers(std::move(headers))
,_last_activity_time (std::chrono::system_clock::now())
{
    try {
        _wsi = do_connect(this, ctx, url, protocol, nullptr);
    } catch (std::exception &e) {
        WebSocketClient::on_error(e.what());
    }

}




void WebSocketClient::on_established() {
    std::lock_guard _(_mx);
    _last_activity_time = std::chrono::system_clock::now();
    lws_callback_on_writable(_wsi);
    _send_pending = false;
    _connecting = false;
    if (_send_el) _send_el->signal(_send_el_id);
}

void WebSocketClient::on_receive(std::string_view data) {
    std::lock_guard _(_mx);
    _last_activity_time = std::chrono::system_clock::now();
    if (lws_is_first_fragment(_wsi)) {
        MsgType type = lws_frame_is_binary(_wsi)?MsgType::binary:MsgType::text;
        _recv_prealloc.set_content(type, data);
    } else {
        _recv_prealloc.insert(_recv_prealloc.end(), data.begin(), data.end());
    }
    if (lws_is_final_fragment(_wsi)) {
        RecvMessage msg = std::move(_recv_prealloc);
        _recv_queue.push_back(std::move(msg));
        if (_recv_el) _recv_el->signal(_recv_el_id);
    }
}

void WebSocketClient::on_error(std::string_view msg) {
    std::lock_guard _(_mx);
    _last_activity_time = std::chrono::system_clock::now();
    _closed = true;
    _connecting = false;
    _last_error = std::string(msg);
    if (_send_el) _send_el->signal(_send_el_id);
    if (_recv_el) _recv_el->signal(_recv_el_id);


}

std::size_t WebSocketClient::get_output_queue_size() const {
    std::lock_guard _(_mx);
    return _send_queue.size();
}

std::size_t WebSocketClient::get_input_queue_size() const {
    std::lock_guard _(_mx);
    return _recv_queue.size();
}

std::size_t WebSocketClient::get_output_queue_size_bytes() const {
    std::lock_guard _(_mx);
    return std::accumulate(_send_queue.begin(), _send_queue.end(), std::size_t(0),
            [](std::size_t cnt, const SendMessage &msg) {
        return cnt + msg.size();
    }
    );
}

std::size_t WebSocketClient::get_input_queue_size_bytes() const {
    std::lock_guard _(_mx);
    return std::accumulate(_recv_queue.begin(), _recv_queue.end(), std::size_t(0),
            [](std::size_t cnt, const RecvMessage &msg) {
        return cnt + msg.size();
    }
    );
}

void WebSocketClient::disable_data_available_notification() {
    std::lock_guard _(_mx);
    _recv_el = nullptr;
}

void WebSocketClient::disable_clear_to_send_notification() {
    std::lock_guard _(_mx);
    _send_el = nullptr;
}

void WebSocketClient::receive_sync(RecvMessage &msg) {
    if (!receive(msg)) {
        WSEventListener el;
        notify_data_available(el, 0);
        while (!receive(msg)) el.wait();
        disable_data_available_notification();
    }
}

void WebSocketClient::notify_data_available(WSEventListener &pl, WSEventListener::ClientID id) {
    std::lock_guard _(_mx);
    _recv_el = &pl;
    _recv_el_id = id;
    if (!_recv_queue.empty() || _closed) _recv_el->signal(id);
}

void WebSocketClient::notify_clear_to_send(WSEventListener &pl, WSEventListener::ClientID id) {
    std::lock_guard _(_mx);
    if (_closed) {
        pl.signal(id);
    } else {
        _send_el = &pl;
        _send_el_id = id;
        if (_wsi && !_connecting) lws_callback_on_writable(_wsi);
    }
}

std::string WebSocketClient::get_last_error() const {
    std::lock_guard _(_mx);
    return _last_error;
}


bool WebSocketClient::on_writable() {
    std::lock_guard _(_mx);
    _send_pending = false;
    if (_closing) {
        _closed = true;
        lws_set_wsi_user(_wsi, nullptr);
        if (_recv_el) _recv_el->signal(_recv_el_id);
        return false;
    }
    if (_send_ping) {
        _send_pending = true;
        _send_ping = false;
        unsigned char buff[LWS_PRE];
        lws_write(_wsi, buff, 0, LWS_WRITE_PING);
        lws_callback_on_writable(_wsi);
    } else if (!_send_queue.empty()) {
        SendMessage msg = _send_queue.front();
        _send_queue.pop_front();
        send_lk_2(msg);
        if (_send_prealloc.size() < msg.size()) _send_prealloc = std::move(msg);
        if (_send_el) _send_el->signal(_send_el_id);
    }
    return true;
}

WebSocketClient::State WebSocketClient::get_state() const {
    std::lock_guard _(_mx);
    if (_closed) return State::closed;
    if (_connecting) return State::connecting;
    if (_closing) return State::closing;
    return State::established;

}

WebSocketClient::~WebSocketClient() {
    State st = get_state();
    if (st != State::closed) {
        close();
        WSEventListener el;
        notify_data_available(el, 0);
        st = get_state();
        while (st != State::closed) {
            el.wait();
            st = get_state();
        }
    }
}

void WebSocketClient::close() {
    std::lock_guard _(_mx);
    _closing = true;
    if (!_send_pending) {
        lws_callback_on_writable(_wsi);
    }
}

void WebSocketClient::on_closed() {
    std::lock_guard _(_mx);
    _last_activity_time = std::chrono::system_clock::now();
    _closed = true;
    if (_recv_el) _recv_el->signal(_recv_el_id);
}

int WebSocketClient::get_pong_counter() {
    std::lock_guard _(_mx);
    return _pong_counter;
}

void WebSocketClient::on_pong() {
    std::lock_guard _(_mx);
    _last_activity_time = std::chrono::system_clock::now();
    _stalled = false;
    ++_pong_counter;
    if (_recv_el) _recv_el->signal(_recv_el_id);
}

int WebSocketClient::send_ping() {
    std::lock_guard _(_mx);
    if (!_closed && !_closing) {
        _send_ping = true;
        lws_callback_on_writable(_wsi);
    }
    return _pong_counter;
}

void WebSocketClient::on_http_established() {
}

void WebSocketClient::on_add_custom_headers(const WebSocketContext::HeaderEmit &emit) {

    for (auto &[k, v]: _headers) {
        if (k.back() != ':') k.push_back(':');
        emit( k.c_str(), v);
    }

}

static const char *getMethodStr(HttpMethod m)  {
    switch (m) {
        default: return "GET";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::POST: return "POST";
        case HttpMethod::DELETE: return "DELETE";
    }
}

HttpClientRequest::HttpClientRequest(WebSocketContext &ctx, std::string_view url, CustomHeaders hdrs)
    :HttpClientRequest(ctx, HttpMethod::GET, url, {},std::move(hdrs)) {}


HttpClientRequest::HttpClientRequest(WebSocketContext &ctx, HttpMethod method, std::string_view url, std::string_view body, CustomHeaders hdrs )
:_headers(std::move(hdrs)),_expect_body(method != HttpMethod::GET)
,_last_activity_time (std::chrono::system_clock::now())
{
    if (!body.empty()) {
        _request_body.resize(LWS_PRE);
        _request_body.insert(_request_body.end(), body.begin(), body.end());
    }
    try {
        _wsi = do_connect(this, ctx, url, {"default"}, getMethodStr(method));
    } catch (std::exception &e) {
        HttpClientRequest::on_error(e.what());
    }
}



void HttpClientRequest::on_receive(std::string_view data) {
    std::lock_guard _(_mx);
    _response.insert(_response.end(), data.begin(), data.end());
    if (_recv_el) _recv_el->signal(_recv_el_id);

}

void HttpClientRequest::on_pong() {
}

void HttpClientRequest::on_established() {
}

void HttpClientRequest::on_error(std::string_view error_msg) {
    std::lock_guard _(_mx);
    _finished = true;
    _last_error = std::string(error_msg);
    if (_recv_el) _recv_el->signal(_recv_el_id);
}

void HttpClientRequest::on_add_custom_headers(const WebSocketContext::HeaderEmit &emit) {
    for (auto &[k, v]: _headers) {
        if (k.back() != ':') k.push_back(':');
        emit(k.c_str(), v);
    }
    if (_expect_body) {
        auto len = std::to_string(_request_body.size()-LWS_PRE);
        emit("Content-Length:", len);
        lws_client_http_body_pending(_wsi, 1);
    }
}

bool HttpClientRequest::on_writable() {
    lws_write(_wsi, reinterpret_cast<unsigned char *>(_request_body.data()+LWS_PRE), _request_body.size()-LWS_PRE, LWS_WRITE_HTTP);
    lws_client_http_body_pending(_wsi, 0);
    return true;
}

void HttpClientRequest::notify_data_available(WSEventListener &evl,
        WSEventListener::ClientID id) {
    std::lock_guard _(_mx);
    _recv_el = &evl;
    _recv_el_id = id;
    if (!_response.empty() || _finished) _recv_el->signal(_recv_el_id);
}

void HttpClientRequest::disable_data_available_notification() {
    std::lock_guard _(_mx);
    _recv_el = nullptr;
}

void HttpClientRequest::on_http_established() {
    _status = lws_http_client_http_response(_wsi);
}

int HttpClientRequest::get_status_sync(unsigned int timeout_ms) {
    int status = get_status();
    WSEventListener lsn;
    notify_data_available(lsn, 0);
    while (status < 0 && lsn.wait_for(std::chrono::milliseconds(timeout_ms)) && get_last_error().empty()) {
        status = get_status();
    }
    disable_data_available_notification();
    return status;
}

void HttpClientRequest::on_closed() {
    std::lock_guard _(_mx);
    _finished = true;
    if (_recv_el) _recv_el->signal(_recv_el_id);
}

bool HttpClientRequest::read_body(Data &data) {
    std::lock_guard _(_mx);
    _last_activity_time = std::chrono::system_clock::now();
    data.clear();
    if (_response.empty()) {
        return _finished;
    }
    std::swap(data,_response);
    return true;
}

HttpClientRequest::ReadStatus HttpClientRequest::read_body_sync(Data &res_data,  bool wait_all, unsigned int timeout_ms) {
    WSEventListener evl;
    notify_data_available(evl, 0);
    if (wait_all) {
        while (!is_finished()) {
            if (!evl.wait_for(std::chrono::milliseconds(timeout_ms))) {
                disable_data_available_notification();
                return timeout;
            }
        }
    }
    while (!read_body(res_data)) {
        if (!evl.wait_for(std::chrono::milliseconds(timeout_ms))) {
            disable_data_available_notification();
            return timeout;
        }
    }
    disable_data_available_notification();
    return res_data.empty()?eof:data;
}

bool HttpClientRequest::is_finished() const {
    std::lock_guard _(_mx);
    return _finished;
}

int HttpClientRequest::get_status() const {
    std::lock_guard _(_mx);
    return _status;
}

std::string_view HttpClientRequest::get_last_error() const {
    std::lock_guard _(_mx);
    return _last_error;
}

std::chrono::system_clock::time_point WebSocketClient::get_last_activity() const {
    std::lock_guard _(_mx);
    return _last_activity_time;
}

std::chrono::system_clock::time_point HttpClientRequest::get_last_activity() const {
    std::lock_guard _(_mx);
    return _last_activity_time;
}

void HttpClientRequest::on_timer() {
    std::lock_guard _(_mx);
    if (_force_close) {
        lws_set_wsi_user(_wsi, nullptr);
        _finished = true;
        _recv_el->signal(_recv_el_id);
    }
}

HttpClientRequest::~HttpClientRequest() {
    if (!is_finished()) {
        WSEventListener lsn;
        notify_data_available(lsn, 0);
        {
            std::lock_guard _(_mx);
            _force_close = true;
            lws_set_timer_usecs(_wsi, 0);
        }
        while (!is_finished()) {
                lsn.wait();
        }
    }
}
/*
int main() {



    WebSocketContext ctx;
    WSEventListener p;
    WebSocketClient ws(ctx, "wss://echo.websocket.org");
    ws.send("Hello");
    ws.on_receive(p, 0);
    WebSocketClient::RecvMessage msg;
    do {
        while (!ws.receive(msg)) p.wait();
        std::string_view tmsg = msg;
        std::cout << tmsg << std::endl;
        if (tmsg == "Hello") break;
    } while (true);

}
*/

