cmake_minimum_required(VERSION 3.24)
project(m06_compilers CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings
add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(kernel_ir src/kernel_ir.cpp)
target_compile_options(kernel_ir PRIVATE -O3 -march=native)

add_executable(fastmath_demo src/fastmath_demo.cpp)
target_compile_options(fastmath_demo PRIVATE -O3 -march=native)