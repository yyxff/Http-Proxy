cmake_minimum_required(VERSION 3.10)
project(proxy)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# proxy library for tests
add_library(proxy_lib
    Proxy.cpp
    Request.cpp
    Response.cpp
    Logger.cpp
    Parser.cpp
    Cache.cpp
    CacheEntry.cpp
    CacheDecision.cpp
    CacheHandler.cpp
)

# Add main executable
add_executable(proxy 
    main.cpp
)

target_link_libraries(proxy PRIVATE proxy_lib pthread)
target_include_directories(proxy_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(proxy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


