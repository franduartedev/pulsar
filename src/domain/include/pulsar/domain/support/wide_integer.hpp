#pragma once

#include <cstddef>
#include <cstdint>

namespace pulsar::domain {

// Minimal signed 128-bit integer for PULSAR's exact time and audio conversions.
//
// Rational normalization, media-time comparisons and timestamp conversion need
// 64x64 -> 128 bit products. GCC and Clang provide __int128; MSVC does not, so
// the arithmetic lives in one portable type instead of an #error in every
// translation unit that needs it.
//
// Representation is two's complement, with wrapping overflow like a native
// integer type. Every caller performs its own range check before widening, so
// wrapping here can only happen on inputs the callers already reject.
class WideInteger final {
public:
  constexpr WideInteger() noexcept = default;

  // Deliberately implicit: existing arithmetic and comparisons mix in plain
  // integer literals (0, 1, 2) and int64 bounds.
  constexpr WideInteger(std::int64_t value) noexcept
      : high_{value < 0 ? ~std::uint64_t{0} : std::uint64_t{0}},
        low_{static_cast<std::uint64_t>(value)} {}

  [[nodiscard]] static constexpr WideInteger from_words(std::uint64_t high,
                                                        std::uint64_t low) noexcept {
    WideInteger result;
    result.high_ = high;
    result.low_ = low;
    return result;
  }

  // Zero-extends an unsigned value; unlike the int64 constructor this keeps
  // values such as std::numeric_limits<std::size_t>::max() positive.
  [[nodiscard]] static constexpr WideInteger from_unsigned(std::uint64_t value) noexcept {
    return from_words(std::uint64_t{0}, value);
  }

  [[nodiscard]] constexpr bool is_negative() const noexcept { return (high_ >> 63U) != 0U; }
  [[nodiscard]] constexpr bool is_zero() const noexcept { return high_ == 0U && low_ == 0U; }

  // Truncates toward zero, exactly like a narrowing conversion of a native
  // integer. Callers check the range first.
  [[nodiscard]] constexpr std::int64_t to_int64() const noexcept {
    return static_cast<std::int64_t>(low_);
  }

  [[nodiscard]] constexpr std::uint64_t low_word() const noexcept { return low_; }
  [[nodiscard]] constexpr std::uint64_t high_word() const noexcept { return high_; }

  explicit constexpr operator std::int64_t() const noexcept { return to_int64(); }
  explicit constexpr operator std::uint64_t() const noexcept { return low_; }

  friend constexpr bool operator==(WideInteger lhs, WideInteger rhs) noexcept {
    return lhs.high_ == rhs.high_ && lhs.low_ == rhs.low_;
  }
  friend constexpr bool operator!=(WideInteger lhs, WideInteger rhs) noexcept {
    return !(lhs == rhs);
  }
  friend constexpr bool operator<(WideInteger lhs, WideInteger rhs) noexcept {
    if (lhs.high_ != rhs.high_) {
      return static_cast<std::int64_t>(lhs.high_) < static_cast<std::int64_t>(rhs.high_);
    }
    return lhs.low_ < rhs.low_;
  }
  friend constexpr bool operator>(WideInteger lhs, WideInteger rhs) noexcept {
    return rhs < lhs;
  }
  friend constexpr bool operator<=(WideInteger lhs, WideInteger rhs) noexcept {
    return !(rhs < lhs);
  }
  friend constexpr bool operator>=(WideInteger lhs, WideInteger rhs) noexcept {
    return !(lhs < rhs);
  }

  friend constexpr WideInteger operator+(WideInteger lhs, WideInteger rhs) noexcept {
    const std::uint64_t low = lhs.low_ + rhs.low_;
    const std::uint64_t carry = low < lhs.low_ ? 1U : 0U;
    return from_words(lhs.high_ + rhs.high_ + carry, low);
  }
  friend constexpr WideInteger operator-(WideInteger lhs, WideInteger rhs) noexcept {
    const std::uint64_t low = lhs.low_ - rhs.low_;
    const std::uint64_t borrow = lhs.low_ < rhs.low_ ? 1U : 0U;
    return from_words(lhs.high_ - rhs.high_ - borrow, low);
  }
  friend constexpr WideInteger operator-(WideInteger value) noexcept {
    return WideInteger{std::int64_t{0}} - value;
  }

  friend WideInteger operator*(WideInteger lhs, WideInteger rhs) noexcept {
    return multiply(lhs, rhs);
  }

  // Division by zero is defined as zero here so the type stays noexcept; every
  // call site divides by an already validated non-zero value.
  friend WideInteger operator/(WideInteger lhs, WideInteger rhs) noexcept {
    WideInteger quotient{std::int64_t{0}};
    divide(lhs, rhs, &quotient, nullptr);
    return quotient;
  }
  friend WideInteger operator%(WideInteger lhs, WideInteger rhs) noexcept {
    WideInteger remainder{std::int64_t{0}};
    divide(lhs, rhs, nullptr, &remainder);
    return remainder;
  }

  friend constexpr WideInteger operator<<(WideInteger value, int count) noexcept {
    if (count <= 0) return value;
    if (count >= 128) return WideInteger{std::int64_t{0}};
    const auto shift = static_cast<unsigned>(count);
    const std::uint64_t low =
        shift >= 64U ? 0U : static_cast<std::uint64_t>(value.low_ << shift);
    std::uint64_t high = 0U;
    if (shift >= 64U) {
      high = value.low_ << (shift - 64U);
    } else {
      high = value.high_ << shift;
      if (shift != 0U) high |= value.low_ >> (64U - shift);
    }
    return from_words(high, low);
  }

  constexpr WideInteger& operator+=(WideInteger other) noexcept {
    *this = *this + other;
    return *this;
  }
  constexpr WideInteger& operator-=(WideInteger other) noexcept {
    *this = *this - other;
    return *this;
  }
  WideInteger& operator*=(WideInteger other) noexcept {
    *this = *this * other;
    return *this;
  }
  WideInteger& operator/=(WideInteger other) noexcept {
    *this = *this / other;
    return *this;
  }
  WideInteger& operator%=(WideInteger other) noexcept {
    *this = *this % other;
    return *this;
  }
  constexpr WideInteger& operator++() noexcept {
    *this += WideInteger{std::int64_t{1}};
    return *this;
  }
  constexpr WideInteger& operator--() noexcept {
    *this -= WideInteger{std::int64_t{1}};
    return *this;
  }

private:
  [[nodiscard]] static constexpr WideInteger negate(WideInteger value) noexcept {
    return WideInteger{std::int64_t{0}} - value;
  }

  [[nodiscard]] static WideInteger multiply(WideInteger lhs, WideInteger rhs) noexcept {
    const bool negative = lhs.is_negative() != rhs.is_negative();
    WideInteger left = lhs;
    WideInteger right = rhs;
    if (left.is_negative()) left = negate(left);
    if (right.is_negative()) right = negate(right);

    // Magnitudes as 32-bit limbs; the low four limbs are the product modulo
    // 2^128, which is exact whenever the caller's range check allows it.
    const std::uint64_t left_words[2] = {left.low_, left.high_};
    const std::uint64_t right_words[2] = {right.low_, right.high_};
    std::uint32_t left_limbs[4] = {};
    std::uint32_t right_limbs[4] = {};
    for (std::size_t word = 0; word < 2; ++word) {
      left_limbs[word * 2] = static_cast<std::uint32_t>(left_words[word] & 0xFFFFFFFFU);
      left_limbs[word * 2 + 1] = static_cast<std::uint32_t>(left_words[word] >> 32U);
      right_limbs[word * 2] = static_cast<std::uint32_t>(right_words[word] & 0xFFFFFFFFU);
      right_limbs[word * 2 + 1] = static_cast<std::uint32_t>(right_words[word] >> 32U);
    }

    std::uint32_t product_limbs[4] = {};
    for (std::size_t i = 0; i < 4; ++i) {
      std::uint64_t carry = 0;
      for (std::size_t j = 0; i + j < 4; ++j) {
        const std::uint64_t current = static_cast<std::uint64_t>(left_limbs[i]) *
                                          right_limbs[j] +
                                      product_limbs[i + j] + carry;
        product_limbs[i + j] = static_cast<std::uint32_t>(current & 0xFFFFFFFFU);
        carry = current >> 32U;
      }
    }

    const std::uint64_t low = static_cast<std::uint64_t>(product_limbs[0]) |
                              (static_cast<std::uint64_t>(product_limbs[1]) << 32U);
    const std::uint64_t high = static_cast<std::uint64_t>(product_limbs[2]) |
                               (static_cast<std::uint64_t>(product_limbs[3]) << 32U);
    const WideInteger magnitude = from_words(high, low);
    return negative ? negate(magnitude) : magnitude;
  }

  // Produces the quotient and/or remainder in out-parameters. A nested struct
  // holding two WideInteger members cannot be declared inside the class
  // itself, because the class is still incomplete at that point.
  static void divide(WideInteger dividend, WideInteger divisor,
                     WideInteger* quotient_out, WideInteger* remainder_out) noexcept {
    if (divisor.is_zero()) {
      if (quotient_out != nullptr) *quotient_out = WideInteger{std::int64_t{0}};
      if (remainder_out != nullptr) *remainder_out = WideInteger{std::int64_t{0}};
      return;
    }
    const bool dividend_negative = dividend.is_negative();
    const bool divisor_negative = divisor.is_negative();
    WideInteger numerator = dividend_negative ? negate(dividend) : dividend;
    const WideInteger denominator = divisor_negative ? negate(divisor) : divisor;

    // |INT128_MIN| is not representable as a positive two's-complement value,
    // so the magnitude-based loop below cannot use it. It is the only divisor
    // with this problem: every dividend magnitude is <= 2^127, so the quotient
    // is either zero or one.
    constexpr std::uint64_t sign_bit = std::uint64_t{1} << 63U;
    if (denominator.high_ == sign_bit && denominator.low_ == 0) {
      const bool magnitudes_equal = numerator.high_ == sign_bit && numerator.low_ == 0;
      if (quotient_out != nullptr) {
        *quotient_out = magnitudes_equal
                            ? ((dividend_negative != divisor_negative) ? WideInteger{-1}
                                                                       : WideInteger{1})
                            : WideInteger{0};
      }
      if (remainder_out != nullptr) {
        *remainder_out = magnitudes_equal ? WideInteger{0}
                                          : (dividend_negative ? negate(numerator) : numerator);
      }
      return;
    }

    // Fast path: both magnitudes fit in 64 bits, which is the common case for
    // media timestamps. A native division replaces the 128-step loop.
    if (numerator.high_ == 0 && denominator.high_ == 0) {
      const std::uint64_t quotient = numerator.low_ / denominator.low_;
      const std::uint64_t remainder = numerator.low_ % denominator.low_;
      if (quotient_out != nullptr) {
        *quotient_out = (dividend_negative != divisor_negative)
                            ? negate(from_words(0, quotient))
                            : from_words(0, quotient);
      }
      if (remainder_out != nullptr) {
        *remainder_out = dividend_negative ? negate(from_words(0, remainder))
                                           : from_words(0, remainder);
      }
      return;
    }

    // A dividend of INT128_MIN has magnitude 2^127, which is not representable
    // as a positive value either. Compute with 2^127 - 1 (INT128_MAX) and add
    // the one missing unit back afterwards.
    if (numerator.high_ == sign_bit && numerator.low_ == 0) {
      const WideInteger reduced = from_words(static_cast<std::uint64_t>(0x7FFFFFFFFFFFFFFFULL),
                                             ~std::uint64_t{0});
      WideInteger quotient{std::int64_t{0}};
      WideInteger remainder{std::int64_t{0}};
      divide(reduced, denominator, &quotient, &remainder);
      remainder += WideInteger{std::int64_t{1}};
      if (remainder >= denominator) {
        remainder -= denominator;
        quotient += WideInteger{std::int64_t{1}};
      }
      if (quotient_out != nullptr) {
        *quotient_out = (dividend_negative != divisor_negative) ? negate(quotient) : quotient;
      }
      if (remainder_out != nullptr) {
        *remainder_out = dividend_negative ? negate(remainder) : remainder;
      }
      return;
    }

    // Restoring long division. The running remainder stays below the divisor,
    // so the extra carry of the shifted remainder is folded back explicitly.
    WideInteger quotient{std::int64_t{0}};
    WideInteger remainder{std::int64_t{0}};
    for (int bit = 127; bit >= 0; --bit) {
      const bool carry = (remainder.high_ >> 63U) != 0U;
      remainder = remainder << 1;
      const std::uint64_t source_bit =
          bit >= 64 ? (numerator.high_ >> static_cast<unsigned>(bit - 64)) & 1U
                    : (numerator.low_ >> static_cast<unsigned>(bit)) & 1U;
      if (source_bit != 0U) remainder.low_ |= 1U;
      quotient = quotient << 1;
      if (carry || remainder >= denominator) {
        remainder = remainder - denominator;
        quotient.low_ |= 1U;
      }
    }

    if (quotient_out != nullptr) {
      *quotient_out = (dividend_negative != divisor_negative) ? negate(quotient) : quotient;
    }
    if (remainder_out != nullptr) {
      *remainder_out = dividend_negative ? negate(remainder) : remainder;
    }
  }

  std::uint64_t high_{0};
  std::uint64_t low_{0};
};

} // namespace pulsar::domain
