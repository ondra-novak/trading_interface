#pragma once


#include "../../trading_ifc/orderbook.h"
#include "../../trading_ifc/ticker.h"
#include "rpc_client.h"

#include <set>

class WSStreams: public RPCClient {
public:

    using SubscriptionType = trading_api::SubscriptionType;

    class IEvents {
    public:

        virtual void on_ticker(std::string_view symbol,  const trading_api::Ticker &ticker) = 0;
        virtual void on_orderbook(std::string_view symbol,  const trading_api::OrderBook &update) = 0;
        virtual void subscribe_result(std::string_view symbol, SubscriptionType type, const Result &result) = 0;
        virtual ~IEvents() = default;
    };

    WSStreams(IEvents &events, WebSocketContext &ctx, std::string url);


    void subscribe(SubscriptionType type, std::string_view symbol);
    void unsubscribe(SubscriptionType type, std::string_view symbol);

    void reconnect();


protected:
    IEvents &_events;

    bool on_json_message(const json::value&);

    using RPCClient::call;
    using RPCClient::operator();

    struct InstrumentState {
        trading_api::Ticker ticker = {};
        trading_api::OrderBook orderbook = {};
    };


    std::map<std::string, InstrumentState, std::less<> > _instrument_states;
    std::set<std::pair<SubscriptionType, std::string> > _subscrlist;


    InstrumentState &get_instrument(const std::string_view id);


    void reconnect(std::unique_lock<std::mutex> &&lk);
    void subscribe(std::unique_lock<std::mutex> &&lk,SubscriptionType type, std::string_view symbol);
    void unsubscribe(std::unique_lock<std::mutex> &&lk,SubscriptionType type, std::string_view symbol);

};
