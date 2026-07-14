cmake_minimum_required(VERSION 3.10)
project(InMemoryKVStore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")
add_compile_options(-Wall -Wextra)

find_package(Threads REQUIRED)

include_directories(include)

add_library(kvstore_core src/KVStore.cpp src/Server.cpp)
target_link_libraries(kvstore_core PUBLIC Threads::Threads)

add_executable(kvstore_server src/main.cpp)
target_link_libraries(kvstore_server PRIVATE kvstore_core)

enable_testing()
add_executable(kvstore_tests tests/test_kvstore.cpp)
target_link_libraries(kvstore_tests PRIVATE kvstore_core)
add_test(NAME kvstore_tests COMMAND kvstore_tests)

add_executable(kvstore_bench tools/bench.cpp)
target_link_libraries(kvstore_bench PRIVATE kvstore_core)
