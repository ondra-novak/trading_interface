#include "rpc_client.h"

RPCClient::RPCClient(WebSocketContext &ctx, std::string url)
    :_client(ctx, url)
{
}

RPCClient::~RPCClient() {
    drop_all();
}

void RPCClient::close() {
    _client.close();
}

void RPCClient::clear_on_response() {
    _client.clear_on_receive();
}

void RPCClient::drop_all() {
    for (auto &[k, v]: _pending) {
        if (std::holds_alternative<CallbackType>(v)) {
            CallbackType &cb = std::get<CallbackType>(v);
            cb(Result{true, status_connection_lost, {}});
        } else if (std::holds_alternative<CoroHandle>(v)) {
            CoroHandle h = std::get<CoroHandle>(v);
            v.template emplace<Result>(Result{true, status_connection_lost, {}});
            h.resume();
        }
    }
    _pending.clear();
}

void RPCClient::create_request(WebSocketClient::SendMessage &msg,
        ID id, const std::string_view &method, const json::value &params) {
    json::value req = {
            {"id", std::to_string(id)},
            {"method", method},
            {"params", params}
    };
    json::serializer_t srl;
    msg.init(WebSocketClient::MsgType::text);
    auto iter = std::back_inserter(msg);
    srl.serialize(req, [&](std::string_view y){
        std::copy(y.begin(), y.end(), iter);
    });

}


RPCClient::AsyncResult RPCClient::call(std::string_view method, json::value params) {
    std::lock_guard _(_mx);
    ID id = _next_id++;
    create_request(_send_msg_cache, id, method, params);
    if (!_client.send(_send_msg_cache)) {
        _pending.emplace(id, Result{true, status_connection_lost, {}});
    } else {
        _pending.emplace(id, std::monostate{});
    }
    return AsyncResult(*this, id);
}

RPCClient::AsyncResult RPCClient::operator ()(std::string_view method, json::value params) {
    return call(method, std::move(params));
}

void RPCClient::on_response(WSEventListener &listener, WSEventListener::ClientID id) {
    _client.on_receive(listener, id);
}

bool RPCClient::process_responses() {
    while (!_client.receive(_recv_msg_cache)) {
        if (_recv_msg_cache.is_close()) {
            drop_all();
            return false;
        }

        json::value data = json::value::from_json(_recv_msg_cache);
        if (on_json_message(data)) continue;
        Result res;
        res.is_error = data["error"].defined();
        if (res.is_error) {
            res.content = data["error"].get();
        } else {
            res.content = data["result"].get();
        }
        res.status = data["status"].get();
        std::string_view idstr = data["id"].get();
        ID id = std::strtoull(idstr.data(), nullptr, 10);

        std::unique_lock lk(_mx);
        auto iter = _pending.find(id);
        if (iter != _pending.end()) {
            PendingRequest &req = iter->second;
            if (std::holds_alternative<CallbackType>(req)) {
                CallbackType &cb = std::get<CallbackType>(req);
                lk.unlock();
                cb(std::move(res));
                lk.lock();
                _pending.erase(iter);
            }
            else if (std::holds_alternative<CoroHandle>(req)) {
                CoroHandle h = std::get<CoroHandle>(req);
                req.template emplace<Result>(std::move(res));
                lk.unlock();
                h.resume();
                lk.lock();
               _pending.erase(iter);
            } else {
                req.template emplace<Result>(std::move(res));
            }
        }
    }

    return true;

}


void RPCClient::attach_callback(ID id, CallbackType cb) {
    std::unique_lock lk(_mx);
    auto iter = _pending.find(id);
    if (iter == _pending.end()) {
        return;
    }
    PendingRequest &req = iter->second;
    if (std::holds_alternative<Result>(req)) {
        Result &res = std::get<Result>(req);
        lk.unlock();
        cb(std::move(res));
        lk.lock();
        _pending.erase(iter);
    } else {
        req.template emplace<CallbackType>(std::move(cb));
    }
}

RPCClient::Result RPCClient::wait(ID id) {
    std::mutex mx;
    std::condition_variable cond;
    bool received = false;
    Result result;
    attach_callback(id,[&](Result res){
        std::lock_guard _(mx);
        result = std::move(res);
        received = true;
        cond.notify_all();
    });

    std::unique_lock lk(mx);
    while (!received) cond.wait(lk);
    return result;
}

RPCClient::Result RPCClient::get_no_wait(ID id) {
    std::unique_lock lk(_mx);
    auto iter = _pending.find(id);
    if (iter == _pending.end()) throw std::logic_error("n/a");
    return std::get<Result>(iter->second);
}

bool RPCClient::attach_coro(ID id, CoroHandle h) {
    std::unique_lock lk(_mx);
    auto iter = _pending.find(id);
    if (iter == _pending.end()) {
        return false;
    }
    PendingRequest &req = iter->second;
    if (std::holds_alternative<Result>(req)) {
        return false;
    } else {
        req.template emplace<CoroHandle>(h);
        return true;
    }
}

bool RPCClient::is_ready(ID id) {
    std::unique_lock lk(_mx);
    auto iter = _pending.find(id);
    return iter == _pending.end() || std::holds_alternative<Result>(iter->second);
}
