#include <pulsar/application/composition/visual_composition_resolver.hpp>

#include <stdexcept>
#include <utility>

namespace pulsar::application {

bool requires_visual_composition(const ProjectSnapshot& snapshot) noexcept {
  if (snapshot.visual_track_ids.size() > 1 || !snapshot.transitions.empty()) return true;
  for (const auto track_id : snapshot.visual_track_ids) {
    const auto* track = snapshot.timeline.find_track(track_id);
    if (track == nullptr) continue;
    for (const auto& clip : track->clips()) {
      if (!clip.visual_transform().is_default()) return true;
    }
  }
  return false;
}

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
