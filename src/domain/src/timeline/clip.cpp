#include <pulsar/domain/timeline/clip.hpp>

#include <cmath>
#include <stdexcept>

namespace pulsar::domain {

Clip::Clip(ClipId id, MediaId media_id, TimeRange source_range, MediaTime timeline_start,
           double audio_gain_db, bool muted, MediaTime fade_in, MediaTime fade_out,
           VisualTransform visual_transform, DistantMusicSettings distant_music,
           TelephoneSettings telephone, UnderwaterSettings underwater)
    : id_{id}, media_id_{media_id}, source_range_{source_range},
      timeline_start_{timeline_start}, audio_gain_db_{audio_gain_db}, muted_{muted},
      fade_in_{fade_in}, fade_out_{fade_out}, visual_transform_{visual_transform},
      distant_music_{distant_music}, telephone_{telephone}, underwater_{underwater} {
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
  validate_visual_transform(visual_transform_);
  distant_music_.validate();
  telephone_.validate();
  underwater_.validate();
}

void validate_visual_transform(const VisualTransform& value) {
  if (!std::isfinite(value.position_x) || !std::isfinite(value.position_y) ||
      !std::isfinite(value.scale) || !std::isfinite(value.rotation_degrees) ||
      !std::isfinite(value.opacity) || value.position_x < -10.0 || value.position_x > 10.0 ||
      value.position_y < -10.0 || value.position_y > 10.0 || value.scale < 0.01 ||
      value.scale > 100.0 || value.rotation_degrees < -36000.0 ||
      value.rotation_degrees > 36000.0 || value.opacity < 0.0 || value.opacity > 1.0) {
    throw std::invalid_argument{"visual transform is non-finite or outside supported range"};
  }
}

MediaTime Clip::duration() const {
  return source_range_.duration();
}

TimeRange Clip::timeline_range() const {
  return TimeRange{timeline_start_, timeline_start_ + duration()};
}

} // namespace pulsar::domain
