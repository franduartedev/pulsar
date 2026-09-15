#include <pulsar/domain/timeline/clip.hpp>

#include <stdexcept>

namespace pulsar::domain {

Clip::Clip(ClipId id, MediaId media_id, TimeRange source_range, MediaTime timeline_start)
    : id_{id}, media_id_{media_id}, source_range_{source_range},
      timeline_start_{timeline_start} {
  if (source_range_.duration() <= MediaTime::zero()) {
    throw std::invalid_argument{"clip source range duration must be positive"};
  }
  if (timeline_start_ < MediaTime::zero()) {
    throw std::invalid_argument{"clip timeline start cannot be negative"};
  }
  if (source_range_.start() < MediaTime::zero()) {
    throw std::invalid_argument{"clip source range start cannot be negative"};
  }
}

MediaTime Clip::duration() const {
  return source_range_.duration();
}

TimeRange Clip::timeline_range() const {
  return TimeRange{timeline_start_, timeline_start_ + duration()};
}

} // namespace pulsar::domain
