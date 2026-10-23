#pragma once

#include <cmath>
#include <stdexcept>

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

  void validate() const {
    if (!std::isfinite(initial_gain_db) || initial_gain_db < -60.0 || initial_gain_db > 0.0 ||
        !std::isfinite(duration_seconds) || duration_seconds <= 0.0 || duration_seconds > 60.0 ||
        !std::isfinite(start_cutoff_hz) || start_cutoff_hz < 20.0 || start_cutoff_hz > 20'000.0 ||
        !std::isfinite(end_cutoff_hz) || end_cutoff_hz < start_cutoff_hz || end_cutoff_hz > 20'000.0 ||
        !std::isfinite(reverb_amount) || reverb_amount < 0.0 || reverb_amount > 1.0) {
      throw std::invalid_argument{"invalid Distant Music settings"};
    }
  }

  friend bool operator==(const DistantMusicSettings&, const DistantMusicSettings&) = default;
};

} // namespace pulsar::domain
