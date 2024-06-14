cmake_minimum_required(VERSION 3.1)

add_library(trading_ifc
	trading_ifc.cpp
)
add_dependencies(trading_ifc trading_ifc_version)

