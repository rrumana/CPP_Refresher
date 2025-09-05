# Lab M01 — Copy vs Move vs Emplace Microbenchmark

Purpose
- Re-activate C++ fluency and establish a repeatable measurement workflow.
- Quantify copy vs move vs in-place construction costs in std::vector with optimizer hygiene.

Why this lab matters at Lemurian Labs
- You will frequently justify micro-optimizations with evidence. This lab teaches the measurement doctrine used throughout the program: -O0 vs -O3 comparisons, no-inline variants, perf counters, and instruction attribution.

Repo layout
- Source: C++_Lecture/labs/m01_reactivation/src/bench_copy_move.cpp
- CMake: C++_Lecture/labs/m01_reactivation/CMakeLists.txt
- This README: C++_Lecture/labs/m01_reactivation/README.md

Prerequisites
- Linux with clang++ 17 or gcc 12+, cmake 3.24+, git
- perf and valgrind (callgrind)
- Internet access (CMake FetchContent downloads Google Benchmark)
- Recommended: taskset, numactl

Install examples (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y clang build-essential cmake git linux-tools-common linux-tools-generic valgrind
```

Build
```bash
# Configure (clang++) and export compile commands for tools
cmake -S C++_Lecture/labs/m01_reactivation -B build/m01 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DBENCHMARK_ENABLE_TESTING=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build optimized and no-inline variants
cmake --build build/m01 -j
```

Run
```bash
# Pin to a core to reduce noise
taskset -c 2 ./build/m01/bench_copy_move
taskset -c 2 ./build/m01/bench_copy_move_noinline
```

Profile (evidence)
```bash
# Hardware counters (5 repetitions)
taskset -c 2 perf stat -d -r 5 ./build/m01/bench_copy_move

# Instruction attribution
valgrind --tool=callgrind ./build/m01/bench_copy_move
callgrind_annotate --auto=yes --threshold=99 callgrind.out.* | less
```

Common VS Code include error and how to fix
- Symptom: cannot open source file "benchmark/benchmark.h"
- Cause: VS Code’s C/C++ intellisense does not know about FetchContent includes until it reads compile_commands.json
- Fix:
  1) Ensure you configured the build with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON (see configure command above)
  2) Point VS Code to the compilation database:
     - Option A: Settings JSON: "C_Cpp.default.compileCommands": "${workspaceFolder}/build/m01/compile_commands.json"
     - Option B: Symlink into repo root:
       ln -sf build/m01/compile_commands.json compile_commands.json
- Note: This is an editor-only warning. The project builds fine because CMake wires the include paths for the benchmark target.

What to record in your notes
- Median ns/op and variance for copy, move, and emplace at sizes 2^10 and 2^14
- Compare bench_copy_move vs bench_copy_move_noinline to ensure optimizer hygiene
- perf IPC and LLC-miss deltas between variants
- A small IR or asm snippet that shows copy vs move vs elided construction differences

Troubleshooting
- CMake cannot fetch Google Benchmark:
  - Ensure git is installed and network is available
  - Re-run cmake to let FetchContent download dependencies
- Link errors about pthread:
  - Google Benchmark needs pthread; CMake target handles it. If using non-default toolchains, ensure pthread is available
- perf permission errors:
  - You may need to adjust kernel.perf_event_paranoid:
    echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid

Stretch goals
- Add a third argument 1<<18 to stress cache effects
- Inspect IR:
  clang++ -O3 -S -emit-llvm -std=c++23 -I<benchmark-include-dir> src/bench_copy_move.cpp -o bench.ll
- Try gcc vs clang and compare codegen with Compiler Explorer for the hot loop body

Deliverables
- A one-page write-up with numbers, plots optional, and 2–3 short code or IR excerpts illustrating the observed differences