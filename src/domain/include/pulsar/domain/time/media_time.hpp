#pragma once

#include <pulsar/domain/time/frame_number.hpp>
#include <pulsar/domain/time/frame_rate.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <cstdint>

namespace pulsar::domain {

enum class RoundingMode {
  Floor,
  Ceil,
  Nearest,
};

class MediaTime {
public:
  MediaTime(std::int64_t value, Rational time_base);

  [[nodiscard]] static MediaTime zero();
  [[nodiscard]] static MediaTime from_frame(FrameNumber frame, FrameRate rate);

  [[nodiscard]] constexpr std::int64_t value() const noexcept {
    return value_;
  }

  [[nodiscard]] constexpr Rational time_base() const noexcept {
    return time_base_;
  }

  [[nodiscard]] Rational seconds() const;
  [[nodiscard]] explicit operator double() const noexcept;

private:
  std::int64_t value_;
  Rational time_base_;
};

[[nodiscard]] FrameNumber frame_at(MediaTime time, FrameRate rate, RoundingMode rounding);

[[nodiscard]] bool operator==(MediaTime lhs, MediaTime rhs);
[[nodiscard]] bool operator!=(MediaTime lhs, MediaTime rhs);
[[nodiscard]] bool operator<(MediaTime lhs, MediaTime rhs);
[[nodiscard]] bool operator>(MediaTime lhs, MediaTime rhs);
[[nodiscard]] bool operator<=(MediaTime lhs, MediaTime rhs);
[[nodiscard]] bool operator>=(MediaTime lhs, MediaTime rhs);

} // namespace pulsar::domain
