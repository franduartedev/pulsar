#include <pulsar/domain/time/rational.hpp>

#include <limits>
#include <stdexcept>

namespace pulsar::domain {
namespace {

#if defined(__SIZEOF_INT128__)
using WideInt = __int128_t;
#else
#error "PULSAR requires a compiler with signed __int128 support for exact rational comparisons."
#endif

[[nodiscard]] std::int64_t checked_to_int64(WideInt value) {
  if (value < static_cast<WideInt>(std::numeric_limits<std::int64_t>::min()) ||
      value > static_cast<WideInt>(std::numeric_limits<std::int64_t>::max())) {
    throw std::overflow_error{"normalized rational cannot be represented as int64"};
  }

  return static_cast<std::int64_t>(value);
}

[[nodiscard]] WideInt abs_wide(WideInt value) noexcept {
  return value < 0 ? -value : value;
}

[[nodiscard]] WideInt gcd_wide(WideInt lhs, WideInt rhs) noexcept {
  lhs = abs_wide(lhs);
  rhs = abs_wide(rhs);

  while (rhs != 0) {
    const WideInt next = lhs % rhs;
    lhs = rhs;
    rhs = next;
  }

  return lhs;
}

} // namespace

Rational::Rational(std::int64_t numerator, std::int64_t denominator)
    : numerator_{numerator}, denominator_{denominator} {
  if (denominator == 0) {
    throw std::invalid_argument{"rational denominator cannot be zero"};
  }

  if (numerator == 0) {
    numerator_ = 0;
    denominator_ = 1;
    return;
  }

  WideInt normalized_numerator = numerator;
  WideInt normalized_denominator = denominator;

  if (normalized_denominator < 0) {
    normalized_numerator = -normalized_numerator;
    normalized_denominator = -normalized_denominator;
  }

  const WideInt divisor = gcd_wide(normalized_numerator, normalized_denominator);
  normalized_numerator /= divisor;
  normalized_denominator /= divisor;

  numerator_ = checked_to_int64(normalized_numerator);
  denominator_ = checked_to_int64(normalized_denominator);
}

Rational::operator double() const noexcept {
  return static_cast<double>(numerator_) / static_cast<double>(denominator_);
}

bool operator==(Rational lhs, Rational rhs) noexcept {
  return lhs.numerator() == rhs.numerator() && lhs.denominator() == rhs.denominator();
}

bool operator!=(Rational lhs, Rational rhs) noexcept {
  return !(lhs == rhs);
}

bool operator<(Rational lhs, Rational rhs) noexcept {
  return static_cast<WideInt>(lhs.numerator()) * static_cast<WideInt>(rhs.denominator()) <
         static_cast<WideInt>(rhs.numerator()) * static_cast<WideInt>(lhs.denominator());
}

bool operator>(Rational lhs, Rational rhs) noexcept {
  return rhs < lhs;
}

bool operator<=(Rational lhs, Rational rhs) noexcept {
  return !(rhs < lhs);
}

bool operator>=(Rational lhs, Rational rhs) noexcept {
  return !(lhs < rhs);
}

} // namespace pulsar::domain
