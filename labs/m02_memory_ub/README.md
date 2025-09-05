# Lab M02 — Memory Model, UB, Aliasing, and Lifetimes

Purpose
- Make aliasing and UB effects observable: compare unsafe type punning vs safe std::bit_cast and std::memcpy.
- Demonstrate acquire/release memory ordering vs relaxed using ThreadSanitizer.
- Produce IR and perf evidence to justify claims.

Why this lab matters at Lemurian Labs
- Runtime and kernel performance hinges on aliasing guarantees and correct synchronization. You must prove what the optimizer can assume (TBAA) and how fences create visibility.

Repo layout
- Aliasing bench: C++_Lecture/labs/m02_memory_ub/src/aliasing_bench.cpp
- Atomics demo: C++_Lecture/labs/m02_memory_ub/src/atomic_order_demo.cpp
- CMake: C++_Lecture/labs/m02_memory_ub/CMakeLists.txt
- This README: C++_Lecture/labs/m02_memory_ub/README.md

Prerequisites
- Linux with clang++ 17 or gcc 12+, cmake 3.24+
- perf, valgrind (callgrind)
- Optional: VS Code C/C++ extension for IntelliSense
- Internet (CMake FetchContent downloads Google Benchmark)

Install examples (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y clang build-essential cmake git linux-tools-common linux-tools-generic valgrind
```

Build
```bash
# Configure and export compile commands for tooling
cmake -S C++_Lecture/labs/m02_memory_ub -B build/m02 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DBENCHMARK_ENABLE_TESTING=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build all targets
cmake --build build/m02 -j
```

Run — Aliasing microbench
```bash
# Strict aliasing (default assumptions)
taskset -c 2 ./build/m02/aliasing_bench_strict

# No-strict-aliasing variant (turns off TBAA assumptions)
taskset -c 2 ./build/m02/aliasing_bench_nostrict
```

Evidence
```bash
# Hardware counters (5 repeats)
taskset -c 2 perf stat -d -r 5 ./build/m02/aliasing_bench_strict

# Instruction attribution
valgrind --tool=callgrind ./build/m02/aliasing_bench_strict
callgrind_annotate --auto=yes --threshold=99 callgrind.out.* | less

# IR with TBAA metadata (adjust include path if needed)
clang++ -O3 -S -emit-llvm -std=c++23 \
  -Ibuild/m02/_deps/benchmark-src/include \
  C++_Lecture/labs/m02_memory_ub/src/aliasing_bench.cpp -o /tmp/alias.ll
```
What to inspect in IR:
- Look for !tbaa metadata on loads/stores in the unsafe path vs memcpy/bit_cast variants.
- Compare vectorization decisions across strict vs no-strict binaries.

Run — Atomics memory order with TSan
```bash
# Relaxed (expect a data race reported by TSan)
./build/m02/atomic_relaxed_tsan

# Acquire/release (no race; happens-before established)
./build/m02/atomic_acqrel_tsan
```
Hints
- If TSan is missing, install a compiler with -fsanitize=thread support (clang typically includes it).
- Output includes TSan race reports only for the relaxed build.

Common VS Code include error and fix
- Symptom: cannot open source file "benchmark/benchmark.h"
- Cause: editor does not know CMake’s generated include paths until it reads compile_commands.json
- Fix:
  1) Ensure configure used -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  2) Point VS Code to the database:
     - Settings JSON: "C_Cpp.default.compileCommands": "${workspaceFolder}/build/m02/compile_commands.json"
     - Or symlink at repo root: ln -sf build/m02/compile_commands.json compile_commands.json

Deliverables (save your notes)
- ns/op and variance for unsafe punning vs memcpy vs bit_cast under strict and no-strict aliasing
- perf counters (IPC, LLC-misses) and 1–2 IR snippets showing TBAA differences
- TSan logs: race present under relaxed, absent under acquire/release
- A short narrative tying aliasing guarantees to optimizer behavior and correctness

Troubleshooting
- Google Benchmark fetch fails: check git and network; re-run CMake
- Link errors about pthread: ensure system has pthread; CMake target normally links it
- perf permission error: echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid