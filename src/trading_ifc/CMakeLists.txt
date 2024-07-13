cmake_minimum_required(VERSION 3.5)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

file(MAKE_DIRECTORY ${CMAKE_HEADER_OUTPUT_DIRECTORY}/trading_api)

set(OUTPUT "${CMAKE_HEADER_OUTPUT_DIRECTORY}/trading_api/strategy.h"
           "${CMAKE_HEADER_OUTPUT_DIRECTORY}/trading_api/exchange.h"
           "${CMAKE_HEADER_OUTPUT_DIRECTORY}/trading_api/module.h"
           )

add_custom_command(
    OUTPUT "${CMAKE_HEADER_OUTPUT_DIRECTORY}/trading_api/strategy.h"
    COMMAND to_single_header ARGS strategy_api.h ${CMAKE_HEADER_OUTPUT_DIRECTORY}/trading_api/strategy.h
    DEPENDS ${SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating single header"
    VERBATIM
)

add_custom_command(
    OUTPUT "${CMAKE_HEADER_OUTPUT_DIRECTORY}/trading_api/exchange.h"
    COMMAND to_single_header ARGS exchange_api.h ${CMAKE_HEADER_OUTPUT_DIRECTORY}/trading_api/exchange.h
    DEPENDS ${SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating single header"
    VERBATIM
)


add_custom_command(
    OUTPUT "${CMAKE_HEADER_OUTPUT_DIRECTORY}/trading_api/module.h"
    COMMAND ${CMAKE_COMMAND} ARGS -E copy module.h ${CMAKE_HEADER_OUTPUT_DIRECTORY}/trading_api/module.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/module.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Copying module.h"
    VERBATIM
)

add_custom_target(trading_api_single_header ALL DEPENDS ${OUTPUT})
add_dependencies(trading_api_single_header to_single_header)



