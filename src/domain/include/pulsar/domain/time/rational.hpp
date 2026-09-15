#pragma once

#include <cstdint>

namespace pulsar::domain {

class Rational {
public:
  constexpr Rational() = default;
  Rational(std::int64_t numerator, std::int64_t denominator);

  [[nodiscard]] constexpr std::int64_t numerator() const noexcept {
    return numerator_;
  }

  [[nodiscard]] constexpr std::int64_t denominator() const noexcept {
    return denominator_;
  }

  [[nodiscard]] explicit operator double() const noexcept;

private:
  std::int64_t numerator_{0};
  std::int64_t denominator_{1};
};

[[nodiscard]] bool operator==(Rational lhs, Rational rhs) noexcept;
[[nodiscard]] bool operator!=(Rational lhs, Rational rhs) noexcept;
[[nodiscard]] bool operator<(Rational lhs, Rational rhs) noexcept;
[[nodiscard]] bool operator>(Rational lhs, Rational rhs) noexcept;
[[nodiscard]] bool operator<=(Rational lhs, Rational rhs) noexcept;
[[nodiscard]] bool operator>=(Rational lhs, Rational rhs) noexcept;

} // namespace pulsar::domain
