#include <pulsar/application/composition/composition_plan.hpp>
#include <pulsar/application/composition/visual_composition_resolver.hpp>

#include <utility>

namespace pulsar::application {

CompositionPlan evaluate_composition_plan(const ProjectSnapshot& snapshot,
                                          const VisualRenderContext& context,
                                          domain::MediaTime timeline_position) {
  if (timeline_position < domain::MediaTime::zero()) {
    throw std::invalid_argument{"composition position cannot be negative"};
  }
  if (context.width <= 0 || context.height <= 0) {
    throw std::invalid_argument{"composition plan requires a valid render size"};
  }
  const auto resolved = resolve_visual_composition(snapshot, timeline_position);
  CompositionPlan plan{timeline_position, context.width, context.height, {}};
  plan.layers.reserve(resolved.layers.size());
  for (const auto& layer : resolved.layers) {
    if (const auto* clip = std::get_if<ResolvedClip>(&layer.source)) {
      plan.layers.push_back(VisualLayerPlan{layer.track_id, clip->clip_id, clip->media_id,
                                             clip->source_time, clip->visual_transform,
                                             clip->source_locator, clip->video_stream_index});
    } else if (std::holds_alternative<MissingClipMedia>(layer.source)) {
      throw CompositionPlanError{CompositionPlanErrorCode::MissingMedia,
                                 "offline video clip references missing media"};
    } else if (std::holds_alternative<TimelineEnded>(layer.source)) {
      throw CompositionPlanError{CompositionPlanErrorCode::EndedBeforePlan,
                                 "timeline ended before the composition plan"};
    }
    // TimelineGap: transparent, omitted. Lower layers show through.
  }
  return plan;
}

} // namespace pulsar::application
