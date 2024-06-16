set(CMAKE_HEADER_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/include)
include_directories(AFTER ${CMAKE_BINARY_DIR}/include)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/src/tools)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/src/trading_ifc)

