cmake_minimum_required(VERSION 3.10)
project(proxy_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add src directory
add_subdirectory(${CMAKE_SOURCE_DIR}/../src ${CMAKE_BINARY_DIR}/src)

# Find GTest package
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/../src)

# Create and set permissions for log directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/../var/log/erss)
execute_process(COMMAND chmod 777 ${CMAKE_BINARY_DIR}/../var/log/erss)

# Create test logs directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_logs)
execute_process(COMMAND chmod 777 ${CMAKE_BINARY_DIR}/test_logs)

# Add test executable
add_executable(proxy_test
    test_proxy.cpp
)

# Link libraries
target_link_libraries(proxy_test
    proxy_lib
    GTest::GTest
    GTest::Main
    pthread
    stdc++fs
)
