#pragma once

#include <cmath>
#include <stdexcept>

namespace pulsar::domain {

enum class KeyframeInterpolation { Hold, Linear, Smooth };

struct ScalarKeyframe {
  double time_seconds{0.0};
  double value{0.0};
  KeyframeInterpolation interpolation{KeyframeInterpolation::Linear};

  void validate() const {
    if (!std::isfinite(time_seconds) || time_seconds < 0.0 || time_seconds > 60.0 ||
        !std::isfinite(value)) {
      throw std::invalid_argument{"invalid scalar keyframe"};
    }
  }

  friend bool operator==(const ScalarKeyframe&, const ScalarKeyframe&) = default;
};

} // namespace pulsar::domain
