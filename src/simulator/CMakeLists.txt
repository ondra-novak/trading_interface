cmake_minimum_required(VERSION 3.5)



add_library (simulator
     sim_exchange.cpp
    )

add_dependencies(trading_api_common libjson20_single_header)
add_dependencies(trading_api_common trading_api_single_header)

