cmake_minimum_required(VERSION 3.5)

project(max_sub_array LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BENCHMARK_ENABLE_TESTING FALSE)
include(FetchContent)
FetchContent_Declare(googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark
    GIT_TAG        main
)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(max_sub_array main.cpp)
target_link_libraries(max_sub_array benchmark::benchmark)
