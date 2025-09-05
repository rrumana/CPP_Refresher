#include <benchmark/benchmark.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#if defined(__clang__) || defined(__GNUC__)
  #define NOINLINE [[gnu::noinline]]
#elif defined(_MSC_VER)
  #define NOINLINE __declspec(noinline)
#else
  #define NOINLINE
#endif

#if defined(__cpp_lib_hardware_interference_size)
constexpr std::size_t CLS = std::hardware_destructive_interference_size;
#else
constexpr std::size_t CLS = 64;
#endif

// Shared atomic counter variant
static void BM_SharedAtomicCounter(benchmark::State& st) {
  const int threads = static_cast<int>(st.range(0));
  // Keep total increments roughly constant across thread counts
  const std::size_t total_increments = 64ull * 1024ull * 1024ull; // 64M
  const std::size_t iters_per_thread = total_increments / static_cast<std::size_t>(threads);

  for (auto _ : st) {
    std::atomic<std::uint64_t> shared{0};
    std::vector<std::thread> ts;
    ts.reserve(static_cast<std::size_t>(threads));
    for (int t = 0; t < threads; ++t) {
      ts.emplace_back([&shared, iters_per_thread]() {
        for (std::size_t i = 0; i < iters_per_thread; ++i) {
          // Single contended cache line
          shared.fetch_add(1, std::memory_order_relaxed);
        }
      });
    }
    for (auto& th : ts) th.join();
    benchmark::DoNotOptimize(shared.load(std::memory_order_relaxed));
    benchmark::ClobberMemory();
  }
  st.SetLabel("shared_atomic_fetch_add");
}
BENCHMARK(BM_SharedAtomicCounter)->Arg(2)->Arg(4)->Arg(8);

// Sharded padded counters to avoid false sharing
struct alignas(CLS) PaddedCounter {
  std::uint64_t v{0};
  // Ensure size is a multiple of CLS (for many platforms this is already true)
  char pad[(CLS > sizeof(std::uint64_t)) ? (CLS - sizeof(std::uint64_t)) : 1]{};
};
static_assert(alignof(PaddedCounter) >= CLS, "PaddedCounter alignment too small");

static void BM_ShardedCounters(benchmark::State& st) {
  const int threads = static_cast<int>(st.range(0));
  const std::size_t total_increments = 64ull * 1024ull * 1024ull; // 64M
  const std::size_t iters_per_thread = total_increments / static_cast<std::size_t>(threads);

  for (auto _ : st) {
    std::vector<PaddedCounter> shards(static_cast<std::size_t>(threads));
    std::vector<std::thread> ts;
    ts.reserve(static_cast<std::size_t>(threads));
    for (int t = 0; t < threads; ++t) {
      ts.emplace_back([t, &shards, iters_per_thread]() {
        // Each thread updates its own cache-line-private shard
        for (std::size_t i = 0; i < iters_per_thread; ++i) {
          // No need for atomics when each thread exclusively owns its shard
          shards[static_cast<std::size_t>(t)].v += 1;
        }
      });
    }
    for (auto& th : ts) th.join();

    // Reduction pass (single-threaded)
    std::uint64_t sum = 0;
    for (const auto& pc : shards) sum += pc.v;
    benchmark::DoNotOptimize(sum);
    benchmark::ClobberMemory();
  }
  st.SetLabel("sharded_padded_counters");
}
BENCHMARK(BM_ShardedCounters)->Arg(2)->Arg(4)->Arg(8);

BENCHMARK_MAIN();