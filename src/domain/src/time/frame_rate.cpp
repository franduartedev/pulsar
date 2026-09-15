#include <pulsar/domain/time/frame_rate.hpp>

#include <stdexcept>

namespace pulsar::domain {

FrameRate::FrameRate(std::int64_t numerator, std::int64_t denominator)
    : FrameRate{Rational{numerator, denominator}} {}

FrameRate::FrameRate(Rational rate) : rate_{rate} {
  if (rate_ <= Rational{0, 1}) {
    throw std::invalid_argument{"frame rate must be strictly positive"};
  }
}

FrameRate::operator double() const noexcept {
  return static_cast<double>(rate_);
}

} // namespace pulsar::domain
