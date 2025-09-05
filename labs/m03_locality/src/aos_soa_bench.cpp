#include <benchmark/benchmark.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
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

// AoS layout
struct P {
  float x, y, z, w;
};

// SoA layout
struct SoA {
  std::vector<float> x, y, z, w;
  explicit SoA(std::size_t n)
      : x(n), y(n), z(n), w(n) {}
};

static void init_aos(std::vector<P>& a) {
  std::mt19937 rng(123);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  for (auto& p : a) {
    p.x = dist(rng);
    p.y = dist(rng);
    p.z = dist(rng);
    p.w = dist(rng);
  }
}

static void init_soa(SoA& s) {
  std::mt19937 rng(123);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  auto n = s.x.size();
  for (std::size_t i = 0; i < n; ++i) {
    s.x[i] = dist(rng);
    s.y[i] = dist(rng);
    s.z[i] = dist(rng);
    s.w[i] = dist(rng);
  }
}

// Simple kernel: x = a * x + b (AXPY-like), operate on one field only to expose layout differences

NOINLINE void kernel_aos_axpy_x(P* p, const float* a, const float* b, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    p[i].x = a[i] * p[i].x + b[i];
  }
}

NOINLINE void kernel_soa_axpy_x(float* __restrict x, const float* __restrict a, const float* __restrict b, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    x[i] = a[i] * x[i] + b[i];
  }
}

// Blocked variants; block size B chosen to keep a, b, and x working sets in L1/L2
NOINLINE void kernel_aos_axpy_x_blocked(P* p, const float* a, const float* b, std::size_t n, std::size_t B) {
  for (std::size_t i0 = 0; i0 < n; i0 += B) {
    std::size_t i1 = std::min(i0 + B, n);
    for (std::size_t i = i0; i < i1; ++i) {
      p[i].x = a[i] * p[i].x + b[i];
    }
  }
}

NOINLINE void kernel_soa_axpy_x_blocked(float* __restrict x, const float* __restrict a, const float* __restrict b, std::size_t n, std::size_t B) {
  for (std::size_t i0 = 0; i0 < n; i0 += B) {
    std::size_t i1 = std::min(i0 + B, n);
    for (std::size_t i = i0; i < i1; ++i) {
      x[i] = a[i] * x[i] + b[i];
    }
  }
}

// Sum reduction over x (to observe gather vs unit-stride loads)
NOINLINE float kernel_aos_sum_x(const P* p, std::size_t n) {
  float acc = 0.0f;
  for (std::size_t i = 0; i < n; ++i) {
    acc += p[i].x;
  }
  return acc;
}

NOINLINE float kernel_soa_sum_x(const float* x, std::size_t n) {
  float acc = 0.0f;
  for (std::size_t i = 0; i < n; ++i) {
    acc += x[i];
  }
  return acc;
}

// Benchmarks
// Args: N, B (B may be 0 for "no blocking")

static void BM_AoS_AXPY_X(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  const std::size_t B = static_cast<std::size_t>(st.range(1));

  std::vector<P> p(N);
  init_aos(p);
  std::vector<float> a(N, 1.01f), b(N, 0.001f);

  for (auto _ : st) {
    if (B == 0) {
      kernel_aos_axpy_x(p.data(), a.data(), b.data(), N);
    } else {
      kernel_aos_axpy_x_blocked(p.data(), a.data(), b.data(), N, B);
    }
    benchmark::DoNotOptimize(p.data());
    benchmark::ClobberMemory();
  }
  st.SetLabel(B == 0 ? "AoS_axpy" : ("AoS_axpy_blk" + std::to_string(B)));
}
BENCHMARK(BM_AoS_AXPY_X)->Args({1<<20, 0})->Args({1<<20, 8<<10})->Args({1<<20, 32<<10});

static void BM_SoA_AXPY_X(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  const std::size_t B = static_cast<std::size_t>(st.range(1));

  SoA s(N);
  init_soa(s);
  std::vector<float> a(N, 1.01f), b(N, 0.001f);

  for (auto _ : st) {
    if (B == 0) {
      kernel_soa_axpy_x(s.x.data(), a.data(), b.data(), N);
    } else {
      kernel_soa_axpy_x_blocked(s.x.data(), a.data(), b.data(), N, B);
    }
    benchmark::DoNotOptimize(s.x.data());
    benchmark::ClobberMemory();
  }
  st.SetLabel(B == 0 ? "SoA_axpy" : ("SoA_axpy_blk" + std::to_string(B)));
}
BENCHMARK(BM_SoA_AXPY_X)->Args({1<<20, 0})->Args({1<<20, 8<<10})->Args({1<<20, 32<<10});

static void BM_AoS_SumX(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  std::vector<P> p(N);
  init_aos(p);
  float out = 0.0f;
  for (auto _ : st) {
    out = kernel_aos_sum_x(p.data(), N);
    benchmark::DoNotOptimize(out);
  }
  st.SetLabel("AoS_sum_x");
}
BENCHMARK(BM_AoS_SumX)->Arg(1<<20);

static void BM_SoA_SumX(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  SoA s(N);
  init_soa(s);
  float out = 0.0f;
  for (auto _ : st) {
    out = kernel_soa_sum_x(s.x.data(), N);
    benchmark::DoNotOptimize(out);
  }
  st.SetLabel("SoA_sum_x");
}
BENCHMARK(BM_SoA_SumX)->Arg(1<<20);

BENCHMARK_MAIN();