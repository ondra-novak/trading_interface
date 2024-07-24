cmake_minimum_required(VERSION 3.5)

add_executable (websocket_test
         websocket_client.cpp
         rpc_client.cpp
         ws_streams.cpp
         stream_test.cpp
         secure_rpc.cpp)
target_link_libraries(websocket_test ${LIBWEBSOCKETS_LIBRARIES} ${LIBSSL_LIBRARIES} ${STANDARD_LIBRARIES})

add_executable (secure_rpc_test
         websocket_client.cpp
         rpc_client.cpp
         ws_streams.cpp
         secure_rpc_test.cpp
         rest_client.cpp
         secure_rpc.cpp)
target_link_libraries(secure_rpc_test trading_api_common ${LIBWEBSOCKETS_LIBRARIES} ${LIBSSL_LIBRARIES} ${STANDARD_LIBRARIES})

add_library(binance SHARED
    websocket_client.cpp
    rpc_client.cpp
    ws_streams.cpp
    secure_rpc.cpp
    exchange_main.cpp
    instrument.cpp
    rest_client.cpp
    instrument_def_cache.cpp
    account.cpp
)
set_target_properties(binance PROPERTIES PREFIX "")


add_executable(test_exchange test_exchange.cpp)
target_link_libraries(test_exchange binance trading_api_common ${LIBWEBSOCKETS_LIBRARIES} ${LIBSSL_LIBRARIES} ${STANDARD_LIBRARIES})
