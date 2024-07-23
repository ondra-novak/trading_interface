cmake_minimum_required(VERSION 3.5)


add_library (trading_api_common
    loader.cpp
    config_desc.cpp
    basic_order.cpp
    basic_context.cpp
    basic_exchange.cpp
    basic_log.cpp
    memory_storage.cpp
    context_scheduler.cpp
    )

add_dependencies(trading_api_common libjson20_single_header)
add_dependencies(trading_api_common trading_api_single_header)

