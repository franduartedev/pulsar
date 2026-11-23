#include <pulsar/domain/timeline/clip.hpp>

#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace pulsar::domain {

Clip::Clip(ClipId id, MediaId media_id, TimeRange source_range, MediaTime timeline_start,
           double audio_gain_db, bool muted, MediaTime fade_in, MediaTime fade_out,
           VisualTransform visual_transform, DistantMusicSettings distant_music,
           TelephoneSettings telephone, UnderwaterSettings underwater,
           std::vector<AudioEffectType> audio_effect_order)
    : id_{id}, media_id_{media_id}, source_range_{source_range},
      timeline_start_{timeline_start}, audio_gain_db_{audio_gain_db}, muted_{muted},
      fade_in_{fade_in}, fade_out_{fade_out}, visual_transform_{visual_transform},
      distant_music_{distant_music}, telephone_{telephone}, underwater_{underwater},
      audio_effect_order_{std::move(audio_effect_order)} {
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
  if (audio_effect_order_.empty()) audio_effect_order_ = canonical_audio_effect_order();
  validate_audio_effect_order(audio_effect_order_);
}

void validate_visual_transform(const VisualTransform& value) {
  if (!std::isfinite(value.position_x) || !std::isfinite(value.position_y) ||
      !std::isfinite(value.scale) || !std::isfinite(value.rotation_degrees) ||
      !std::isfinite(value.opacity) || value.position_x < -10.0 || value.position_x > 10.0 ||
      value.position_y < -10.0 || value.position_y > 10.0 || value.scale < 0.01 ||
      value.scale > 100.0 || value.rotation_degrees < -36000.0 ||
      value.rotation_degrees > 36000.0 || value.opacity < 0.0 || value.opacity > 1.0 ||
       (value.fit != VisualFit::Contain && value.fit != VisualFit::Fill)) {
    throw std::invalid_argument{"visual transform is non-finite or outside supported range"};
  }
  if (value.keyframes.size() > 128) {
    throw std::invalid_argument{"too many visual transform keyframes"};
  }
  double previous_time = -1.0;
  for (const auto& keyframe : value.keyframes) {
    if (!std::isfinite(keyframe.time_seconds) || keyframe.time_seconds < 0.0 ||
        keyframe.time_seconds > 60.0 || !std::isfinite(keyframe.position_x) ||
        !std::isfinite(keyframe.position_y) || !std::isfinite(keyframe.scale) ||
        !std::isfinite(keyframe.rotation_degrees) || !std::isfinite(keyframe.opacity) ||
        keyframe.time_seconds <= previous_time || keyframe.position_x < -10.0 ||
        keyframe.position_x > 10.0 || keyframe.position_y < -10.0 || keyframe.position_y > 10.0 ||
        keyframe.scale < 0.01 || keyframe.scale > 100.0 ||
        keyframe.rotation_degrees < -36000.0 || keyframe.rotation_degrees > 36000.0 ||
        keyframe.opacity < 0.0 || keyframe.opacity > 1.0 ||
        (keyframe.interpolation != KeyframeInterpolation::Hold &&
         keyframe.interpolation != KeyframeInterpolation::Linear &&
         keyframe.interpolation != KeyframeInterpolation::Smooth)) {
      throw std::invalid_argument{"visual transform keyframes must be ordered and bounded"};
    }
    previous_time = keyframe.time_seconds;
  }
}

VisualTransform evaluate_visual_transform(const VisualTransform& transform, double clip_time_seconds) {
  validate_visual_transform(transform);
  if (!std::isfinite(clip_time_seconds) || clip_time_seconds < 0.0 || transform.keyframes.empty()) {
    return transform;
  }
  const auto& frames = transform.keyframes;
  const auto make_transform = [&](const VisualTransformKeyframe& frame) {
    return VisualTransform{frame.position_x, frame.position_y, frame.scale, frame.rotation_degrees,
                           frame.opacity, transform.fit};
  };
  if (clip_time_seconds <= frames.front().time_seconds) return make_transform(frames.front());
  if (clip_time_seconds >= frames.back().time_seconds) return make_transform(frames.back());
  const auto right = std::upper_bound(frames.begin(), frames.end(), clip_time_seconds,
      [](double time, const VisualTransformKeyframe& frame) { return time < frame.time_seconds; });
  const auto& from = *(right - 1);
  const auto& to = *right;
  double progress = (clip_time_seconds - from.time_seconds) / (to.time_seconds - from.time_seconds);
  if (from.interpolation == KeyframeInterpolation::Hold) progress = 0.0;
  if (from.interpolation == KeyframeInterpolation::Smooth) progress = progress * progress * (3.0 - 2.0 * progress);
  const auto blend = [progress](double a, double b) { return a + (b - a) * progress; };
  return VisualTransform{blend(from.position_x, to.position_x), blend(from.position_y, to.position_y),
                         blend(from.scale, to.scale), blend(from.rotation_degrees, to.rotation_degrees),
                         blend(from.opacity, to.opacity), transform.fit};
}

MediaTime Clip::duration() const {
  return source_range_.duration();
}

TimeRange Clip::timeline_range() const {
  return TimeRange{timeline_start_, timeline_start_ + duration()};
}

} // namespace pulsar::domain
