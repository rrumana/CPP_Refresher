cmake_minimum_required(VERSION 3.24)
project(m03_locality CXX)
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

# AoS vs SoA microbench
add_executable(aos_soa_bench src/aos_soa_bench.cpp)
target_link_libraries(aos_soa_bench PRIVATE benchmark::benchmark)
target_compile_options(aos_soa_bench PRIVATE -O3 -march=native)
# Add a no-inline variant to keep optimizer honest during investigations
add_executable(aos_soa_bench_noinline src/aos_soa_bench.cpp)
target_link_libraries(aos_soa_bench_noinline PRIVATE benchmark::benchmark)
target_compile_options(aos_soa_bench_noinline PRIVATE -O3 -march=native -fno-inline)
target_compile_definitions(aos_soa_bench_noinline PRIVATE NOINLINE_BUILD=1)

# False sharing microbench (multithreaded)
add_executable(false_sharing_bench src/false_sharing_bench.cpp)
target_link_libraries(false_sharing_bench PRIVATE benchmark::benchmark pthread)
target_compile_options(false_sharing_bench PRIVATE -O3 -march=native)