#pragma once


#include "../../trading_ifc/orderbook.h"
#include "../../trading_ifc/ticker.h"
#include "rpc_client.h"

class WSStreams: public RPCClient {
public:

    class IEvents {
    public:

        virtual void on_ticker(std::string_view symbol, trading_api::Timestamp timestamp, const trading_api::Ticker &ticker) = 0;
        virtual void on_orderbook(std::string_view symbol, trading_api::Timestamp timestamp, const trading_api::OrderBook::Update &update) = 0;
        virtual ~IEvents() = default;
    };

    WSStreams(IEvents &events, WebSocketContext &ctx, std::string url);


    AsyncResult subscribe_ticker(std::string_view symbol);
    AsyncResult unsubscribe_ticker(std::string_view symbol);
    AsyncResult subscribe_orderbook(std::string_view symbol);
    AsyncResult unsubscribe_orderbook(std::string_view symbol);





protected:
    IEvents &_events;

    virtual bool on_json_message(const json::value&) override;

    using RPCClient::call;
    using RPCClient::operator();

    static std::string to_id(std::string_view symbol, std::string_view type);

};
