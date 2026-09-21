#pragma once

#include <pulsar/application/waveform/waveform_cache.hpp>

#include <cstdint>
#include <vector>

namespace pulsar::application {

// A deterministic, UI-independent raster plan. Coordinates are relative to the
// supplied content rectangle and are rounded from absolute source intervals;
// callers never accumulate a floating point bucket width.
struct WaveformSegment {
  std::uint32_t column{0};
  double top{0.0};
  double bottom{0.0};
};

struct WaveformGeometry {
  std::vector<WaveformSegment> segments;
  std::vector<std::uint32_t> pending_columns;
  std::vector<std::uint32_t> unavailable_columns;
  std::uint32_t width{0};
};

struct WaveformRenderPolicy {
  // Used with the source-range query to select A7 LOD; never alters timing.
  double density{1.0};
  bool combine_channels{false};
  std::uint32_t raster_width{0};
  // Pixel rectangle expressed as usable height after clip labels and padding.
  double content_height{0.0};
};

// Prepares at most one extrema segment per occupied pixel column and displayed
// channel. Bucket extrema are merged, never stride sampled. A partial slice's
// unavailable source prefix is distinct from its pending suffix.
[[nodiscard]] WaveformGeometry prepare_waveform_geometry(
    const WaveformSlice& slice, int sample_rate, const WaveformRenderPolicy& policy);
// Boundary refinements replace the base LOD's contribution in every raster
// column touched by their requested intervals. Each refined slice is queried
// at a finer A7 LOD by the caller.
[[nodiscard]] WaveformGeometry prepare_waveform_geometry(
    const WaveformSlice& slice, const std::vector<WaveformSlice>& boundary_refinements,
    int sample_rate, const WaveformRenderPolicy& policy);

} // namespace pulsar::application
