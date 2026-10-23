#pragma once

#include <cmath>
#include <pulsar/domain/timeline/keyframe.hpp>
#include <stdexcept>
#include <vector>

namespace pulsar::domain {

enum class DistantMusicCurve { Smooth, Progressive, Explosive };

struct DistantMusicSettings {
  bool enabled{false};
  double initial_gain_db{-30.0};
  double duration_seconds{4.0};
  double start_cutoff_hz{900.0};
  double end_cutoff_hz{18'000.0};
  double reverb_amount{0.18};
  DistantMusicCurve curve{DistantMusicCurve::Progressive};
  std::vector<ScalarKeyframe> gain_keyframes;

  void validate() const {
    if (!std::isfinite(initial_gain_db) || initial_gain_db < -60.0 || initial_gain_db > 0.0 ||
        !std::isfinite(duration_seconds) || duration_seconds <= 0.0 || duration_seconds > 60.0 ||
        !std::isfinite(start_cutoff_hz) || start_cutoff_hz < 20.0 || start_cutoff_hz > 20'000.0 ||
        !std::isfinite(end_cutoff_hz) || end_cutoff_hz < start_cutoff_hz || end_cutoff_hz > 20'000.0 ||
        !std::isfinite(reverb_amount) || reverb_amount < 0.0 || reverb_amount > 1.0) {
      throw std::invalid_argument{"invalid Distant Music settings"};
    }
    if (gain_keyframes.size() > 128) {
      throw std::invalid_argument{"too many Distant Music gain keyframes"};
    }
    double previous_time = -1.0;
    for (const auto& keyframe : gain_keyframes) {
      keyframe.validate();
      if (keyframe.time_seconds <= previous_time || keyframe.value < -60.0 ||
          keyframe.value > 0.0) {
        throw std::invalid_argument{"Distant Music gain keyframes must be ordered and bounded"};
      }
      previous_time = keyframe.time_seconds;
    }
  }

  friend bool operator==(const DistantMusicSettings&, const DistantMusicSettings&) = default;
};

} // namespace pulsar::domain
