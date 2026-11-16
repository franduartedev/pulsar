#include <pulsar/application/composition/composition_plan.hpp>
#include <pulsar/application/composition/visual_composition_resolver.hpp>

#include <algorithm>
#include <cctype>
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
  const auto asset_may_have_alpha = [&snapshot](domain::MediaId media_id) {
    const auto asset = std::find_if(snapshot.assets.begin(), snapshot.assets.end(),
                                    [media_id](const auto& item) { return item.id() == media_id; });
    if (asset == snapshot.assets.end() || !asset->is_still()) return false;
    auto extension = asset->source_locator().extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".png";
  };
  const auto resolved = resolve_visual_composition(snapshot, timeline_position);
  CompositionPlan plan{timeline_position, context.width, context.height, {}};
  plan.layers.reserve(resolved.layers.size());
  for (const auto& layer : resolved.layers) {
    if (const auto* clip = std::get_if<ResolvedClip>(&layer.source)) {
      plan.layers.push_back(VisualLayerPlan{layer.track_id, clip->clip_id, clip->media_id,
                                             clip->source_time, clip->visual_transform,
                                             clip->source_locator, clip->video_stream_index,
                                             asset_may_have_alpha(clip->media_id)});
    } else if (std::holds_alternative<MissingClipMedia>(layer.source)) {
      throw CompositionPlanError{CompositionPlanErrorCode::MissingMedia,
                                 "offline video clip references missing media"};
    } else if (std::holds_alternative<TimelineEnded>(layer.source)) {
      throw CompositionPlanError{CompositionPlanErrorCode::EndedBeforePlan,
                                 "timeline ended before the composition plan"};
    }
    // TimelineGap: transparent, omitted. Lower layers show through.
  }

  // A dissolve is evaluated before its edit point: the outgoing clip is still
  // the track's ordinary active layer and the incoming clip is pre-rolled from
  // its source handle, then blended above it. No fake timeline clip is created.
  for (const auto& transition : snapshot.transitions) {
    const auto* outgoing = snapshot.timeline.find_clip(transition.outgoing_clip_id);
    const auto* incoming = snapshot.timeline.find_clip(transition.incoming_clip_id);
    if (outgoing == nullptr || incoming == nullptr) {
      throw std::invalid_argument{"transition references a missing clip"};
    }
    const auto edit = incoming->timeline_range().start();
    const auto start = edit - transition.duration;
    const auto end = edit + transition.duration;
    if (transition.type == domain::TransitionType::DipToBlack) {
      if (timeline_position >= start && timeline_position < edit) {
        const auto outgoing_layer = std::find_if(plan.layers.begin(), plan.layers.end(), [&](const auto& item) {
          return item.clip_id == transition.outgoing_clip_id;
        });
        if (outgoing_layer == plan.layers.end()) throw std::invalid_argument{"transition outgoing clip is not active"};
        const double progress = static_cast<double>(timeline_position - start) /
                                static_cast<double>(transition.duration);
        outgoing_layer->transform.opacity *= 1.0 - std::clamp(progress, 0.0, 1.0);
      } else if (timeline_position >= edit && timeline_position < end) {
        const auto incoming_layer = std::find_if(plan.layers.begin(), plan.layers.end(), [&](const auto& item) {
          return item.clip_id == transition.incoming_clip_id;
        });
        if (incoming_layer == plan.layers.end()) throw std::invalid_argument{"transition incoming clip is not active"};
        const double progress = static_cast<double>(timeline_position - edit) /
                                static_cast<double>(transition.duration);
        incoming_layer->transform.opacity *= std::clamp(progress, 0.0, 1.0);
      }
      continue;
    }
    if (timeline_position < start || timeline_position >= edit) continue;
    const auto asset = std::find_if(snapshot.assets.begin(), snapshot.assets.end(), [&](const auto& item) {
      return item.id() == incoming->media_id();
    });
    if (asset == snapshot.assets.end() || asset->availability() == MediaAvailability::Unavailable) {
      throw CompositionPlanError{CompositionPlanErrorCode::MissingMedia,
                                 "transition references missing incoming media"};
    }
    auto transform = incoming->visual_transform();
    const double progress = static_cast<double>(timeline_position - start) /
                            static_cast<double>(transition.duration);
    transform.opacity *= std::clamp(progress, 0.0, 1.0);
    VisualLayerPlan incoming_plan{transition.track_id, incoming->id(), incoming->media_id(),
        incoming->source_range().start() + (timeline_position - edit), transform,
        asset->source_locator(), asset->video_stream_index(),
        asset_may_have_alpha(incoming->media_id())};
    const auto outgoing_layer = std::find_if(plan.layers.begin(), plan.layers.end(), [&](const auto& item) {
      return item.clip_id == transition.outgoing_clip_id;
    });
    if (outgoing_layer == plan.layers.end()) {
      throw std::invalid_argument{"transition outgoing clip is not active"};
    }
    plan.layers.insert(outgoing_layer + 1, std::move(incoming_plan));
  }
  return plan;
}

} // namespace pulsar::application
