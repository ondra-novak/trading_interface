cmake_minimum_required(VERSION 3.5)

add_executable (websocket_test websocket_client.cpp rpc_client.cpp ws_streams.cpp stream_test.cpp)
target_link_libraries(websocket_test websockets)
