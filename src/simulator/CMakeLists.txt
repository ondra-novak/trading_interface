cmake_minimum_required(VERSION 3.5)



add_library (simulator
    simulator.cpp
    simulator_account.cpp
    simulator_instrument.cpp
    )

add_dependencies(trading_api_common libjson20_single_header)
add_dependencies(trading_api_common trading_api_single_header)

