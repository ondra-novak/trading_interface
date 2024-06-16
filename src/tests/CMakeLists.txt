cmake_minimum_required(VERSION 3.1)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/)
#file(GLOB testFiles "*.cpp")
set(testFiles 
	compile_test.cpp
	loader.cpp
)

link_libraries(
     trading_api_common
	${STANDARD_LIBRARIES}
)

foreach (testFile ${testFiles})
	string(REGEX MATCH "([^\/]+$)" filename ${testFile})
	string(REGEX MATCH "[^.]*" executable_name tests_${filename})
	add_executable(${executable_name} ${testFile})
	target_compile_definitions(${executable_name} PRIVATE
	   MODULE_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
	 )	
	add_test(NAME "tests/${filename}" COMMAND ${executable_name})
endforeach ()

