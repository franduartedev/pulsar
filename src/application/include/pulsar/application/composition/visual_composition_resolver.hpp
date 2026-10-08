#pragma once

#include <pulsar/application/timeline/timeline_clip_resolver.hpp>

#include <vector>

namespace pulsar::application {

struct ResolvedVisualLayer {
  domain::TrackId track_id;
  // Gaps, end-of-timeline and missing media remain explicit. Only the primary
  // track may retain an embedded audio binding; auxiliary layers are visual-only.
  TimelineResolveOutcome source;
};

struct ResolvedVisualComposition {
  domain::MediaTime timeline_position;
  std::vector<ResolvedVisualLayer> layers; // bottom-to-top, including gaps
};

// Pure editorial resolution. No decoder/frame ownership or display timestamps.
[[nodiscard]] ResolvedVisualComposition resolve_visual_composition(
    const ProjectSnapshot& snapshot, domain::MediaTime timeline_position);

} // namespace pulsar::application
