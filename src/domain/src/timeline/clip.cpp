#include <pulsar/domain/timeline/clip.hpp>

#include <stdexcept>
#include <cmath>

namespace pulsar::domain {

Clip::Clip(ClipId id, MediaId media_id, TimeRange source_range, MediaTime timeline_start,
           double audio_gain_db, bool muted, MediaTime fade_in, MediaTime fade_out)
    : id_{id}, media_id_{media_id}, source_range_{source_range},
      timeline_start_{timeline_start}, audio_gain_db_{audio_gain_db}, muted_{muted},
      fade_in_{fade_in}, fade_out_{fade_out} {
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
  const MediaTime maximum_fade{maximum_fade_seconds, Rational{1, 1}};
  if (fade_in_ < MediaTime::zero() || fade_out_ < MediaTime::zero() || fade_in_ > maximum_fade ||
      fade_out_ > maximum_fade) {
    throw std::invalid_argument{"clip fades must be between zero and the supported maximum"};
  }
}

MediaTime Clip::duration() const {
  return source_range_.duration();
}

TimeRange Clip::timeline_range() const {
  return TimeRange{timeline_start_, timeline_start_ + duration()};
}

} // namespace pulsar::domain
