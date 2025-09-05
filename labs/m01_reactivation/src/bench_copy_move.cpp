#include <benchmark/benchmark.h>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

#if defined(__clang__) || defined(__GNUC__)
  #define NOINLINE [[gnu::noinline]]
#elif defined(_MSC_VER)
  #define NOINLINE __declspec(noinline)
#else
  #define NOINLINE
#endif

struct Payload {
  std::string s;
  std::array<int, 16> buf{};
  Payload() = default;
  explicit Payload(std::string v) : s(std::move(v)) {}
  Payload(const Payload&) = default;
  Payload& operator=(const Payload&) = default;
  Payload(Payload&&) noexcept = default;
  Payload& operator=(Payload&&) noexcept = default;
  ~Payload() = default;
};

NOINLINE Payload make_payload(std::size_t i) {
  return Payload(std::string(32, static_cast<char>('a' + (i % 23))));
}

static void BM_CopyPushBack(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  std::vector<Payload> src;
  src.reserve(N);
  for (std::size_t i = 0; i < N; ++i) src.emplace_back(make_payload(i));
  for (auto _ : st) {
    st.PauseTiming();
    std::vector<Payload> dst;
    dst.reserve(N);
    st.ResumeTiming();
    for (std::size_t i = 0; i < N; ++i) {
      dst.push_back(src[i]);            // copy
      benchmark::DoNotOptimize(dst.data());
    }
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_CopyPushBack)->Arg(1<<10)->Arg(1<<14);

static void BM_MovePushBack(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  for (auto _ : st) {
    st.PauseTiming();
    std::vector<Payload> src;
    src.reserve(N);
    for (std::size_t i = 0; i < N; ++i) src.emplace_back(make_payload(i));
    std::vector<Payload> dst;
    dst.reserve(N);
    st.ResumeTiming();
    for (std::size_t i = 0; i < N; ++i) {
      dst.push_back(std::move(src[i])); // move
      benchmark::DoNotOptimize(dst.data());
    }
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_MovePushBack)->Arg(1<<10)->Arg(1<<14);

static void BM_EmplaceBack(benchmark::State& st) {
  const std::size_t N = static_cast<std::size_t>(st.range(0));
  for (auto _ : st) {
    st.PauseTiming();
    std::vector<Payload> dst;
    dst.reserve(N);
    st.ResumeTiming();
    for (std::size_t i = 0; i < N; ++i) {
      auto p = make_payload(i);
      dst.emplace_back(std::move(p));   // in-place construction
      benchmark::DoNotOptimize(dst.data());
    }
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_EmplaceBack)->Arg(1<<10)->Arg(1<<14);

BENCHMARK_MAIN();