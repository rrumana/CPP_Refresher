cmake_minimum_required(VERSION 3.24)
project(m05_concurrency CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Google Benchmark via FetchContent
include(FetchContent)
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)
FetchContent_MakeAvailable(benchmark)

# SPSC ring buffer throughput
add_executable(spsc_ring_bench src/spsc_ring_bench.cpp)
target_link_libraries(spsc_ring_bench PRIVATE benchmark::benchmark pthread)
target_compile_options(spsc_ring_bench PRIVATE -O3 -march=native)

# Contended counters: shared vs sharded
add_executable(counters_contention src/counters_contention.cpp)
target_link_libraries(counters_contention PRIVATE benchmark::benchmark pthread)
target_compile_options(counters_contention PRIVATE -O3 -march=native)