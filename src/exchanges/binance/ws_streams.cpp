#include "ws_streams.h"

WSStreams::WSStreams(IEvents &events, WebSocketContext &ctx, std::string url)
:RPCClient(ctx, std::move(url)),_events(events)
{
}

WSStreams::AsyncResult WSStreams::subscribe_ticker(std::string_view symbol) {
    return call("SUBSCRIBE", {to_id(symbol,"bookTicker"), to_id(symbol, "aggTrade")});
}

WSStreams::AsyncResult WSStreams::unsubscribe_ticker(std::string_view symbol) {
    return call("UNSUBSCRIBE", {to_id(symbol,"bookTicker"), to_id(symbol, "aggTrade")});

}

WSStreams::AsyncResult WSStreams::subscribe_orderbook(std::string_view symbol) {
    std::string id(symbol);
    id.append("@depth");
    return call("SUBSCRIBE", {to_id(symbol,"depth")});
}

WSStreams::AsyncResult WSStreams::unsubscribe_orderbook(
        std::string_view symbol) {
    std::string id(symbol);
    id.append("@depth");
    return call("UNSUBSCRIBE", {to_id(symbol,"depth")});
}

std::string WSStreams::to_id(std::string_view symbol, std::string_view type) {
    std::string s;
    s.append(symbol);
    std::transform(s.begin(), s.end(), s.begin(), [](char c){return std::tolower(c);});
    s.append("@");
    s.append(type);
    return s;
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
    } else {
        return false;
    }

}
