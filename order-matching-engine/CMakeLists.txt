cmake_minimum_required(VERSION 3.10)
project(OrderMatchingEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")
add_compile_options(-Wall -Wextra)

find_package(Threads REQUIRED)

include_directories(include)

add_library(matching_core src/OrderBook.cpp src/MatchingEngine.cpp)
target_link_libraries(matching_core PUBLIC Threads::Threads)

add_executable(matching_engine src/main.cpp)
target_link_libraries(matching_engine PRIVATE matching_core)

enable_testing()
add_executable(matching_tests tests/test_order_book.cpp)
target_link_libraries(matching_tests PRIVATE matching_core)
add_test(NAME matching_tests COMMAND matching_tests)

add_executable(matching_bench tools/bench.cpp)
target_link_libraries(matching_bench PRIVATE matching_core)
