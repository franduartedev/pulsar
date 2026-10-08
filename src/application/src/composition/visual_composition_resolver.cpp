#include <pulsar/application/composition/visual_composition_resolver.hpp>

#include <stdexcept>
#include <utility>

namespace pulsar::application {

ResolvedVisualComposition resolve_visual_composition(
    const ProjectSnapshot& snapshot, domain::MediaTime timeline_position) {
  if (timeline_position < domain::MediaTime::zero()) {
    throw std::invalid_argument{"composition position cannot be negative"};
  }
  validate_visual_track_ids(snapshot.timeline, snapshot.video_track_id, snapshot.visual_track_ids);
  ResolvedVisualComposition result{timeline_position, {}};
  result.layers.reserve(snapshot.visual_track_ids.size());
  for (const auto track_id : snapshot.visual_track_ids) {
    auto source = resolve_clip_at(snapshot, track_id, timeline_position);
    if (auto* clip = std::get_if<ResolvedClip>(&source);
        clip != nullptr && track_id != snapshot.video_track_id) {
      clip->audio.reset();
    }
    result.layers.push_back({track_id, std::move(source)});
  }
  return result;
}

} // namespace pulsar::application
