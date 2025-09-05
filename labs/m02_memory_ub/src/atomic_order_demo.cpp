#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>

// Global shared state
static std::atomic<int> flag{0};
static int data = 0;

// Writer sets data then signals via flag
static void writer() {
  // Small delay to increase interleaving opportunity
  std::this_thread::sleep_for(std::chrono::microseconds(10));
  data = 42;
#if defined(ORDER_RELAXED)
  flag.store(1, std::memory_order_relaxed);
#elif defined(ORDER_ACQREL)
  flag.store(1, std::memory_order_release);
#else
  flag.store(1, std::memory_order_seq_cst);
#endif
}

// Reader waits for flag then reads data
static void reader() {
#if defined(ORDER_RELAXED)
  while (flag.load(std::memory_order_relaxed) == 0) {
    // busy wait
  }
#elif defined(ORDER_ACQREL)
  while (flag.load(std::memory_order_acquire) == 0) {
    // busy wait
  }
#else
  while (flag.load(std::memory_order_seq_cst) == 0) {
    // busy wait
  }
#endif

  // Non-atomic read participates in race unless acquire/release establishes happens-before
  int x = data;
  // Print occasionally (kept minimal to avoid perturbing TSan)
  static thread_local int printed = 0;
  if (printed++ == 0) {
    std::cout << "Observed data=" << x << "\n";
  }
}

int main() {
  // Run multiple iterations to encourage a racy interleaving under relaxed
  constexpr int ITERS = 2000;
  int mismatches = 0;

  for (int i = 0; i < ITERS; ++i) {
    flag.store(0, std::memory_order_relaxed);
    data = 0;
    std::thread t1(writer);
    std::thread t2(reader);
    t1.join();
    t2.join();

#if defined(ORDER_RELAXED)
    // Under relaxed ordering, this is undefined behavior; we may observe stale data.
    if (data != 42) ++mismatches;
#elif defined(ORDER_ACQREL)
    // With acquire/release, reader should see 42
    if (data != 42) ++mismatches;
#endif
  }

#if defined(ORDER_RELAXED)
  std::cout << "[RELAXED] Completed " << ITERS << " iterations. (TSan should report a data race.)\n";
#elif defined(ORDER_ACQREL)
  std::cout << "[ACQ_REL] Completed " << ITERS << " iterations. Mismatches=" << mismatches
            << " (expected 0; TSan should report no data race.)\n";
#else
  std::cout << "[SEQ_CST] Completed " << ITERS << " iterations.\n";
#endif

  return 0;
}