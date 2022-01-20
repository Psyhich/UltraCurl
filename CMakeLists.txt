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

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)
# GoogleTest requires at least C++11
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES 
	include/base_functionality.h src/base_functionality.cpp
	include/http_downloader.h
	include/downloader_pool.h
	include/uri.h src/uri.cpp
	include/response.h src/response.cpp
	include/sockets.h src/sockets.cpp 
)
set(CLI_BUILD 
	src/main.cpp
	include/cli_args_helper.h src/cli_args_helper.cpp
	include/cli_progres.h src/cli_progres.cpp
)
set(TESTS 
	tests/uri_tests.cpp
	tests/cli_helper_tests.cpp
	tests/response_tests.cpp
	tests/downloader_tests.cpp
	include/cli_args_helper.h src/cli_args_helper.cpp # adding files which need to be tested
	tests/test_socket.h tests/test_socket.cpp
	tests/fake_socket.h
	tests/pool_tests.cpp
)

# Setting flags for coverage test
set(CMAKE_CXX_FLAGS_DEBUG " -g -O0 -fprofile-arcs -ftest-coverage ${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_DEBUG " -g -O0 -fprofile-arcs -ftest-coverage ${CMAKE_C_FLAGS_DEBUG}")

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

add_subdirectory(lib/zstd/build/cmake)

add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_link_libraries(${PROJECT_NAME}
	pthread
	libzstd_static
)
target_include_directories(${PROJECT_NAME} PUBLIC include/)

add_executable(ultra_curlcli ${CLI_BUILD})
target_link_libraries(ultra_curlcli ${PROJECT_NAME})


add_executable(tests ${TESTS})
target_link_libraries( tests
	gtest # For basic library usage
	gmock
	gtest_main # For tests to be runned(this rung RUN_ALL_TESTS())
	rt # Needed for gtest
	pthread # Needed for gtest
	${PROJECT_NAME} # My test files
)
if(CMAKE_COMPILER_IS_GNUCXX)
	include(CodeCoverage)
	SETUP_TARGET_FOR_COVERAGE_COBERTURA(tests_coverage tests cobertura "\"(.*lib.*|.*tests.*|.*main.cpp)\"")
endif()


include(GoogleTest)
gtest_discover_tests(tests)

# Automaticaly running all tests on build
add_custom_command(TARGET ultra_curlcli
					POST_BUILD
					COMMAND ctest -C $<CONFIGURATION> 
						--output-on-failure
)
