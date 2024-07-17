#include "ws_streams.h"

class PrintEvents: public WSStreams::IEvents {
public:
    virtual void on_ticker(std::string_view symbol,  const trading_api::Ticker &ticker) override {
        std::cout << symbol << ":" << ticker << std::endl;
    }
    virtual void on_orderbook(std::string_view symbol,  const trading_api::OrderBook &update) override {
        //na
    }

};

int main() {
    WebSocketContext ctx;
    PrintEvents pev;
    WSEventListener lsn;

    WSStreams stream(pev,ctx,"wss://fstream.binance.com/ws");    
    stream.on_clear_to_send(lsn,1);
    lsn.wait();
    std::cout << "Connected" << std::endl;
    stream.clear_on_clear_to_send();
    stream.subscribe_ticker("BTCUSDT") >> [](RPCClient::Result res) {
        std::cout << "Subscribe response:" << (res.is_error?"error":"ok") << " " << res.status << " " << res.content.to_string() << std::endl;
    };
    stream.on_response(lsn,0);
    while (stream.process_responses()) {
        lsn.wait();
    }
    return 0;
}