#pragma once

#include <pulsar/application/audio/pulse_map.hpp>

#include <cstddef>
#include <vector>

namespace pulsar::application {

enum class SceneCardLabel { Quiet, Build, Sustained, Impact };

struct SceneCard {
  double start_seconds{0.0};
  double end_seconds{0.0};
  float average_energy{0.0F};
  float maximum_peak{0.0F};
  std::size_t pulse_count{0};
  SceneCardLabel label{SceneCardLabel::Quiet};
};

// Turns Pulse Map points into stable editorial regions. This is intentionally
// read-only: it suggests structure without changing the user's timeline.
[[nodiscard]] std::vector<SceneCard> build_scene_cards(const PulseMap& pulse_map,
                                                       double minimum_duration_seconds = 1.0);

} // namespace pulsar::application
