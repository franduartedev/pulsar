#pragma once

#include <pulsar/domain/timeline/keyframe.hpp>

#include <vector>

namespace pulsar::domain {

// How a source picture meets a canvas with a different aspect ratio:
//   Contain: the whole picture is visible, bars fill the remaining canvas.
//   Fill:    the picture covers the canvas, cropping the overflow.
// The fit is editorial, per clip, and never modifies the source media.
enum class VisualFit { Contain, Fill };

// A complete canvas transform at a clip-local time. Fit remains a clip-level
// editorial decision: animating it would create discontinuous source framing.
struct VisualTransformKeyframe {
  double time_seconds{0.0};
  double position_x{0.0};
  double position_y{0.0};
  double scale{1.0};
  double rotation_degrees{0.0};
  double opacity{1.0};
  KeyframeInterpolation interpolation{KeyframeInterpolation::Linear};

  bool operator==(const VisualTransformKeyframe&) const noexcept = default;
};

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
  std::vector<VisualTransformKeyframe> keyframes;

  [[nodiscard]] bool is_default() const noexcept {
    return position_x == 0.0 && position_y == 0.0 && scale == 1.0 &&
            rotation_degrees == 0.0 && opacity == 1.0 && fit == VisualFit::Contain && keyframes.empty();
  }
  bool operator==(const VisualTransform&) const noexcept = default;
};

void validate_visual_transform(const VisualTransform& transform);
[[nodiscard]] VisualTransform evaluate_visual_transform(const VisualTransform& transform,
                                                         double clip_time_seconds);

} // namespace pulsar::domain
