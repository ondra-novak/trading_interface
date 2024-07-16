cmake_minimum_required(VERSION 3.5)

add_executable (websocket_test websocket_client.cpp)
target_link_libraries(websocket_test websockets)
