#include <pulsar/domain/time/media_time.hpp>

#include <limits>
#include <numeric>
#include <stdexcept>

namespace pulsar::domain {
namespace {

#if defined(__SIZEOF_INT128__)
using WideInt = __int128_t;
#else
#error "PULSAR requires a compiler with signed __int128 support for exact media-time operations."
#endif

[[nodiscard]] std::int64_t checked_to_int64(WideInt value) {
  if (value < static_cast<WideInt>(std::numeric_limits<std::int64_t>::min()) ||
      value > static_cast<WideInt>(std::numeric_limits<std::int64_t>::max())) {
    throw std::overflow_error{"value cannot be represented as int64"};
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

[[nodiscard]] WideInt checked_multiply(WideInt lhs, WideInt rhs) {
  constexpr WideInt wide_max =
      (static_cast<WideInt>(1) << 126) + ((static_cast<WideInt>(1) << 126) - 1);
  constexpr WideInt wide_min = -wide_max - 1;

  if (lhs == 0 || rhs == 0) {
    return 0;
  }

  if (lhs > 0) {
    if (rhs > 0) {
      if (lhs > wide_max / rhs) {
        throw std::overflow_error{"media-time multiplication exceeds supported range"};
      }
    } else if (rhs < wide_min / lhs) {
      throw std::overflow_error{"media-time multiplication exceeds supported range"};
    }
  } else {
    if (rhs > 0) {
      if (lhs < wide_min / rhs) {
        throw std::overflow_error{"media-time multiplication exceeds supported range"};
      }
    } else if (lhs < wide_max / rhs) {
      throw std::overflow_error{"media-time multiplication exceeds supported range"};
    }
  }

  return lhs * rhs;
}

[[nodiscard]] int compare_positive_fractions(WideInt lhs_numerator, WideInt lhs_denominator,
                                             WideInt rhs_numerator, WideInt rhs_denominator) noexcept {
  bool reversed = false;
  while (true) {
    const WideInt lhs_quotient = lhs_numerator / lhs_denominator;
    const WideInt rhs_quotient = rhs_numerator / rhs_denominator;
    if (lhs_quotient != rhs_quotient) {
      const bool lhs_is_less = lhs_quotient < rhs_quotient;
      return reversed ? (lhs_is_less ? 1 : -1) : (lhs_is_less ? -1 : 1);
    }

    const WideInt lhs_remainder = lhs_numerator % lhs_denominator;
    const WideInt rhs_remainder = rhs_numerator % rhs_denominator;
    if (lhs_remainder == 0 || rhs_remainder == 0) {
      if (lhs_remainder == 0 && rhs_remainder == 0) {
        return 0;
      }
      const bool lhs_is_less = lhs_remainder == 0;
      return reversed ? (lhs_is_less ? 1 : -1) : (lhs_is_less ? -1 : 1);
    }

    lhs_numerator = lhs_denominator;
    lhs_denominator = lhs_remainder;
    rhs_numerator = rhs_denominator;
    rhs_denominator = rhs_remainder;
    reversed = !reversed;
  }
}

[[nodiscard]] WideInt media_time_numerator(MediaTime time) noexcept {
  return static_cast<WideInt>(time.value()) * time.time_base().numerator();
}

[[nodiscard]] int compare_media_times(MediaTime lhs, MediaTime rhs) noexcept {
  const WideInt lhs_numerator = media_time_numerator(lhs);
  const WideInt rhs_numerator = media_time_numerator(rhs);
  if ((lhs_numerator < 0) != (rhs_numerator < 0)) {
    return lhs_numerator < 0 ? -1 : 1;
  }
  if (lhs_numerator == 0 && rhs_numerator == 0) {
    return 0;
  }

  const bool negative = lhs_numerator < 0;
  const int result = compare_positive_fractions(
      abs_wide(lhs_numerator), lhs.time_base().denominator(), abs_wide(rhs_numerator),
      rhs.time_base().denominator());
  return negative ? -result : result;
}

[[nodiscard]] WideInt checked_add(WideInt lhs, WideInt rhs) {
  constexpr WideInt wide_max =
      (static_cast<WideInt>(1) << 126) + ((static_cast<WideInt>(1) << 126) - 1);
  constexpr WideInt wide_min = -wide_max - 1;

  if ((rhs > 0 && lhs > wide_max - rhs) || (rhs < 0 && lhs < wide_min - rhs)) {
    throw std::overflow_error{"media-time addition exceeds supported range"};
  }

  return lhs + rhs;
}

[[nodiscard]] Rational combine_seconds(MediaTime lhs, MediaTime rhs, bool subtract_rhs) {
  const Rational left = lhs.seconds();
  const Rational right = rhs.seconds();

  const WideInt left_numerator = left.numerator();
  const WideInt right_numerator = subtract_rhs ? -static_cast<WideInt>(right.numerator())
                                               : static_cast<WideInt>(right.numerator());

  const WideInt numerator =
      checked_add(checked_multiply(left_numerator, right.denominator()),
                  checked_multiply(right_numerator, left.denominator()));
  const WideInt denominator = checked_multiply(left.denominator(), right.denominator());
  const WideInt divisor = gcd_wide(numerator, denominator);

  return Rational{checked_to_int64(numerator / divisor), checked_to_int64(denominator / divisor)};
}

[[nodiscard]] FrameNumber rounded_frame(WideInt numerator, WideInt denominator,
                                        RoundingMode rounding) {
  if (denominator <= 0) {
    throw std::invalid_argument{"rounding denominator must be positive"};
  }

  const WideInt quotient = numerator / denominator;
  const WideInt remainder = numerator % denominator;

  if (remainder == 0) {
    return FrameNumber{checked_to_int64(quotient)};
  }

  switch (rounding) {
  case RoundingMode::Floor:
    return FrameNumber{checked_to_int64(numerator < 0 ? quotient - 1 : quotient)};
  case RoundingMode::Ceil:
    return FrameNumber{checked_to_int64(numerator > 0 ? quotient + 1 : quotient)};
  case RoundingMode::Nearest: {
    const WideInt doubled_remainder = abs_wide(remainder) * 2;
    const WideInt magnitude = abs_wide(quotient);
    WideInt rounded_magnitude = magnitude;

    if (doubled_remainder >= denominator) {
      rounded_magnitude += 1;
    }

    return FrameNumber{checked_to_int64(numerator < 0 ? -rounded_magnitude : rounded_magnitude)};
  }
  }

  throw std::invalid_argument{"unknown rounding mode"};
}

} // namespace

MediaTime::MediaTime(std::int64_t value, Rational time_base)
    : value_{value}, time_base_{time_base} {
  if (time_base_ <= Rational{0, 1}) {
    throw std::invalid_argument{"media time base must be strictly positive"};
  }
}

MediaTime MediaTime::zero() {
  return MediaTime{0, Rational{1, 1}};
}

MediaTime MediaTime::from_frame(FrameNumber frame, FrameRate rate) {
  const Rational frames_per_second = rate.rational();
  return MediaTime{frame.value(),
                   Rational{frames_per_second.denominator(), frames_per_second.numerator()}};
}

Rational MediaTime::seconds() const {
  const WideInt numerator = static_cast<WideInt>(value_) * time_base_.numerator();
  const WideInt denominator = time_base_.denominator();
  const WideInt divisor = gcd_wide(numerator, denominator);

  return Rational{checked_to_int64(numerator / divisor), checked_to_int64(denominator / divisor)};
}

MediaTime::operator double() const noexcept {
  return static_cast<double>(value_) * static_cast<double>(time_base_);
}

FrameNumber frame_at(MediaTime time, FrameRate rate, RoundingMode rounding) {
  const Rational time_base = time.time_base();
  const Rational frames_per_second = rate.rational();

  const WideInt numerator =
      checked_multiply(checked_multiply(time.value(), time_base.numerator()),
                       frames_per_second.numerator());
  const WideInt denominator =
      checked_multiply(time_base.denominator(), frames_per_second.denominator());

  return rounded_frame(numerator, denominator, rounding);
}

MediaTime operator+(MediaTime lhs, MediaTime rhs) {
  const Rational seconds = combine_seconds(lhs, rhs, false);
  return MediaTime{seconds.numerator(), Rational{1, seconds.denominator()}};
}

MediaTime operator-(MediaTime lhs, MediaTime rhs) {
  const Rational seconds = combine_seconds(lhs, rhs, true);
  return MediaTime{seconds.numerator(), Rational{1, seconds.denominator()}};
}

bool operator==(MediaTime lhs, MediaTime rhs) {
  return compare_media_times(lhs, rhs) == 0;
}

bool operator!=(MediaTime lhs, MediaTime rhs) {
  return compare_media_times(lhs, rhs) != 0;
}

bool operator<(MediaTime lhs, MediaTime rhs) {
  return compare_media_times(lhs, rhs) < 0;
}

bool operator>(MediaTime lhs, MediaTime rhs) {
  return compare_media_times(lhs, rhs) > 0;
}

bool operator<=(MediaTime lhs, MediaTime rhs) {
  return compare_media_times(lhs, rhs) <= 0;
}

bool operator>=(MediaTime lhs, MediaTime rhs) {
  return compare_media_times(lhs, rhs) >= 0;
}

} // namespace pulsar::domain
