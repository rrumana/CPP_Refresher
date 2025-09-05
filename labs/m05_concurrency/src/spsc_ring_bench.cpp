#include <benchmark/benchmark.h>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <vector>
#include <chrono>

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

// A simple single-producer/single-consumer ring buffer for trivially copyable T.
// Capacity must be a power of two for mask arithmetic.
template <class T, std::size_t CapacityPow2>
struct alignas(CLS) SpscRing {
  static_assert((CapacityPow2 & (CapacityPow2 - 1)) == 0, "Capacity must be power of two");
  static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

  // Avoid false sharing by separating indices to different cache lines.
  struct alignas(CLS) Index {
    std::atomic<std::size_t> v{0};
    char pad[CLS - sizeof(std::atomic<std::size_t>) > 0 ? CLS - sizeof(std::atomic<std::size_t>) : 1]{};
  };

  // Head (write index) and tail (read index)
  Index head;
  Index tail;

  // The ring storage itself (not padded).
  T buf[CapacityPow2];

  static constexpr std::size_t mask = CapacityPow2 - 1;

  NOINLINE bool try_push(const T &x) {
    // Producer-only code
    const std::size_t h = head.v.load(std::memory_order_relaxed);
    const std::size_t t = tail.v.load(std::memory_order_acquire); // observe consumer retirements
    const std::size_t next = (h + 1) & mask;
    if (next == t) {
      return false; // full
    }
    // Store payload before publish
    buf[h] = x;
    // Publish the new head; release makes payload visible to consumer acquire
    head.v.store(next, std::memory_order_release);
    return true;
  }

  NOINLINE bool try_pop(T &out) {
    // Consumer-only code
    const std::size_t t = tail.v.load(std::memory_order_relaxed);
    const std::size_t h = head.v.load(std::memory_order_acquire); // observe producer publishes
    if (t == h) {
      return false; // empty
    }
    out = buf[t];
    const std::size_t next = (t + 1) & mask;
    // Retire slot
    tail.v.store(next, std::memory_order_release);
    return true;
  }

  // Convenience helpers for batch operations (reduce pub/retire overhead)
  NOINLINE std::size_t push_many(const T *xs, std::size_t n) {
    std::size_t pushed = 0;
    while (pushed < n) {
      if (!try_push(xs[pushed])) break;
      ++pushed;
    }
    return pushed;
  }

  NOINLINE std::size_t pop_many(T *xs, std::size_t n) {
    std::size_t popped = 0;
    while (popped < n) {
      if (!try_pop(xs[popped])) break;
      ++popped;
    }
    return popped;
  }
};

// Producer thread: push count values into the ring.
template <class RB>
NOINLINE void producer(RB *rb, std::size_t count, std::atomic<bool> *start_flag) {
  // Busy-wait until both threads are ready to start
  while (!start_flag->load(std::memory_order_acquire)) {}
  std::size_t i = 0;
  while (i < count) {
    // Try to batch a few to amortize publish overhead
    for (int k = 0; k < 8 && i < count; ++k) {
      if (rb->try_push(static_cast<uint32_t>(i))) {
        ++i;
      } else {
        break;
      }
    }
    // If ring is full, yield briefly
    if (i < count) std::this_thread::yield();
  }
}

// Consumer thread: pop count values and accumulate (to avoid DCE)
template <class RB>
NOINLINE void consumer(RB *rb, std::size_t count, std::atomic<bool> *start_flag, uint64_t *checksum) {
  while (!start_flag->load(std::memory_order_acquire)) {}
  std::size_t i = 0;
  uint64_t sum = 0;
  uint32_t val{};
  while (i < count) {
    for (int k = 0; k < 8 && i < count; ++k) {
      if (rb->try_pop(val)) {
        sum += val;
        ++i;
      } else {
        break;
      }
    }
    if (i < count) std::this_thread::yield();
  }
  *checksum = sum;
}

static void BM_SPSC_Ring_Throughput(benchmark::State &st) {
  const std::size_t items = static_cast<std::size_t>(st.range(0));
  // Use a modest ring capacity (power of two). Bigger rings reduce contention.
  using Ring = SpscRing<uint32_t, 1u << 14>; // 16K slots
  for (auto _ : st) {
    st.PauseTiming();
    Ring rb{};
    std::atomic<bool> start{false};
    uint64_t checksum = 0;
    std::thread tp(producer<Ring>, &rb, items, &start);
    std::thread tc(consumer<Ring>, &rb, items, &start, &checksum);
    // Align start of threads
    auto t0 = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    st.ResumeTiming();

    tp.join();
    tc.join();

    benchmark::DoNotOptimize(checksum);
    auto t1 = std::chrono::steady_clock::now();
    // Optionally set a custom counter: items per second
    const double secs = std::chrono::duration<double>(t1 - t0).count();
    if (secs > 0) st.counters["items_per_sec"] = benchmark::Counter(items / secs);
    benchmark::ClobberMemory();
  }
  st.SetLabel("spsc_ring_release_acquire");
}
BENCHMARK(BM_SPSC_Ring_Throughput)->Arg(1<<20)->Arg(4<<20);

static void BM_SPSC_Ring_Relaxed_Bug(benchmark::State &st) {
  // A deliberately incorrect variant using relaxed on head publish; only to show TSan catching races.
  const std::size_t items = static_cast<std::size_t>(st.range(0));
  struct BadRing {
    struct alignas(CLS) Index {
      std::atomic<std::size_t> v{0};
      char pad[CLS - sizeof(std::atomic<std::size_t>) > 0 ? CLS - sizeof(std::atomic<std::size_t>) : 1]{};
    };
    Index head, tail;
    uint32_t buf[1u << 12];
    static constexpr std::size_t mask = (1u << 12) - 1;

    bool try_push(uint32_t x) {
      const auto h = head.v.load(std::memory_order_relaxed);
      const auto t = tail.v.load(std::memory_order_relaxed); // also wrong: should be acquire
      const auto next = (h + 1) & mask;
      if (next == t) return false;
      buf[h] = x;
      head.v.store(next, std::memory_order_relaxed); // wrong: should be release
      return true;
    }
    bool try_pop(uint32_t &out) {
      const auto t = tail.v.load(std::memory_order_relaxed);
      const auto h = head.v.load(std::memory_order_relaxed); // wrong: should be acquire
      if (t == h) return false;
      out = buf[t];
      tail.v.store((t + 1) & mask, std::memory_order_relaxed); // wrong: should be release (or relaxed if only single consumer after acquire)
      return true;
    }
  };

  for (auto _ : st) {
    BadRing rb{};
    std::atomic<bool> start{false};
    uint64_t checksum = 0;
    std::thread tp([&] {
      while (!start.load(std::memory_order_acquire)) {}
      for (std::size_t i = 0; i < items; ++i) {
        while (!rb.try_push(static_cast<uint32_t>(i))) std::this_thread::yield();
      }
    });
    std::thread tc([&] {
      while (!start.load(std::memory_order_acquire)) {}
      uint32_t v{};
      for (std::size_t i = 0; i < items; ++i) {
        while (!rb.try_pop(v)) std::this_thread::yield();
        checksum += v;
      }
    });
    start.store(true, std::memory_order_release);
    tp.join();
    tc.join();
    benchmark::DoNotOptimize(checksum);
  }
  st.SetLabel("spsc_ring_relaxed_bug (use TSan to demonstrate)");
}
BENCHMARK(BM_SPSC_Ring_Relaxed_Bug)->Arg(1<<18);

BENCHMARK_MAIN();