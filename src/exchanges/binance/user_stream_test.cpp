#include "ws_streams.h"
#include "rest_client.h"
#include "../../common/basic_log.h"

class PrintEvents: public WSStreams::IEvents {
public:
    virtual void on_ticker(std::string_view symbol,  const trading_api::TickData &ticker) override {
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
    trading_api::Log log(std::make_shared<trading_api::BasicLog>(std::cerr, trading_api::ILog::Serverity::trace));
    WebSocketContext wsctx;
    RestClientContext restctx(wsctx,log);

    PIdentity ident = Identity::create({
        "3cfa2991082a67c0d3e20318b172d6badc07c1169ace1d83dae410b43b34f8d5",
        "13795a26cf2e407347d9a3a3c3283122f63cfe8dd3f214708e80c4cf002845bc"
    });

    RestClient client(restctx, "https://testnet.binancefuture.com/fapi", 10000);

    client.signed_call(*ident, HttpMethod::POST, "/v1/listenKey", {}, [&](RestClient::Result res){
        std::cout << res.content.to_json() << std::endl;
    });
#if 0
    WebSocketContext ctx;
    PrintEvents pev;
    WSEventListener lsn;

    WSStreams stream(pev,ctx,"wss://fstream.binance.com/ws");
    stream.subscribe(trading_api::SubscriptionType::orderbook,"BTCUSDT");
    stream.run_thread_auto_reconnect(stream, 10, nullptr);
    std::cout << std::cin.get();
    stream.close();
#endif 
    return 0;
}
