#include "rest_client.h"
#include "../../common/basic_log.h"


int main() {

    trading_api::Log log(std::make_shared<trading_api::BasicLog>(std::cerr, trading_api::ILog::Serverity::trace));
    WebSocketContext wsctx;
    RestClientContext restctx(wsctx,log);

    PIdentity ident = Identity::create({
        "3cfa2991082a67c0d3e20318b172d6badc07c1169ace1d83dae410b43b34f8d5",
        "13795a26cf2e407347d9a3a3c3283122f63cfe8dd3f214708e80c4cf002845bc"
    });

    RestClient client(restctx, "https://testnet.binancefuture.com/fapi", 10000);

    auto print_result = [](const RestClient::Result &res){
        std::cout << res.is_error() << std::endl;
        std::cout << res.content.to_json() << std::endl;
    };

    client.public_call("/v1/depth", {
            {"symbol","BTCUSDT"},
            {"limit", 5}
    }, print_result);
    client.signed_call(*ident, HttpMethod::GET, "/v2/account", {}, print_result);


    std::cout << "Press enter to exit" << std::endl << std::cin.get();



}


