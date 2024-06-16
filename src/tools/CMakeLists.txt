cmake_minimum_required(VERSION 3.5)

if(NOT TARGET to_single_header)

link_libraries(${STANDARD_LIBRARIES})

add_executable (to_single_header to_single_header.cpp)

endif()
