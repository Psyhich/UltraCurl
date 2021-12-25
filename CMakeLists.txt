cmake_minimum_required(VERSION 3.21)

project(ultra_curl)

# Command to create symlink for clangd to see all includes
if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E create_symlink
			${CMAKE_BINARY_DIR}/compile_commands.json
			${CMAKE_SOURCE_DIR}/compile_commands.json
	)
endif()

# GoogleTest requires at least C++11
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES 
	include/http_downloader.h src/http_downloader.cpp
	include/sockets.h src/sockets.cpp 
	include/uri.h src/uri.cpp
	include/response.h src/response.cpp
)
set(CLI_BUILD 
	src/main.cpp
	include/cli_helper.h src/cli_helper.cpp
)
set(TESTS tests/uri_tests.cpp)

# Setting flags for coverage test
set(CMAKE_CXX_FLAGS_DEBUG " -g -O0 -fprofile-arcs -ftest-coverage ${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_DEBUG " -g -O0 -fprofile-arcs -ftest-coverage ${CMAKE_C_FLAGS_DEBUG}")
#option(CodeCoverage "CodeCoverage" OFF)

# Making all warnings errors and turning on pedantic feature in GCC
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

find_package(Threads REQUIRED)

# Adding GoogleTest from lib directory 
add_subdirectory(lib/googletest)
enable_testing()

add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC include/)

add_executable(ultra_curlcli ${CLI_BUILD})
target_link_libraries(ultra_curlcli ${PROJECT_NAME})


add_executable(tests ${TESTS})
target_link_libraries( tests
	gtest # For basic library usage
	gtest_main # For tests to be runned(this rung RUN_ALL_TESTS())
	rt # Needed for gtest
	pthread # Needed for gtest
	${PROJECT_NAME} # My test files
)
if(CMAKE_COMPILER_IS_GNUCXX)
	include(CodeCoverage)
	# Currenty I cannot make parameters more universal, so I have to use python regexp
	SETUP_TARGET_FOR_COVERAGE_COBERTURA(tests_coverage tests cobertura "\".*/(tests.cpp|main.cpp)\"")
endif()


include(GoogleTest)
#include(Dart)
gtest_discover_tests(tests)

# Automaticaly running all tests on build
add_custom_command(TARGET ultra_curlcli
					POST_BUILD
					COMMAND ctest -C $<CONFIGURATION> 
						--output-on-failure
						)
