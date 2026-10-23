#pragma once

#include <pulsar/domain/timeline/keyframe.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace pulsar::application {

// Evaluates an ordered scalar automation lane without allocating. The cursor
// is monotonic during forward playback, but safely resets when seeking back.
inline float evaluate_scalar_keyframes(const std::vector<domain::ScalarKeyframe>& keyframes,
                                       float time_seconds, std::size_t& cursor) noexcept {
  if (keyframes.empty()) return 0.0F;
  if (cursor >= keyframes.size() || time_seconds < keyframes[cursor].time_seconds) cursor = 0;
  if (time_seconds <= keyframes.front().time_seconds) {
    cursor = 0;
    return static_cast<float>(keyframes.front().value);
  }
  while (cursor + 1 < keyframes.size() &&
         time_seconds > keyframes[cursor + 1].time_seconds) {
    ++cursor;
  }
  if (cursor + 1 == keyframes.size()) return static_cast<float>(keyframes.back().value);
  const auto& left = keyframes[cursor];
  const auto& right = keyframes[cursor + 1];
  if (right.interpolation == domain::KeyframeInterpolation::Hold) {
    return static_cast<float>(left.value);
  }
  const float span = static_cast<float>(right.time_seconds - left.time_seconds);
  const float progress = std::clamp(
      (time_seconds - static_cast<float>(left.time_seconds)) / span, 0.0F, 1.0F);
  const float shaped = right.interpolation == domain::KeyframeInterpolation::Smooth
                           ? progress * progress * (3.0F - 2.0F * progress)
                           : progress;
  return static_cast<float>(left.value) +
         (static_cast<float>(right.value) - static_cast<float>(left.value)) * shaped;
}

} // namespace pulsar::application
