cmake_minimum_required(VERSION 3.24)
project(m01_reactivation CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Conservative defaults; optimization flags per target below
add_compile_options(-Wall -Wextra -Wpedantic)

include(FetchContent)
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)
FetchContent_MakeAvailable(benchmark)

add_executable(bench_copy_move src/bench_copy_move.cpp)
target_link_libraries(bench_copy_move PRIVATE benchmark::benchmark)
# Strong optimization for perf runs
target_compile_options(bench_copy_move PRIVATE -O3 -march=native)
# Build a debug variant that disables inlining to keep compiler honest
add_executable(bench_copy_move_noinline src/bench_copy_move.cpp)
target_link_libraries(bench_copy_move_noinline PRIVATE benchmark::benchmark)
target_compile_options(bench_copy_move_noinline PRIVATE -O3 -march=native -fno-inline)
target_compile_definitions(bench_copy_move_noinline PRIVATE NOINLINE_BUILD=1)