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
         secure_rpc.cpp)
target_link_libraries(secure_rpc_test ${LIBWEBSOCKETS_LIBRARIES} ${LIBSSL_LIBRARIES} ${STANDARD_LIBRARIES})

add_library(binance SHARED 
    rpc_client.cpp 
    ws_streams.cpp
    secure_rpc.cpp
    exchange_main.cpp
    instrument.cpp
)
set_target_properties(binance PROPERTIES PREFIX "")  