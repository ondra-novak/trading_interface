#include "ws_streams.h"

WSStreams::WSStreams(IEvents &events, WebSocketContext &ctx, std::string url)
:RPCClient(ctx, std::move(url)),_events(events)
{
    _subclass_cb = [this](const auto &x){return on_json_message(x);};
}

static std::string create_topic(std::string_view symbol, std::string_view type) {
    std::string s;
    s.append(symbol);
    std::transform(s.begin(), s.end(), s.begin(), [](char c){return std::tolower(c);});
    s.append("@");
    s.append(type);
    return s;
}


void WSStreams::subscribe(SubscriptionType type, std::string_view symbol) {
    manage_subscription([this](auto &&a, auto &&b){
        subscribe(std::move(a), std::move(b));
    },type, symbol);
}

template<typename Fn>
void WSStreams::manage_subscription(Fn &&fn, SubscriptionType type, std::string_view symbol) {
    std::unique_lock lk(_mx);
    switch (type) {
        case SubscriptionType::orderbook:
            fn(std::move(lk), create_topic(symbol, "depth"));
            break;
        case SubscriptionType::ticker:
            fn(std::move(lk), create_topic(symbol, "bookTicker"));
            fn(std::move(lk), create_topic(symbol, "aggTrade"));
            break;
    }

}

void WSStreams::subscribe(std::unique_lock<std::mutex> &&lk,std::string topic) {
    if (_subscrlist.insert(topic).second) {
        auto id = call_lk("SUBSCRIBE", {topic});
        attach_callback(lk, id, [this, topic](const Result &res) {
            if (res.is_error) {
                std::lock_guard _(_mx);
                if (res.status != RPCClient::status_connection_lost) {
                    _subscrlist.erase(topic);
                }
                _events.on_stream_error(res);
            }
        });
    }
}
void WSStreams::unsubscribe(SubscriptionType type, std::string_view symbol) {
    manage_subscription([this](auto &&a, auto &&b){
        unsubscribe(std::move(a), std::move(b));
    },type, symbol);
}
void WSStreams::unsubscribe(std::unique_lock<std::mutex> &&lk, std::string topic)
{
    auto iter = _subscrlist.find(topic);
    if (iter != _subscrlist.end()) {
        _subscrlist.erase(iter);
        auto id =call_lk("UNSUBSCRIBE", {topic});
        attach_callback(lk, id, [](const Result &) {});
    }
}

WSStreams::InstrumentState &WSStreams::get_instrument(const std::string_view id) {
        auto iter = _instrument_states.find(id);
        if (iter == _instrument_states.end()) {
            iter = _instrument_states.emplace(std::string(id), InstrumentState{}).first;
        }
        InstrumentState &st = iter->second;
        return st;

}

bool WSStreams::on_json_message(const json::value& v) {
    auto e = v["e"].as<std::string_view>();
    if (e == "bookTicker") {

        trading_api::Timestamp ts =
                trading_api::Timestamp(std::chrono::milliseconds(v["E"].as<std::uint64_t>()));
        std::string_view s = v["s"].as<std::string_view>();

        InstrumentState &st = get_instrument(s);

        st.ticker.ask = v["a"].get();
        st.ticker.ask_volume = v["A"].get();
        st.ticker.bid = v["b"].get();
        st.ticker.bid_volume = v["B"].get();
        st.ticker.last = 0;
        st.ticker.volume = 0;
        st.ticker.tp = ts;

        _events.on_ticker(s, st.ticker);
        return true;
    } else if (e == "aggTrade") {
        trading_api::Timestamp ts =
                trading_api::Timestamp(std::chrono::milliseconds(v["T"].as<std::uint64_t>()));
        std::string_view s = v["s"].as<std::string_view>();
        InstrumentState &st = get_instrument(s);
        st.ticker.tp = ts;
        st.ticker.last = v["p"].get();
        st.ticker.volume = v["q"].get();
        _events.on_ticker(s, st.ticker);
        return true;
    } else if (e == "depthUpdate") {

        trading_api::Timestamp ts =
                trading_api::Timestamp(std::chrono::milliseconds(v["T"].as<std::uint64_t>()));

        std::string_view s = v["s"].as<std::string_view>();

        auto iter = _instrument_states.find(s);
        if (iter == _instrument_states.end()) {
            iter = _instrument_states.emplace(std::string(s), InstrumentState{}).first;
        }
        InstrumentState &st = iter->second;

        st.orderbook.set_timestamp(ts);

        for (const json::value &a: v["a"]) {
            st.orderbook.update_ask(a[0].get(), a[1].get());
        }
        for (const json::value &b: v["b"]) {
            st.orderbook.update_bid(b[0].get(), b[1].get());
        }
        _events.on_orderbook(s, st.orderbook);
        return true;
    } else if (e == "ORDER_TRADE_UPDATE") {
        _events.on_order(v);
        return true;
    } else {
        return false;
    }

}

void WSStreams::reconnect() {
    reconnect(std::unique_lock(_mx));
}


void WSStreams::reconnect(std::unique_lock<std::mutex> &&lk) {
    _instrument_states.clear();
    auto lst = std::move(_subscrlist);
    RPCClient::reconnect(std::move(lk));
    lk.lock();
    json::value args(lst.begin(), lst.end());
    auto id = call_lk("SUBSCRIBE", json::value(lst.begin(), lst.end()));
    attach_callback(lk, id, [this](const Result &res) {
        if (res.is_error) {
            this->close();
        }
    });
}
