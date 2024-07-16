#include "ws_streams.h"

WSStreams::WSStreams(IEvents &events, WebSocketContext &ctx, std::string url)
:RPCClient(ctx, std::move(url)),_events(events)
{
}

WSStreams::AsyncResult WSStreams::subscribe_ticker(std::string_view symbol) {
    return call("SUBSCRIBE", {to_id(symbol,"bookTicker")});
}

WSStreams::AsyncResult WSStreams::unsubscribe_ticker(std::string_view symbol) {
    std::string id(symbol);
    id.append("@bookTicker");
    return call("UNSUBSCRIBE", {to_id(symbol,"bookTicker")});

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

bool WSStreams::on_json_message(const json::value& v) {
    auto e = v["e"].as<std::string_view>();
    if (e == "bookTicker") {

        trading_api::Timestamp ts =
                trading_api::Timestamp(std::chrono::milliseconds(v["E"].as<std::uint64_t>()));

        std::string_view s = v["s"].as<std::string_view>();

        _events.on_ticker(s,ts,
                trading_api::Ticker{
            v["b"].as<double>(),
            v["B"].as<double>(),
            v["a"].as<double>(),
            v["A"].as<double>()
        });

        return true;
    } else if (e == "depthUpdate") {

        trading_api::Timestamp ts =
                trading_api::Timestamp(std::chrono::milliseconds(v["E"].as<std::uint64_t>()));

        std::string_view s = v["s"].as<std::string_view>();

        for (const json::value &a: v["a"]) {
            _events.on_orderbook(s, ts, {trading_api::Side::sell, a[0].as<double>(), a[1].as<double>()});
        }
        for (const json::value &b: v["b"]) {
            _events.on_orderbook(s, ts, {trading_api::Side::buy, b[0].as<double>(), v[1].as<double>()});
        }
        return true;
    } else {
        return false;
    }

}
