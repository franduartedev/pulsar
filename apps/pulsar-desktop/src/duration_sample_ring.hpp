#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <vector>

// Fixed-capacity ring of recent durations for on-demand p50/p95/max reporting.
// Fixed memory, no allocation on push. Single-writer (GUI/control thread);
// snapshots may copy for reporting. Percentiles are computed by the caller,
// never here and never on a hot path.
class DurationSampleRing final {
 public:
  static constexpr std::size_t capacity = 64;

  void push(std::chrono::nanoseconds sample) noexcept {
    samples_[head_] = sample;
    head_ = (head_ + 1) % capacity;
    if (count_ < capacity) {
      ++count_;
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return count_; }

  [[nodiscard]] std::vector<std::chrono::nanoseconds> snapshot() const {
    std::vector<std::chrono::nanoseconds> out;
    out.reserve(count_);
    for (std::size_t i = 0; i < count_; ++i) {
      out.push_back(samples_[(head_ + capacity - count_ + i) % capacity]);
    }
    return out;
  }

 private:
  std::array<std::chrono::nanoseconds, capacity> samples_{};
  std::size_t head_{0};
  std::size_t count_{0};
};
