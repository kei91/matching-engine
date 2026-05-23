cmake_minimum_required(VERSION 3.28)
project(matching_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
    -O3 -march=native
    -Wall -Wextra
    -Wpedantic
    -Werror
)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
)
FetchContent_MakeAvailable(googletest)

add_executable(matching_engine src/main.cpp)

enable_testing()

add_executable(matching_engine_tests tests/test_order_book.cpp)

target_include_directories(matching_engine_tests PRIVATE src)
target_link_libraries(matching_engine_tests GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(matching_engine_tests)
