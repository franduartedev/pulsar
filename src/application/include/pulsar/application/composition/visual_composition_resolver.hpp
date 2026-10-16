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

// True when the direct single-source playback path cannot faithfully represent
// the project. Multi-track stacks, transitions, and any non-default visual
// transform must use the shared composition renderer even when there is only
// one visual track.
[[nodiscard]] bool requires_visual_composition(const ProjectSnapshot& snapshot) noexcept;

// Pure editorial resolution. No decoder/frame ownership or display timestamps.
[[nodiscard]] ResolvedVisualComposition resolve_visual_composition(
    const ProjectSnapshot& snapshot, domain::MediaTime timeline_position);

} // namespace pulsar::application
