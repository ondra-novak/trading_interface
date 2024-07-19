#include "secure_rpc.h"



int main() {

    WebSocketContext ctx;
    HttpClientRequest client(ctx, HttpMethod::POST, "https://httpbin.org/post","Hello world",{{"Content-Type","text/plain"}});
    HttpClientRequest::Data data;
    while (client.read_body_sync(data)) {
        std::cout << std::string_view(data.begin(), data.end()) << std::endl;
    }


    std::cout << std::cin.get() << std::endl;




    SecureRPCClient::Account red (
            "",
            ""
    );



    SecureRPCClient rpc(ctx, "wss://ws-fapi.binance.com/ws-fapi/v1");
    rpc.run_thread_auto_reconnect(rpc);

    auto res = rpc(red,"account.status",json::type_t::object).get();
    std::cout << res.content.to_json() << std::endl;

}


