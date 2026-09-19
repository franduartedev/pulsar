#include <pulsar/domain/timeline/clip.hpp>

#include <stdexcept>
#include <cmath>

namespace pulsar::domain {

Clip::Clip(ClipId id, MediaId media_id, TimeRange source_range, MediaTime timeline_start,
           double audio_gain_db, bool muted)
    : id_{id}, media_id_{media_id}, source_range_{source_range},
      timeline_start_{timeline_start}, audio_gain_db_{audio_gain_db}, muted_{muted} {
  if (source_range_.duration() <= MediaTime::zero()) {
    throw std::invalid_argument{"clip source range duration must be positive"};
  }
  if (timeline_start_ < MediaTime::zero()) {
    throw std::invalid_argument{"clip timeline start cannot be negative"};
  }
  if (source_range_.start() < MediaTime::zero()) {
    throw std::invalid_argument{"clip source range start cannot be negative"};
  }
  if (!std::isfinite(audio_gain_db_) ||
      audio_gain_db_ < minimum_audio_gain_db || audio_gain_db_ > maximum_audio_gain_db) {
    throw std::invalid_argument{"clip audio gain must be finite and within supported range"};
  }
}

MediaTime Clip::duration() const {
  return source_range_.duration();
}

TimeRange Clip::timeline_range() const {
  return TimeRange{timeline_start_, timeline_start_ + duration()};
}

} // namespace pulsar::domain
