cmake_minimum_required(VERSION 3.5)



add_library (trading_api_common 
    loader.cpp
    config_desc.cpp
    )

    
add_dependencies(trading_api_common libjson20_single_header)