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

std::atomic<bool> stopped = false;
void read_thread(WSStreams *strm) {
    while (!stopped) {
        auto reconnect_after = std::chrono::system_clock::now()+std::chrono::seconds(5);
        WSEventListener lsn;
        strm->on_response(lsn, 0);
        bool try_ping = true;
        while (strm->process_responses()) {
            if (!lsn.wait_for(std::chrono::seconds(10))) {
                if (try_ping) {
                    strm->send_ping();
                    try_ping = false;
                } else {
                    break;
                }
            }
            else {
                try_ping = true;
            }
        }
        if (stopped) break;
        std::cerr<<"WS Error:" << strm->get_last_error() << std::endl;
        std::this_thread::sleep_until(reconnect_after);
        if (stopped) break;
        strm->reconnect();
    }
}


int main() {
    WebSocketContext ctx;
    PrintEvents pev;
    WSEventListener lsn;

    WSStreams stream(pev,ctx,"wss://fstream.binance.com/ws");
    stream.subscribe(trading_api::SubscriptionType::orderbook,"BTCUSDT");
    std::thread thr(read_thread, &stream);
    std::cout << std::cin.get();
    stopped = true;
    stream.close();
    thr.join();
    return 0;
}
