#pragma once

#include <cmath>
#include <stdexcept>
#include <utility>

namespace pulsar::desktop {

// Viewer pixels are ephemeral input. The returned values are deltas in project
// canvas fractions and are independent of letterbox offsets and widget size.
inline std::pair<double, double> viewer_delta_to_canvas(double delta_x, double delta_y,
                                                         double painted_width,
                                                         double painted_height) {
  if (!std::isfinite(delta_x) || !std::isfinite(delta_y) || !std::isfinite(painted_width) ||
      !std::isfinite(painted_height) || painted_width <= 0.0 || painted_height <= 0.0) {
    throw std::invalid_argument{"invalid Viewer transform geometry"};
  }
  return {delta_x / painted_width, delta_y / painted_height};
}

} // namespace pulsar::desktop
