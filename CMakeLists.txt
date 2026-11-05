cmake_minimum_required(VERSION 3.25)

project(EntornVGI LANGUAGES C CXX)

include(CTest)
add_subdirectory(EntornVGI)

if(BUILD_TESTING)
	add_subdirectory(tests)
endif()
