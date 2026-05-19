cmake_minimum_required(VERSION 3.20)
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

add_executable(matching_engine src/main.cpp)