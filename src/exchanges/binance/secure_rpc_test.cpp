#include "secure_rpc.h"



int main() {

    SecureRPCClient::Account red (
            "",
            ""
    );

    WebSocketContext ctx;
    SecureRPCClient rpc(ctx, "wss://ws-fapi.binance.com/ws-fapi/v1");
    rpc.run_thread_auto_reconnect(rpc);

    auto res = rpc(red,"account.status",json::type_t::object).get();
    std::cout << res.content.to_json() << std::endl;

}


