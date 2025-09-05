#include <benchmark/benchmark.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#if defined(__clang__) || defined(__GNUC__)
  #define NOINLINE [[gnu::noinline]]
#elif defined(_MSC_VER)
  #define NOINLINE __declspec(noinline)
#else
  #define NOINLINE
#endif

// -----------------------------------------
// Virtual dispatch path
// -----------------------------------------
struct Op {
  virtual float apply(float x) const = 0;
  virtual ~Op() = default;
};

struct MulAdd : Op {
  float a{1.01f}, b{0.001f};
  // Prevent inlining to keep virtual-call overhead visible.
  NOINLINE float apply(float x) const override {
    return a * x + b;
  }
};

NOINLINE float loop_virtual(const Op& op, const float* in, std::size_t n) {
  float acc = 0.0f;
  for (std::size_t i = 0; i < n; ++i) {
    acc += op.apply(in[i]);
  }
  return acc;
}

// -----------------------------------------
// CRTP static dispatch path
// -----------------------------------------
template <class Derived>
struct OpC {
  // Inline-friendly, calls Derived::apply_impl
  float apply(float x) const {
    return static_cast<const Derived*>(this)->apply_impl(x);
  }
};

struct MulAddC : OpC<MulAddC> {
  float a{1.01f}, b{0.001f};
  // Allow inlining for the static path
  float apply_impl(float x) const {
    return a * x + b;
  }
};

template <class D>
NOINLINE float loop_crtp(const D& op, const float* in, std::size_t n) {
  float acc = 0.0f;
  for (std::size_t i = 0; i < n; ++i) {
    // Static dispatch, expected to inline fully at -O3
    acc += op.apply(in[i]);
  }
  return acc;
}

// -----------------------------------------
// Benchmarks
// -----------------------------------------
static void BM_Virtual_Dispatch(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  std::vector<float> x(N, 1.0f);
  MulAdd op; // dynamic polymorphism used via base ref
  for (auto _ : st) {
    float out = loop_virtual(static_cast<const Op&>(op), x.data(), N);
    benchmark::DoNotOptimize(out);
    benchmark::ClobberMemory();
  }
  st.SetLabel("virtual_dispatch");
}
BENCHMARK(BM_Virtual_Dispatch)->Arg(1<<16)->Arg(1<<20);

static void BM_CRTP_Static(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  std::vector<float> x(N, 1.0f);
  MulAddC op; // static polymorphism
  for (auto _ : st) {
    float out = loop_crtp(op, x.data(), N);
    benchmark::DoNotOptimize(out);
    benchmark::ClobberMemory();
  }
  st.SetLabel("crtp_static");
}
BENCHMARK(BM_CRTP_Static)->Arg(1<<16)->Arg(1<<20);

BENCHMARK_MAIN();