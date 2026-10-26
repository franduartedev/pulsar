#pragma once

#include <cmath>
#include <pulsar/domain/timeline/keyframe.hpp>
#include <stdexcept>
#include <vector>

namespace pulsar::domain {

// Underwater intentionally stays compact: depth controls the muffling cutoff
// and mix controls how much of the submerged signal replaces the dry source.
struct UnderwaterSettings {
  bool enabled{false};
  double mix{0.72};
  double depth{0.68};
  std::vector<ScalarKeyframe> depth_keyframes;

  void validate() const {
    if (!std::isfinite(mix) || !std::isfinite(depth) || mix < 0.0 || mix > 1.0 ||
        depth < 0.0 || depth > 1.0 || depth_keyframes.size() > 128) {
      throw std::invalid_argument{"invalid Underwater settings"};
    }
    double previous_time = -1.0;
    for (const auto& keyframe : depth_keyframes) {
      keyframe.validate();
      if (keyframe.time_seconds <= previous_time || keyframe.value < 0.0 || keyframe.value > 1.0) {
        throw std::invalid_argument{"Underwater depth keyframes must be ordered and bounded"};
      }
      previous_time = keyframe.time_seconds;
    }
  }

  friend bool operator==(const UnderwaterSettings&, const UnderwaterSettings&) = default;
};

} // namespace pulsar::domain
