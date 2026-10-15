#pragma once

namespace pulsar::domain {

// Renderer-independent visual transform. Position is an offset measured in
// project-canvas widths/heights; scale is uniform around the canvas centre and
// rotation is clockwise in degrees. Opacity is source-over coverage [0, 1].
struct VisualTransform {
  double position_x{0.0};
  double position_y{0.0};
  double scale{1.0};
  double rotation_degrees{0.0};
  double opacity{1.0};

  [[nodiscard]] bool is_default() const noexcept {
    return position_x == 0.0 && position_y == 0.0 && scale == 1.0 &&
           rotation_degrees == 0.0 && opacity == 1.0;
  }
  bool operator==(const VisualTransform&) const noexcept = default;
};

void validate_visual_transform(const VisualTransform& transform);

} // namespace pulsar::domain
