#include <benchmark/benchmark.h>
#include <bit>
#include <cstring>
#include <vector>
#include <cstddef>
#include <cstdint>

#if defined(__clang__) || defined(__GNUC__)
  #define NOINLINE [[gnu::noinline]]
#elif defined(_MSC_VER)
  #define NOINLINE __declspec(noinline)
#else
  #define NOINLINE
#endif

NOINLINE void unsafe_pun_write(float* dst, std::uint32_t pattern) {
  auto ip = reinterpret_cast<std::uint32_t*>(dst);
  *ip = pattern; // UB: violates strict aliasing
}

NOINLINE void memcpy_write(float* dst, std::uint32_t pattern) {
  std::memcpy(dst, &pattern, sizeof(pattern));
}

NOINLINE void bitcast_write(float* dst, std::uint32_t pattern) {
  *dst = std::bit_cast<float>(pattern);
}

static void BM_UnsafePunning(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  std::vector<float> a(N, 0.0f);
  for (auto _ : st) {
    for (std::size_t i = 0; i < N; ++i) {
      std::uint32_t p = 0x3f800000u + static_cast<std::uint32_t>(i & 7);
      unsafe_pun_write(&a[i], p);
      benchmark::DoNotOptimize(a.data());
    }
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_UnsafePunning)->Arg(1<<10)->Arg(1<<14);

static void BM_MemcpyWrite(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  std::vector<float> a(N, 0.0f);
  for (auto _ : st) {
    for (std::size_t i = 0; i < N; ++i) {
      std::uint32_t p = 0x3f800000u + static_cast<std::uint32_t>(i & 7);
      memcpy_write(&a[i], p);
      benchmark::DoNotOptimize(a.data());
    }
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_MemcpyWrite)->Arg(1<<10)->Arg(1<<14);

static void BM_BitCastWrite(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  std::vector<float> a(N, 0.0f);
  for (auto _ : st) {
    for (std::size_t i = 0; i < N; ++i) {
      std::uint32_t p = 0x3f800000u + static_cast<std::uint32_t>(i & 7);
      bitcast_write(&a[i], p);
      benchmark::DoNotOptimize(a.data());
    }
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BitCastWrite)->Arg(1<<10)->Arg(1<<14);

BENCHMARK_MAIN();