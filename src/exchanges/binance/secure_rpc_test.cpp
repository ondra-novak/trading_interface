#include "secure_rpc.h"



int main() {

    WebSocketContext ctx;
    HttpClientRequest client(ctx, "https://fapi.binance.com/fapi/v1/exchangeInfo");
    int status = client.get_status_sync();
    std::cerr << "status:" << status << std::endl;
    HttpClientRequest::Data data;
    client.read_body_sync(data, true);
    std::cout << std::string_view(data.begin(), data.end()) << std::endl;

/*
    RPCClient rpc(ctx, "wss://fapi.binance.com/fapi/fapi/v1/exchangeInfo");
    rpc.run_thread_auto_reconnect(rpc);
    auto res = rpc("exchangeInfo",{}).get();
    std::cout << res.content.to_json() << std::endl;

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
*/
}


