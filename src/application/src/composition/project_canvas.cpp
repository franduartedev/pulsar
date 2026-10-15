#include <pulsar/application/composition/project_canvas.hpp>

namespace pulsar::application {
std::optional<ProjectCanvas> legacy_project_canvas(const ProjectSnapshot& snapshot) {
  const auto* primary = snapshot.timeline.find_track(snapshot.video_track_id);
  if (!primary) return std::nullopt;
  const auto* master = pick_master_asset(primary->clips(), [&](const domain::Clip& clip) -> const MediaAsset* {
    for (const auto& asset : snapshot.assets) if (asset.id() == clip.media_id()) return &asset;
    return nullptr;
  });
  if (!master || !master->metadata().nominal_frame_rate) return std::nullopt;
  const auto [width, height] = master_canvas_size(*master);
  return ProjectCanvas{width, height, *master->metadata().nominal_frame_rate, master->is_still()};
}

std::optional<ProjectCanvas> effective_project_canvas(const ProjectSnapshot& snapshot) {
  return snapshot.canvas ? snapshot.canvas : legacy_project_canvas(snapshot);
}
} // namespace pulsar::application
