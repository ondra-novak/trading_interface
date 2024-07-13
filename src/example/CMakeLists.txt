cmake_minimum_required(VERSION 3.5)


add_library (example_strategy SHARED example_strategy.cpp)
set_target_properties(example_strategy PROPERTIES PREFIX "")
add_dependencies(example_strategy trading_api_single_header)

add_library (example_exchange SHARED example_exchange.cpp)
set_target_properties(example_exchange PROPERTIES PREFIX "")
add_dependencies(example_strategy trading_api_single_header)