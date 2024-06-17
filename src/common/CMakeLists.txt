cmake_minimum_required(VERSION 3.5)



add_library (trading_api_common 
    loader.cpp
    config_desc.cpp
    simul_context.cpp
    )

    
add_dependencies(trading_api_common libjson20_single_header)