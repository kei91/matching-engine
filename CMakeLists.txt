cmake_minimum_required(VERSION 3.28)
project(matching_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

add_compile_options(-O3 -march=native)

set(PROJECT_WARNINGS -Wall -Wextra -Wpedantic -Werror)

add_executable(matching_engine src/main.cpp)
target_compile_options(matching_engine PRIVATE ${PROJECT_WARNINGS})

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.5.tar.gz
)
set(BENCHMARK_ENABLE_TESTING OFF)
FetchContent_MakeAvailable(googlebenchmark)

enable_testing()
include(GoogleTest)

function(add_unit_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE src)
    target_link_libraries(${name} GTest::gtest_main ${ARGN})
    target_compile_options(${name} PRIVATE ${PROJECT_WARNINGS})
    gtest_discover_tests(${name})
endfunction()

function(add_bench name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE src)
    target_link_libraries(${name} benchmark::benchmark ${ARGN})
    target_compile_options(${name} PRIVATE ${PROJECT_WARNINGS})
endfunction()

# --- unit tests --------------------------------------------------------------
add_unit_test(matching_engine_tests tests/test_order_book.cpp)
add_unit_test(rdtsc_tests           tests/test_rdtsc.cpp)

# --- benchmarks --------------------------------------------------------------
add_bench(bench      benchmarks/bench_order_book.cpp)
add_bench(bench_spsc benchmarks/bench_spsc.cpp Threads::Threads)

# --- sanitizer targets -------------------------------------------------------
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

add_executable(tests_tsan tests/test_spsc_concurrent.cpp)
target_include_directories(tests_tsan PRIVATE src)
target_link_libraries(tests_tsan GTest::gtest_main Threads::Threads)
target_compile_options(tests_tsan PRIVATE -g -O1 -fsanitize=thread -Wall -Wextra)
target_link_options(tests_tsan PRIVATE -fsanitize=thread)
