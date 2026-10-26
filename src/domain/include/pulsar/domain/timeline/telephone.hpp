#pragma once

#include <cmath>
#include <pulsar/domain/timeline/keyframe.hpp>
#include <stdexcept>
#include <vector>

namespace pulsar::domain {

struct TelephoneSettings {
  bool enabled{false};
  double mix{1.0};
  double drive{0.12};
  std::vector<ScalarKeyframe> mix_keyframes;

  void validate() const {
    if (!std::isfinite(mix) || mix < 0.0 || mix > 1.0 || !std::isfinite(drive) ||
        drive < 0.0 || drive > 1.0 || mix_keyframes.size() > 128) {
      throw std::invalid_argument{"invalid Telephone settings"};
    }
    double previous_time = -1.0;
    for (const auto& keyframe : mix_keyframes) {
      keyframe.validate();
      if (keyframe.time_seconds <= previous_time || keyframe.value < 0.0 || keyframe.value > 1.0) {
        throw std::invalid_argument{"Telephone mix keyframes must be ordered and bounded"};
      }
      previous_time = keyframe.time_seconds;
    }
  }

  friend bool operator==(const TelephoneSettings&, const TelephoneSettings&) = default;
};

} // namespace pulsar::domain
