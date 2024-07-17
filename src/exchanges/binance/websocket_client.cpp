
#include "websocket_client.h"
#include <libwebsockets.h>
#include <numeric>

class WebSocketContext::Callback {
public:
    static int callback(lws *, lws_callback_reasons reason, void *user, void *in, size_t len) {

        Events *ev = reinterpret_cast<Events *>(user);

        switch (reason) {
            case LWS_CALLBACK_CLIENT_ESTABLISHED:
                if (ev) ev->on_established();
                break;
            case LWS_CALLBACK_CLIENT_RECEIVE:
                if (ev) ev->on_receive({reinterpret_cast<char*>(in), len});
                break;
            case LWS_CALLBACK_CLIENT_WRITEABLE:
                if (!ev || !ev->on_writable()) return -1;
                break;
            case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
                if (ev) ev->on_error();
                break;
            case LWS_CALLBACK_CLIENT_CLOSED:
                if (ev) ev->on_closed();
                break;

            default:
                break;
        }
        return 0;

    }
};


WebSocketContext::WebSocketContext() {
    lws_context_creation_info context_info = {};
    lws_protocols protocols[2] = {
            {"",&WebSocketContext::Callback::callback, 0, 0, 0, nullptr, 0},
            {nullptr, nullptr, 0, 0,0,  nullptr, 0}
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
    if (_closing) return false;
    if (!send_lk(msg)) {
        msg = std::move(_send_prealloc);
    }
    return true;

}


bool WebSocketClient::send_lk(SendMessage &msg) {
    handle_errors();
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
        _send_error = true;
        notify();
    } else {
        _send_pending = true;
        lws_callback_on_writable(_wsi);
    }

}

bool WebSocketClient::receive(RecvMessage &msg) {
    std::lock_guard _(_mx);
    handle_errors();
    if (_recv_queue.empty()) return false;
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


void WebSocketClient::notify() {
     if (_recv_el) _recv_el->signal(_recv_el_id);
}

WebSocketClient::WebSocketClient(WebSocketContext &ctx, std::string_view url) {
    ctx.start();
    lws_client_connect_info connect_info = {};
    bool use_ssl = false;

    if (url.substr(0,6) == "wss://") {
        use_ssl = true;
        url = url.substr(6);
    } else if (url.substr(0,5) == "ws://") {
        use_ssl = false;
        url = url.substr(5);
    } else {
        throw std::runtime_error("Unknown websocket protocol: "+ std::string(url));
    }

    auto pddot = url.find(':');
    auto pathsep = url.find('/');
    std::string_view addr_str;
    unsigned long port = 0;
    std::string_view path_str;
    if (pddot < pathsep) {
        auto portstr = url.substr(pddot+1, pathsep-pddot-1);
        for (char c: portstr) {
            if (!std::isdigit(c)) std::runtime_error("Invalid port: " + std::string(url));
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

    std::string caddr (addr_str);
    std::string cpath (path_str);

    connect_info.context = ctx.get_context();;
    connect_info.address = caddr.c_str();
    connect_info.port = port;
    connect_info.ssl_connection = use_ssl?LCCSCF_USE_SSL: 0;
    connect_info.path = cpath.c_str();
    connect_info.host = connect_info.address;
//    connect_info.origin = connect_info.address;
    connect_info.protocol = nullptr;
    connect_info.pwsi = &_wsi;
    connect_info.userdata = this;

    lws_client_connect_via_info(&connect_info);

}

void WebSocketClient::handle_errors() {
    if (_send_error) {
        throw std::runtime_error("WebSocket send error");
        _send_error = false;
    }
    if (_recv_error) {
        throw std::runtime_error("WebSocket receive error");
        _recv_error = false;
    }
}

void WebSocketClient::on_established() {
    std::lock_guard _(_mx);
    lws_callback_on_writable(_wsi);
    _send_pending = false;
    _connecting = false;
    if (_send_el) _send_el->signal(_send_el_id);
}

void WebSocketClient::on_receive(std::string_view data) {
    std::lock_guard _(_mx);
    MsgType type = lws_frame_is_binary(_wsi)?MsgType::binary:MsgType::text;
    RecvMessage msg = std::move(_recv_prealloc);
    msg.set_content(type, data);
    _recv_queue.push_back(std::move(msg));
    notify();
}

void WebSocketClient::on_error() {
    std::lock_guard _(_mx);
    _recv_error = true;
    _closed = true;
    notify();
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

void WebSocketClient::clear_on_receive() {
    std::lock_guard _(_mx);
    _recv_el = nullptr;
}

void WebSocketClient::clear_on_send() {
    std::lock_guard _(_mx);
    _send_el = nullptr;
}

void WebSocketClient::receive_sync(RecvMessage &msg) {
    if (!receive(msg)) {
        WSEventListener el;
        on_receive(el, 0);
        while (!receive(msg)) el.wait();
        clear_on_receive();
    }
}

void WebSocketClient::on_receive(WSEventListener &pl, WSEventListener::ClientID id) {
    std::lock_guard _(_mx);
    _recv_el = &pl;
    _recv_el_id = id;
}

void WebSocketClient::on_send(WSEventListener &pl, WSEventListener::ClientID id) {
    std::lock_guard _(_mx);
    _send_el = &pl;
    _send_el_id = id;
}

void WebSocketClient::push_close_frame() {
    RecvMessage msg;
    msg.set_content(MsgType::close, "");
    _recv_queue.push_back(msg);
    notify();
}

bool WebSocketClient::on_writable() {
    std::lock_guard _(_mx);
    _send_pending = false;
    if (_closing) {
        _closed = true;
        lws_set_wsi_user(_wsi, nullptr);
        push_close_frame();
        return false;
    }
    if (!_send_queue.empty()) {
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
        on_receive(el, 0);
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
    _closed = true;
    push_close_frame();
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
