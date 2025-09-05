#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <random>
#include <iostream>

// A small kernel suitable for IR reading/vectorization study
float axpy_sum(const float* a, const float* x, const float* y, std::size_t n) {
  float acc = 0.0f;
  for (std::size_t i = 0; i < n; ++i) {
    acc += a[i] * x[i] + y[i];
  }
  return acc;
}

int main() {
  constexpr std::size_t N = 1 << 20;
  std::vector<float> a(N, 1.01f), x(N, 0.5f), y(N, 0.001f);
  // Ensure data isn't trivially predictable
  std::mt19937 rng(123);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  for (std::size_t i = 0; i < N; ++i) {
    a[i] = dist(rng);
    x[i] = dist(rng);
    y[i] = dist(rng);
  }
  float s = axpy_sum(a.data(), x.data(), y.data(), N);
  std::cout << s << "\n";
  return 0;
}