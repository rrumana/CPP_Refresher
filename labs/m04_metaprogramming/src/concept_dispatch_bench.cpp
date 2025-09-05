#include <benchmark/benchmark.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#if defined(__clang__) || defined(__GNUC__)
  #define NOINLINE [[gnu::noinline]]
#elif defined(_MSC_VER)
  #define NOINLINE __declspec(noinline)
#else
  #define NOINLINE
#endif

template <class T>
concept Floating = std::is_floating_point_v<T>;

// Compile-time tiled AXPY kernel: x = a * x + b
template <int TILE, Floating T>
NOINLINE void axpy_tile(const T* __restrict a,
                        const T* __restrict b,
                        T* __restrict x,
                        std::size_t n) {
  static_assert(TILE > 0, "TILE must be positive");
  std::size_t i = 0;
  // Full tiles
  for (; i + (TILE - 1) < n; i += TILE) {
#pragma GCC unroll 8
#pragma clang loop unroll(enable)
    for (int k = 0; k < TILE; ++k) {
      x[i + k] = a[i + k] * x[i + k] + b[i + k];
    }
  }
  // Tail
  for (; i < n; ++i) {
    x[i] = a[i] * x[i] + b[i];
  }
}

// Runtime-dispatched tiled kernel: compiler cannot see TILE as a constant
template <Floating T>
NOINLINE void axpy_runtime_tile(const T* __restrict a,
                                const T* __restrict b,
                                T* __restrict x,
                                std::size_t n,
                                int tile) {
  if (tile <= 0) tile = 1;
  std::size_t i = 0;
  for (; i + static_cast<std::size_t>(tile - 1) < n; i += static_cast<std::size_t>(tile)) {
    for (int k = 0; k < tile; ++k) {
      x[i + static_cast<std::size_t>(k)] = a[i + static_cast<std::size_t>(k)] * x[i + static_cast<std::size_t>(k)] + b[i + static_cast<std::size_t>(k)];
    }
  }
  for (; i < n; ++i) {
    x[i] = a[i] * x[i] + b[i];
  }
}

// Compile-time dispatch wrapper: selects among a small, curated set of tile sizes
template <Floating T>
NOINLINE void axpy_compile_time_dispatch(const T* __restrict a,
                                         const T* __restrict b,
                                         T* __restrict x,
                                         std::size_t n,
                                         int wanted_tile) {
  switch (wanted_tile) {
    case 8:  return axpy_tile<8>(a, b, x, n);
    case 16: return axpy_tile<16>(a, b, x, n);
    case 32: return axpy_tile<32>(a, b, x, n);
    default: return axpy_tile<8>(a, b, x, n); // safe default
  }
}

// -------------------------------------------------------------
// Benchmarks
// Arg( N, tile )
// -------------------------------------------------------------

static void BM_Runtime_Dispatch(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  const int tile = static_cast<int>(st.range(1));

  std::vector<float> a(N, 1.01f), b(N, 0.001f), x(N, 0.5f);
  for (auto _ : st) {
    axpy_runtime_tile(a.data(), b.data(), x.data(), N, tile);
    benchmark::DoNotOptimize(x.data());
    benchmark::ClobberMemory();
  }
  st.SetLabel(std::string("runtime_tile_") + std::to_string(tile));
}
BENCHMARK(BM_Runtime_Dispatch)->Args({1<<20, 8})->Args({1<<20, 16})->Args({1<<20, 32});

static void BM_CompileTime_Dispatch(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  const int tile = static_cast<int>(st.range(1));

  std::vector<float> a(N, 1.01f), b(N, 0.001f), x(N, 0.5f);
  for (auto _ : st) {
    axpy_compile_time_dispatch(a.data(), b.data(), x.data(), N, tile);
    benchmark::DoNotOptimize(x.data());
    benchmark::ClobberMemory();
  }
  st.SetLabel(std::string("compile_time_tile_") + std::to_string(tile));
}
BENCHMARK(BM_CompileTime_Dispatch)->Args({1<<20, 8})->Args({1<<20, 16})->Args({1<<20, 32});

BENCHMARK_MAIN();