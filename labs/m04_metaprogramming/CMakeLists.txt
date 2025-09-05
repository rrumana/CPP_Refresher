cmake_minimum_required(VERSION 3.24)
project(m04_metaprogramming CXX)
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

# CRTP vs Virtual microbench
add_executable(crtp_vs_virtual_bench src/crtp_vs_virtual_bench.cpp)
target_link_libraries(crtp_vs_virtual_bench PRIVATE benchmark::benchmark pthread)
target_compile_options(crtp_vs_virtual_bench PRIVATE -O3 -march=native)

# Concepts + constexpr dispatch microbench
add_executable(concept_dispatch_bench src/concept_dispatch_bench.cpp)
target_link_libraries(concept_dispatch_bench PRIVATE benchmark::benchmark pthread)
target_compile_options(concept_dispatch_bench PRIVATE -O3 -march=native)

# Optional: inline and vectorization diagnostics for Clang (uncomment to use)
# target_compile_options(crtp_vs_virtual_bench PRIVATE -Rpass=inline -Rpass-missed=inline)
# target_compile_options(concept_dispatch_bench PRIVATE -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize)