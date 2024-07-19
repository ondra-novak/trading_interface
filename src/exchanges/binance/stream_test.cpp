#include "ws_streams.h"

class PrintEvents: public WSStreams::IEvents {
public:
    virtual void on_ticker(std::string_view symbol,  const trading_api::Ticker &ticker) override {
        std::cout << symbol << ":" << ticker << std::endl;
    }
    virtual void on_orderbook(std::string_view symbol,  const trading_api::OrderBook &update) override {
        std::cout << symbol << ":" << update << std::endl;
    }
    virtual void subscribe_result(std::string_view symbol, trading_api::SubscriptionType , const RPCClient::Result &res) override {
        std::cout << symbol << ":" << (res.is_error?"error":"ok") << " " << res.status << " " << res.content.to_string() << std::endl;
    }

};



int main() {
    WebSocketContext ctx;
    PrintEvents pev;
    WSEventListener lsn;

    WSStreams stream(pev,ctx,"wss://fstream.binance.com/ws");
    stream.subscribe(trading_api::SubscriptionType::orderbook,"BTCUSDT");
    stream.run_thread_auto_reconnect(stream, 10, nullptr);
    std::cout << std::cin.get();
    stream.close();
    return 0;
}
