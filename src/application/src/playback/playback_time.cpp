#include <pulsar/application/playback/playback_time.hpp>

#include <stdexcept>

namespace pulsar::application {

PlaybackPosition::PlaybackPosition(domain::MediaTime value) : value_{value} {
  if (value_ < domain::MediaTime::zero()) {
    throw std::invalid_argument{"playback position cannot be negative"};
  }
}

PlaybackPosition PlaybackPosition::zero() {
  return PlaybackPosition{domain::MediaTime::zero()};
}

MonotonicInstant::MonotonicInstant(domain::MediaTime value) : value_{value} {
  if (value_ < domain::MediaTime::zero()) {
    throw std::invalid_argument{"monotonic instant cannot be negative"};
  }
}

MonotonicInstant MonotonicInstant::zero() {
  return MonotonicInstant{domain::MediaTime::zero()};
}

domain::MediaTime MonotonicInstant::elapsed_since(MonotonicInstant earlier) const {
  if (value_ < earlier.value_) {
    throw std::invalid_argument{"monotonic instant moved backwards"};
  }

  return value_ - earlier.value_;
}

bool operator==(PlaybackPosition lhs, PlaybackPosition rhs) {
  return lhs.value() == rhs.value();
}

bool operator!=(PlaybackPosition lhs, PlaybackPosition rhs) {
  return !(lhs == rhs);
}

bool operator==(StreamTimestamp lhs, StreamTimestamp rhs) {
  return lhs.value() == rhs.value();
}

bool operator!=(StreamTimestamp lhs, StreamTimestamp rhs) {
  return !(lhs == rhs);
}

bool operator==(MonotonicInstant lhs, MonotonicInstant rhs) {
  return lhs.value() == rhs.value();
}

bool operator!=(MonotonicInstant lhs, MonotonicInstant rhs) {
  return !(lhs == rhs);
}

} // namespace pulsar::application
