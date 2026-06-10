cmake_minimum_required(VERSION 3.28)
project(matching_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

add_compile_options(-O3 -march=native)
add_executable(matching_engine src/main.cpp)
target_compile_options(matching_engine PRIVATE -Wall -Wextra -Wpedantic -Werror)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(matching_engine_tests tests/test_order_book.cpp)
target_include_directories(matching_engine_tests PRIVATE src)
target_link_libraries(matching_engine_tests GTest::gtest_main)
target_compile_options(matching_engine_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

include(GoogleTest)
gtest_discover_tests(matching_engine_tests)

FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.5.tar.gz
)
set(BENCHMARK_ENABLE_TESTING OFF)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(bench benchmarks/bench_order_book.cpp)
target_include_directories(bench PRIVATE src)
target_link_libraries(bench benchmark::benchmark)
target_compile_options(bench PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(bench_asan benchmarks/bench_order_book.cpp)
target_include_directories(bench_asan PRIVATE src)
target_link_libraries(bench_asan benchmark::benchmark)
target_compile_options(bench_asan PRIVATE -g -O0 -fsanitize=address -Wall -Wextra)
target_link_options(bench_asan PRIVATE -fsanitize=address)

add_executable(tests_asan tests/test_order_book.cpp)
target_include_directories(tests_asan PRIVATE src)
target_link_libraries(tests_asan GTest::gtest_main)
target_compile_options(tests_asan PRIVATE -g -O1 -fsanitize=address -Wall -Wextra)
target_link_options(tests_asan PRIVATE -fsanitize=address)

add_executable(bench_spsc benchmarks/bench_spsc.cpp)
target_include_directories(bench_spsc PRIVATE src)
target_link_libraries(bench_spsc benchmark::benchmark Threads::Threads)
target_compile_options(bench_spsc PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(tests_tsan tests/test_spsc_concurrent.cpp)
target_include_directories(tests_tsan PRIVATE src)
target_link_libraries(tests_tsan GTest::gtest_main Threads::Threads)
target_compile_options(tests_tsan PRIVATE -g -O1 -fsanitize=thread -Wall -Wextra)
target_link_options(tests_tsan PRIVATE -fsanitize=thread)