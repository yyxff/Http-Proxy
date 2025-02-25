cmake_minimum_required(VERSION 3.10)
project(proxy)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# proxy library for tests
add_library(proxy_lib
    Proxy.cpp
    Request.cpp
    Logger.cpp
    Parser.cpp
)

# Add main executable
add_executable(proxy 
    main.cpp
    Proxy.cpp
    Cache.cpp
    Request.cpp
    Logger.cpp
    Parser.cpp
)

target_link_libraries(proxy pthread)
target_include_directories(proxy_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(proxy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


