# Lab M05 — Concurrency and Parallelism

Purpose
- Implement and measure an SPSC ring buffer with correct acquire/release happens-before edges.
- Demonstrate contention collapse on a shared atomic counter and fix it with sharded, cache-line–padded counters.
- Practice repeatable concurrency measurements (pinning, counters, flamegraphs) and correctness checks (TSan).

Repo layout
- SPSC ring buffer bench: [src/spsc_ring_bench.cpp](C++_Lecture/labs/m05_concurrency/src/spsc_ring_bench.cpp)
- Contention demo: [src/counters_contention.cpp](C++_Lecture/labs/m05_concurrency/src/counters_contention.cpp)
- CMake: [CMakeLists.txt](C++_Lecture/labs/m05_concurrency/CMakeLists.txt)
- This README: [README.md](C++_Lecture/labs/m05_concurrency/README.md)

Prerequisites
- Linux with clang++ 17 or gcc 12+, cmake 3.24+
- perf, valgrind (optional), ThreadSanitizer (clang recommended)
- Internet (CMake FetchContent downloads Google Benchmark)

Install examples (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y clang build-essential cmake git linux-tools-common linux-tools-generic
```

Build
```bash
# Configure and export compile commands for tools (VS Code, IWYU, etc.)
cmake -S C++_Lecture/labs/m05_concurrency -B build/m05 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DBENCHMARK_ENABLE_TESTING=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build all targets
cmake --build build/m05 -j
```

Run — SPSC ring buffer
```bash
# Pin producer and consumer to reduce scheduler noise (adjust cores as needed)
taskset -c 2-3 ./build/m05/spsc_ring_bench --benchmark_min_time=2.0
```
What to observe
- Items/sec counter and total runtime. The label spsc_ring_release_acquire indicates the correct acquire/release protocol.

Run — Contention demo
```bash
# Compare shared vs sharded counters across threads
taskset -c 2-9 ./build/m05/counters_contention --benchmark_min_time=2.0 --benchmark_counters_tabular=true
```
What to observe
- shared_atomic_fetch_add scaling collapses as threads grow.
- sharded_padded_counters scales closer to linear, limited by reduction cost and memory bandwidth.

Evidence
```bash
# Hardware counters (5 repeats)
taskset -c 2-3 perf stat -d -r 5 ./build/m05/spsc_ring_bench
taskset -c 2-9 perf stat -d -r 5 ./build/m05/counters_contention
```

Optional: TSan correctness proof
- The SPSC benchmark includes a deliberately incorrect relaxed variant, labeled spsc_ring_relaxed_bug (within the same source). Build and run a TSan build to show the race:
```bash
cmake -S C++_Lecture/labs/m05_concurrency -B build/m05_tsan \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER=clang++ \
  -DBENCHMARK_ENABLE_TESTING=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread"
cmake --build build/m05_tsan -j
./build/m05_tsan/spsc_ring_bench  # TSan should report a race on the relaxed variant
```

Flamegraphs (optional)
- Record and generate a flamegraph to spot time in spinning vs work:
```bash
taskset -c 2-3 perf record -g ./build/m05/spsc_ring_bench
perf script | stackcollapse-perf.pl | flamegraph.pl > spsc_ring.svg
```

VS Code include error and fix
- Symptom: cannot open source file "benchmark/benchmark.h"
- Cause: the editor does not know CMake-generated include paths until it reads compile_commands.json
- Fix:
  1) Ensure configure used -DCMAKE_EXPORT_COMPILE_COMMANDS=ON (see above)
  2) Point VS Code to the database:
     - Settings JSON: "C_Cpp.default.compileCommands": "${workspaceFolder}/build/m05/compile_commands.json"
     - Or symlink at repo root: ln -sf build/m05/compile_commands.json compile_commands.json

Deliverables
- Items/sec and perf counters for the SPSC benchmark; short HB justification (which store/load pairs carry release/acquire).
- A scaling table (2/4/8 threads) for shared vs sharded counters, with a one-paragraph explanation tying LLC-store-misses and IPC to the results.

Troubleshooting
- Google Benchmark fetch fails: check git and network; re-run cmake
- perf permission error: echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid
- Inconsistent SPSC throughput: ensure pinning, close background workloads, and consider increasing ring capacity (CapacityPow2) for backpressure scenarios.