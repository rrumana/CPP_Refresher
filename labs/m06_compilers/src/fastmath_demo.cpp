// fastmath_demo.cpp — Fast-math policy exploration
// Build target: fastmath_demo

#include <cstddef>
#include <cmath>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

using clock_t = std::chrono::high_resolution_clock;

// Kernel uses operations impacted by fast-math: reassociation, reciprocal approx, FMA opportunities.
static float compute_kernel(const float* x, std::size_t n) {
  float sum = 0.0f;
  for (std::size_t i = 0; i < n; ++i) {
    float v = x[i];
    // Mix of transcendental, sqrt, and reciprocal to highlight differences.
    float t1 = std::sinf(v) * std::cosf(v);
    float t2 = std::sqrtf(v + 1.0f);
    float t3 = 1.0f / (v + 1.0f);
    // Encourage FMA by a*b + c pattern
    sum += t1 * t2 + t3;
  }
  return sum;
}

int main() {
  constexpr std::size_t N = 1u << 20; // 1M elements
  std::vector<float> x(N);
  std::mt19937 rng(123);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  for (auto& v : x) v = dist(rng);

  auto t0 = clock_t::now();
  float s = compute_kernel(x.data(), x.size());
  auto t1 = clock_t::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

  std::cout << "sum=" << s << " time_ms=" << ms << "\n";
  return 0;
}