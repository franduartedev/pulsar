#pragma once

#include <cstdint>

namespace pulsar::domain {

// PULSAR frame indices are zero-based: the first frame is FrameNumber{0}.
class FrameNumber {
public:
  explicit constexpr FrameNumber(std::int64_t value) noexcept : value_{value} {}

  [[nodiscard]] constexpr std::int64_t value() const noexcept {
    return value_;
  }

private:
  std::int64_t value_;
};

[[nodiscard]] constexpr bool operator==(FrameNumber lhs, FrameNumber rhs) noexcept {
  return lhs.value() == rhs.value();
}

[[nodiscard]] constexpr bool operator!=(FrameNumber lhs, FrameNumber rhs) noexcept {
  return !(lhs == rhs);
}

[[nodiscard]] constexpr bool operator<(FrameNumber lhs, FrameNumber rhs) noexcept {
  return lhs.value() < rhs.value();
}

[[nodiscard]] constexpr bool operator>(FrameNumber lhs, FrameNumber rhs) noexcept {
  return rhs < lhs;
}

[[nodiscard]] constexpr bool operator<=(FrameNumber lhs, FrameNumber rhs) noexcept {
  return !(rhs < lhs);
}

[[nodiscard]] constexpr bool operator>=(FrameNumber lhs, FrameNumber rhs) noexcept {
  return !(lhs < rhs);
}

} // namespace pulsar::domain
