cmake_minimum_required(VERSION 3.5)


add_library (example_strategy SHARED example_strategy.cpp)
set_target_properties(example_strategy PROPERTIES PREFIX "")
