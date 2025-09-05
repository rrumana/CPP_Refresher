#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>
#include <new>      // hardware_destructive_interference_size

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
constexpr std::size_t CLS = 64; // reasonable default on many x86
#endif

struct SharedSlot {
  std::uint64_t c{0};
};

// Ensure each slot starts on a separate cache line and occupies at least one full line.
// alignas alone is insufficient with std::vector (contiguous elements); make size a multiple of CLS.
struct alignas(CLS) PaddedSlot {
  std::uint64_t c{0};
  static constexpr std::size_t PAD = (CLS > sizeof(std::uint64_t)) ? (CLS - sizeof(std::uint64_t)) : 1;
  char pad[PAD];
};
static_assert(alignof(PaddedSlot) >= CLS, "PaddedSlot alignment too small");
static_assert(sizeof(PaddedSlot) % CLS == 0, "PaddedSlot should fill full cache lines");

template <typename Slot>
NOINLINE void thread_body(Slot* slot, std::size_t iters) {
  // Force each increment to touch memory to expose coherence traffic.
  volatile std::uint64_t* vp = &slot->c;
  for (std::size_t i = 0; i < iters; ++i) {
    *vp += 1;
  }
}

template <typename Slot>
NOINLINE std::uint64_t run_false_sharing_trial(int threads, std::size_t iters_per_thread) {
  std::vector<Slot> slots(static_cast<std::size_t>(threads));
  std::vector<std::thread> ts;
  ts.reserve(static_cast<std::size_t>(threads));
  for (int t = 0; t < threads; ++t) {
    ts.emplace_back([&slots, iters_per_thread, t]() {
      thread_body(&slots[static_cast<std::size_t>(t)], iters_per_thread);
    });
  }
  for (auto& th : ts) th.join();
  std::uint64_t sum = 0;
  for (const auto& s : slots) sum += s.c;
  return sum;
}

static void BM_FalseSharing_Shared(benchmark::State& st) {
  const int threads = static_cast<int>(st.range(0));
  // Keep total work roughly constant across thread counts
  const std::size_t total_iters = 64ull * 1024ull * 1024ull; // 64M increments total
  const std::size_t iters_per_thread = total_iters / static_cast<std::size_t>(threads);

  for (auto _ : st) {
    auto sum = run_false_sharing_trial<SharedSlot>(threads, iters_per_thread);
    benchmark::DoNotOptimize(sum);
    benchmark::ClobberMemory();
  }
  st.SetLabel("shared_line_slots");
}
BENCHMARK(BM_FalseSharing_Shared)->Arg(2)->Arg(4)->Arg(8);

static void BM_FalseSharing_Padded(benchmark::State& st) {
  const int threads = static_cast<int>(st.range(0));
  const std::size_t total_iters = 64ull * 1024ull * 1024ull;
  const std::size_t iters_per_thread = total_iters / static_cast<std::size_t>(threads);

  for (auto _ : st) {
    auto sum = run_false_sharing_trial<PaddedSlot>(threads, iters_per_thread);
    benchmark::DoNotOptimize(sum);
    benchmark::ClobberMemory();
  }
  st.SetLabel("padded_slots");
}
BENCHMARK(BM_FalseSharing_Padded)->Arg(2)->Arg(4)->Arg(8);

BENCHMARK_MAIN();