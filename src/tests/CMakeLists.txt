

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/)
#file(GLOB testFiles "*.cpp")
set(testFiles 
	compile_test.cpp
	loader.cpp
	config_desc.cpp
	wandering_bst.cpp
)

link_libraries(
     trading_api_common
	${STANDARD_LIBRARIES}
)

foreach (testFile ${testFiles})
	string(REGEX MATCH "([^\/]+$)" filename ${testFile})
	string(REGEX MATCH "[^.]*" executable_name tests_${filename})
	add_executable(${executable_name} ${testFile})
	add_dependencies(${executable_name} trading_api_single_header)
	target_compile_definitions(${executable_name} PRIVATE
	   MODULE_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
	 )	
	add_test(NAME "tests/${filename}" COMMAND ${executable_name})
endforeach ()

