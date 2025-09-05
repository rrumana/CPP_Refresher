# Lab M03 — Caching, Locality, and Core Allocation

Purpose
- Build intuition for memory-bound behavior: cache lines, associativity, and access order dominate kernel performance.
- Show how data layout (AoS vs SoA) and blocking improve locality and vectorization.
- Demonstrate false sharing and its mitigation using padding and per-thread sharding.
- Practice measurement discipline with perf, cachegrind, and vectorization reports.

Why this lab matters at Lemurian Labs
- Many ML kernels are bandwidth or latency bound. Engineers must reason from cache models to code structure and counters, and articulate evidence-driven improvements.

Repo layout
- AoS vs SoA microbench: [src/aos_soa_bench.cpp](C++_Lecture/labs/m03_locality/src/aos_soa_bench.cpp)
- False sharing microbench: [src/false_sharing_bench.cpp](C++_Lecture/labs/m03_locality/src/false_sharing_bench.cpp)
- CMake: [CMakeLists.txt](C++_Lecture/labs/m03_locality/CMakeLists.txt)
- This README: [README.md](C++_Lecture/labs/m03_locality/README.md)

Prerequisites
- Linux with clang++ 17 or gcc 12+, cmake 3.24+
- perf, valgrind (cachegrind, callgrind)
- Optional: numactl, taskset, lscpu
- Internet (CMake FetchContent downloads Google Benchmark)

Install examples (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y clang build-essential cmake git linux-tools-common linux-tools-generic valgrind numactl
```

Build
```bash
# Configure and export compile commands for tools (VS Code, IWYU, etc.)
cmake -S C++_Lecture/labs/m03_locality -B build/m03 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DBENCHMARK_ENABLE_TESTING=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build all targets
cmake --build build/m03 -j
```

Run — AoS vs SoA with blocking
```bash
# Baseline runs (stride-1 vs gather, no blocking vs blocked)
taskset -c 2 ./build/m03/aos_soa_bench --benchmark_min_time=2.0
taskset -c 2 ./build/m03/aos_soa_bench_noinline --benchmark_min_time=2.0
```
What to try
- Compare labels SoA_axpy vs AoS_axpy and the blocked variants (blk8k, blk32k).
- Add more block sizes by editing the Args list in aos_soa_bench.cpp and re-building.

Run — False sharing demo
```bash
# Compare shared-line vs padded slots at different thread counts
taskset -c 2-9 ./build/m03/false_sharing_bench --benchmark_min_time=2.0 --benchmark_counters_tabular=true
```
What to look for
- Throughput collapse in shared_line_slots as threads increase; near-linear scaling in padded_slots.
- Expect higher LLC-store-misses and store buffer stalls in the shared version.

Evidence
```bash
# Hardware counters (5 repeats)
taskset -c 2 perf stat -d -r 5 ./build/m03/aos_soa_bench
taskset -c 2-9 perf stat -d -r 5 ./build/m03/false_sharing_bench

# Cache modeling
valgrind --tool=cachegrind ./build/m03/aos_soa_bench
cg_annotate cachegrind.out.* | less

# Instruction attribution
valgrind --tool=callgrind ./build/m03/aos_soa_bench
callgrind_annotate --auto=yes --threshold=99 callgrind.out.* | less
```

Vectorization reports (optional rebuild)
- Clang:
  - Add: -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize to target_compile_options for aos_soa_bench
- GCC:
  - Add: -fopt-info-vec-optimized -fopt-info-vec-missed to target_compile_options

NUMA and pinning (optional)
- Pin to isolate noise: taskset -c 2 …
- Control allocation: numactl --localalloc ./binary
- Inspect topology: lscpu, numastat

VS Code include error and fix
- Symptom: cannot open source file "benchmark/benchmark.h"
- Cause: the editor does not know generated include paths until it reads compile_commands.json
- Fix:
  1) Ensure CMake used -DCMAKE_EXPORT_COMPILE_COMMANDS=ON (see configure above)
  2) Point VS Code to the database:
     - Settings JSON: "C_Cpp.default.compileCommands": "${workspaceFolder}/build/m03/compile_commands.json"
     - Or symlink at repo root: ln -sf build/m03/compile_commands.json compile_commands.json

Deliverables
- Table with ns/op for AoS vs SoA across block sizes; indicate the best block for your machine and the expected cache target (L1/L2).
- perf and cachegrind snippets correlating wins to fewer LLC-load-misses and better IPC.
- False sharing scaling chart and a one-paragraph explanation of the coherence mechanism and the padding fix.

Troubleshooting
- Google Benchmark fetch fails: ensure git/network; re-run cmake
- perf permission error: echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid
- No vectorization: check aliasing assumptions; in SoA kernel, pointers are marked __restrict to communicate non-aliasing (uphold this contract).