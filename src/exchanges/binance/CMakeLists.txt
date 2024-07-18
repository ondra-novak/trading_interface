cmake_minimum_required(VERSION 3.5)

add_executable (websocket_test
         websocket_client.cpp
         rpc_client.cpp
         ws_streams.cpp
         stream_test.cpp
         secure_rpc.cpp)
target_link_libraries(websocket_test websockets ssl crypto)

add_executable (secure_rpc_test
         websocket_client.cpp
         rpc_client.cpp
         ws_streams.cpp
         secure_rpc_test.cpp
         secure_rpc.cpp)
target_link_libraries(secure_rpc_test websockets ssl crypto)
