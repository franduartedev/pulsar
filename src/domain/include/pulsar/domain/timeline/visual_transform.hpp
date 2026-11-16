#pragma once

namespace pulsar::domain {

// How a source picture meets a canvas with a different aspect ratio:
//   Contain: the whole picture is visible, bars fill the remaining canvas.
//   Fill:    the picture covers the canvas, cropping the overflow.
// The fit is editorial, per clip, and never modifies the source media.
enum class VisualFit { Contain, Fill };

// Renderer-independent visual transform. Position is an offset measured in
// project-canvas widths/heights; scale is uniform around the canvas centre and
// rotation is clockwise in degrees. Opacity is source-over coverage [0, 1].
struct VisualTransform {
  double position_x{0.0};
  double position_y{0.0};
  double scale{1.0};
  double rotation_degrees{0.0};
  double opacity{1.0};
  VisualFit fit{VisualFit::Contain};

  [[nodiscard]] bool is_default() const noexcept {
    return position_x == 0.0 && position_y == 0.0 && scale == 1.0 &&
           rotation_degrees == 0.0 && opacity == 1.0 && fit == VisualFit::Contain;
  }
  bool operator==(const VisualTransform&) const noexcept = default;
};

void validate_visual_transform(const VisualTransform& transform);

} // namespace pulsar::domain
