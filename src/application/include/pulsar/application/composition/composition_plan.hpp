#pragma once

#include <pulsar/application/composition/visual_render_context.hpp>
#include <pulsar/domain/timeline/visual_transform.hpp>

#include <filesystem>
#include <stdexcept>
#include <vector>

namespace pulsar::application {

// Renderer-neutral evaluated layer. The renderer opens `source_locator`,
// seeks `source_time`, fits the decoded picture preserving aspect (E1
// fit_geometry) and then applies `transform` in canvas space. No project,
// UI, Qt or Viewer state crosses this boundary.
struct VisualLayerPlan {
  domain::TrackId track_id;
  domain::ClipId clip_id;
  domain::MediaId media_id;
  domain::MediaTime source_time;
  domain::VisualTransform transform;
  std::filesystem::path source_locator;
  int video_stream_index{-1};
};

// Bottom-to-top active layers only. Gaps are omitted (transparent); an active
// unavailable visual is a deterministic evaluation failure shared by preview
// and export. Inactive unavailable media never appears here.
struct CompositionPlan {
  domain::MediaTime timeline_time{0, {1, 1}};
  int width{0};
  int height{0};
  std::vector<VisualLayerPlan> layers;
};

enum class CompositionPlanErrorCode { MissingMedia, EndedBeforePlan };

class CompositionPlanError final : public std::runtime_error {
public:
  CompositionPlanError(CompositionPlanErrorCode code, std::string message)
      : std::runtime_error{std::move(message)}, code_{code} {}
  [[nodiscard]] CompositionPlanErrorCode code() const noexcept { return code_; }

private:
  CompositionPlanErrorCode code_;
};

// Pure editorial evaluation: ProjectSnapshot + VisualRenderContext + t.
// Throws CompositionPlanError on active missing media / ended timeline,
// std::invalid_argument on bad position or stack.
[[nodiscard]] CompositionPlan evaluate_composition_plan(const ProjectSnapshot& snapshot,
                                                        const VisualRenderContext& context,
                                                        domain::MediaTime timeline_position);

} // namespace pulsar::application
