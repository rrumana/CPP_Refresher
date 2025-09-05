# Lab M04 — Templates, Constexpr, Concepts, and Static Polymorphism

Purpose
- Compare dynamic (virtual) vs static (CRTP) dispatch in hot loops; verify inlining and code size effects.
- Show compile-time tile selection (concepts + if constexpr) vs runtime-dispatched tiles; correlate to vectorization stability and performance.

Repo layout
- CRTP vs virtual bench: [`src/crtp_vs_virtual_bench.cpp`](C++_Lecture/labs/m04_metaprogramming/src/crtp_vs_virtual_bench.cpp)
- Concept + constexpr dispatch bench: [`src/concept_dispatch_bench.cpp`](C++_Lecture/labs/m04_metaprogramming/src/concept_dispatch_bench.cpp)
- CMake: [`CMakeLists.txt`](C++_Lecture/labs/m04_metaprogramming/CMakeLists.txt)
- This README: [`README.md`](C++_Lecture/labs/m04_metaprogramming/README.md)

Prerequisites
- Linux with clang++ 17 or gcc 12+, CMake 3.24+
- perf, binutils (size, nm, objdump), valgrind (optional)
- Internet (CMake FetchContent downloads Google Benchmark)

Install examples (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y clang build-essential cmake git linux-tools-common linux-tools-generic binutils valgrind
```

Build
```bash
# Configure and export compile commands for tools (VS Code, IWYU, etc.)
cmake -S C++_Lecture/labs/m04_metaprogramming -B build/m04 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DBENCHMARK_ENABLE_TESTING=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build all targets
cmake --build build/m04 -j
```

Run — CRTP vs virtual
```bash
# Hot-loop dispatch comparison
taskset -c 2 ./build/m04/crtp_vs_virtual_bench

# Hardware counters (5 repeats)
taskset -c 2 perf stat -d -r 5 ./build/m04/crtp_vs_virtual_bench

# Binary/code size review
size ./build/m04/crtp_vs_virtual_bench
nm -C --size-sort ./build/m04/crtp_vs_virtual_bench | tail -40
objdump -d ./build/m04/crtp_vs_virtual_bench | less
```
What to look for
- In CRTP path, expect the hot loop body inlined (no callq in asm) and higher IPC. In virtual path, a call per iteration often blocks vectorization; perf shows lower IPC.

Run — Concepts + constexpr dispatch vs runtime dispatch
```bash
# Tile sizes 8, 16, 32
taskset -c 2 ./build/m04/concept_dispatch_bench

# Hardware counters
taskset -c 2 perf stat -d -r 5 ./build/m04/concept_dispatch_bench
```
What to look for
- Compile-time dispatch (specialized TILE) yields stable vectorization; runtime tile variant may prevent vectorizer from assuming constant trip in the inner loop.

Optional diagnostics
- Clang inlining and vectorizer reports:
  - In CMake, add to the relevant targets:
    - -Rpass=inline -Rpass-missed=inline
    - -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize
- Compile-time profiling:
  - Add -ftime-trace to target_compile_options and inspect JSON in Chrome tracing.

VS Code include error and fix
- Symptom: cannot open source file "benchmark/benchmark.h"
- Cause: the editor does not know CMake-generated include paths until it reads compile_commands.json
- Fix:
  1) Ensure configure used -DCMAKE_EXPORT_COMPILE_COMMANDS=ON (see command above)
  2) Point VS Code to the database:
     - Settings JSON: "C_Cpp.default.compileCommands": "${workspaceFolder}/build/m04/compile_commands.json"
     - Or symlink at repo root: ln -sf build/m04/compile_commands.json compile_commands.json

Deliverables
- ns/op and IPC for CRTP vs virtual; show objdump snippet proving inlining in CRTP and a call in the virtual loop.
- ns/op and IPC for compile-time tiles vs runtime tiles; include vectorizer report notes or CE screenshots.
- Brief discussion of code size tradeoffs and when to keep dynamic dispatch at the boundaries.

Troubleshooting
- Google Benchmark fetch fails: check git/network; re-run cmake
- perf permission error: echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid
- Inlining still occurs in virtual path: the compiler can devirtualize when the dynamic type is visible; to keep the comparison clean, CRTP path should inline, and virtual path can be kept non-inlined by isolating the call in a separate TU (advanced) or trusting that the virtual call remains in the loop for this microbench.