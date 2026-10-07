#pragma once

#include <algorithm>
#include <cmath>

namespace pulsar::desktop {

constexpr double kMinTimelineZoom = 1.0;          // whole timeline in view
constexpr double kMaxTimelineZoom = 10'000'000.0; // matches the zoom slider range

// Presentation-only math for cursor-anchored zoom. The logical viewport limits stay
// exact MediaTime in the controller; this only picks the next (zoom, scroll) pair.
struct ZoomView {
  double zoom{1.0};
  double scroll{0.0};  // 0..1 fraction of the scrollable span (matches timelineScroll)
};

// The time under `anchor` (0..1 across the viewport) stays under the same pixel.
// `total` is the timeline length in seconds.
[[nodiscard]] inline ZoomView anchored_zoom(double total, ZoomView current, double factor,
                                            double anchor) {
  if (!(total > 0.0) || !std::isfinite(factor) || !(factor > 0.0)) return current;
  anchor = std::clamp(std::isfinite(anchor) ? anchor : 0.5, 0.0, 1.0);
  const double zoom = std::clamp(current.zoom * factor, kMinTimelineZoom, kMaxTimelineZoom);
  if (zoom <= kMinTimelineZoom) return ZoomView{kMinTimelineZoom, 0.0};
  const double old_duration = total / std::max(current.zoom, kMinTimelineZoom);
  const double old_start = (total - old_duration) * std::clamp(current.scroll, 0.0, 1.0);
  const double anchor_time = old_start + anchor * old_duration;
  const double duration = total / zoom;
  const double span = total - duration;
  if (!(span > 0.0)) return ZoomView{zoom, 0.0};
  const double start = std::clamp(anchor_time - anchor * duration, 0.0, span);
  return ZoomView{zoom, start / span};
}

} // namespace pulsar::desktop
