cmake_minimum_required(VERSION 3.24)
project(m02_memory_ub CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# ------------------------------------------------------------------
# Google Benchmark (FetchContent)
# ------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)
FetchContent_MakeAvailable(benchmark)

# ------------------------------------------------------------------
# Aliasing microbenchmarks (strict vs no-strict-aliasing)
# ------------------------------------------------------------------
add_executable(aliasing_bench_strict src/aliasing_bench.cpp)
target_link_libraries(aliasing_bench_strict PRIVATE benchmark::benchmark)
target_compile_options(aliasing_bench_strict PRIVATE -O3 -march=native)
target_compile_definitions(aliasing_bench_strict PRIVATE ALIASING_STRICT=1)

add_executable(aliasing_bench_nostrict src/aliasing_bench.cpp)
target_link_libraries(aliasing_bench_nostrict PRIVATE benchmark::benchmark)
target_compile_options(aliasing_bench_nostrict PRIVATE -O3 -march=native -fno-strict-aliasing)
target_compile_definitions(aliasing_bench_nostrict PRIVATE ALIASING_NOSTRICT=1)

# ------------------------------------------------------------------
# ThreadSanitizer atomic memory order demos
# ------------------------------------------------------------------
add_executable(atomic_relaxed_tsan src/atomic_order_demo.cpp)
target_compile_options(atomic_relaxed_tsan PRIVATE -O2 -g -fsanitize=thread)
target_link_options(atomic_relaxed_tsan PRIVATE -fsanitize=thread)
target_compile_definitions(atomic_relaxed_tsan PRIVATE ORDER_RELAXED=1)

add_executable(atomic_acqrel_tsan src/atomic_order_demo.cpp)
target_compile_options(atomic_acqrel_tsan PRIVATE -O2 -g -fsanitize=thread)
target_link_options(atomic_acqrel_tsan PRIVATE -fsanitize=thread)
target_compile_definitions(atomic_acqrel_tsan PRIVATE ORDER_ACQREL=1)