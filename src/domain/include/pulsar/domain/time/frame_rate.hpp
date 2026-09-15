#pragma once

#include <pulsar/domain/time/rational.hpp>

#include <cstdint>

namespace pulsar::domain {

class FrameRate {
public:
  FrameRate(std::int64_t numerator, std::int64_t denominator);
  explicit FrameRate(Rational rate);

  [[nodiscard]] constexpr Rational rational() const noexcept {
    return rate_;
  }

  [[nodiscard]] explicit operator double() const noexcept;

private:
  Rational rate_;
};

} // namespace pulsar::domain
