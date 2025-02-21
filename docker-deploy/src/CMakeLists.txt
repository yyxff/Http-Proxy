cmake_minimum_required(VERSION 3.10)
project(proxy)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(proxy 
    main.cpp
    proxy.cpp
    cache.cpp
    request.cpp
    Logger.cpp
)

target_link_libraries(proxy pthread)


