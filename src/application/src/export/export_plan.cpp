#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/application/composition/project_canvas.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>

#include <limits>
#include <unordered_map>
#include <utility>

namespace pulsar::application::exporting {
namespace {

domain::FrameRate audio_rate() { return domain::FrameRate{48'000, 1}; }

[[nodiscard]] std::int64_t checked_frame_index(domain::MediaTime time,
                                               domain::FrameRate rate,
                                               domain::RoundingMode rounding) {
  if (time < domain::MediaTime::zero()) {
    throw ExportPlanError{ExportPlanErrorCode::InvalidProject,
                          "export time cannot be negative"};
  }
  try {
    return domain::frame_at(time, rate, rounding).value();
  } catch (const std::overflow_error&) {
    throw ExportPlanError{ExportPlanErrorCode::Overflow, "time-to-index conversion overflows"};
  } catch (const std::invalid_argument&) {
    throw ExportPlanError{ExportPlanErrorCode::InvalidFrameRate,
                          "frame rate is invalid"};
  }
}

[[nodiscard]] const MediaAsset& find_asset(
    const std::unordered_map<std::uint64_t, const MediaAsset*>& assets, domain::MediaId id) {
  const auto found = assets.find(id.value());
  if (found == assets.end()) {
    throw ExportPlanError{ExportPlanErrorCode::MissingMedia,
                          "timeline references an unknown media asset"};
  }
  if (found->second->availability() != MediaAvailability::Available) {
    throw ExportPlanError{ExportPlanErrorCode::MissingMedia,
                          "timeline references unavailable media"};
  }
  return *found->second;
}

} // namespace

ExportProfile ExportProfile::beta_v01() noexcept { return {}; }

std::int64_t output_frame_index_at(domain::MediaTime time, domain::FrameRate frame_rate,
                                   domain::RoundingMode rounding) {
  return checked_frame_index(time, frame_rate, rounding);
}

domain::MediaTime output_frame_time(domain::FrameNumber frame, domain::FrameRate frame_rate) {
  if (frame.value() < 0) {
    throw ExportPlanError{ExportPlanErrorCode::InvalidProject, "frame index cannot be negative"};
  }
  try {
    return domain::MediaTime::from_frame(frame, frame_rate);
  } catch (const std::overflow_error&) {
    throw ExportPlanError{ExportPlanErrorCode::Overflow, "frame-to-time conversion overflows"};
  } catch (const std::invalid_argument&) {
    throw ExportPlanError{ExportPlanErrorCode::InvalidFrameRate,
                          "frame rate is invalid"};
  }
}

std::int64_t output_frame_count(domain::MediaTime duration, domain::FrameRate frame_rate) {
  return checked_frame_index(duration, frame_rate, domain::RoundingMode::Ceil);
}

std::int64_t audio_sample_index_at(domain::MediaTime time, domain::RoundingMode rounding) {
  return checked_frame_index(time, audio_rate(), rounding);
}

domain::MediaTime audio_sample_time(std::int64_t sample_index) {
  return output_frame_time(domain::FrameNumber{sample_index}, audio_rate());
}

std::int64_t audio_sample_count(domain::MediaTime duration) {
  return output_frame_count(duration, audio_rate());
}

ExportPlan ExportPlan::build(const ProjectSnapshot& input, ExportProfile profile) {
  if (input.timeline.duration() <= domain::MediaTime::zero()) {
    throw ExportPlanError{ExportPlanErrorCode::EmptyTimeline, "cannot export an empty timeline"};
  }

  std::unordered_map<std::uint64_t, const MediaAsset*> assets;
  assets.reserve(input.assets.size());
  for (const auto& asset : input.assets) {
    if (!assets.emplace(asset.id().value(), &asset).second) {
      throw ExportPlanError{ExportPlanErrorCode::InvalidProject, "duplicate media asset id"};
    }
  }

  for (const auto& track : input.timeline.tracks()) {
    for (const auto& clip : track.clips()) {
      (void)find_asset(assets, clip.media_id());
    }
  }

  const auto* video_track = input.timeline.find_track(input.video_track_id);
  if (video_track == nullptr || video_track->kind() != domain::TrackKind::Video) {
    throw ExportPlanError{ExportPlanErrorCode::NoVideoMaster,
                          "project has no designated video track"};
  }
  try {
    validate_visual_track_ids(input.timeline, input.video_track_id, input.visual_track_ids);
  } catch (const std::invalid_argument& error) {
    throw ExportPlanError{ExportPlanErrorCode::InvalidProject, error.what()};
  }

  const MediaAsset* master = pick_master_asset(
      video_track->clips(),
      [&](const domain::Clip& clip) -> const MediaAsset* { return &find_asset(assets, clip.media_id()); });
  bool has_visual_content = false;
  for (const auto id : input.visual_track_ids) {
    if (!input.timeline.find_track(id)->clips().empty()) has_visual_content = true;
  }
  if (!has_visual_content) {
    throw ExportPlanError{ExportPlanErrorCode::NoVideoMaster,
                          "project has no visual content"};
  }
  if (master && !effective_project_canvas(input)) {
    throw ExportPlanError{ExportPlanErrorCode::InvalidFrameRate,
                          "video master has no nominal frame rate"};
  }
  VisualRenderContext visual = [&] {
    try {
      return VisualRenderContext::build(input, profile.max_output_width(), profile.max_output_height());
    } catch (const std::invalid_argument& error) {
      throw ExportPlanError{ExportPlanErrorCode::NoVideoMaster, error.what()};
    }
  }();
  const auto frame_rate = visual.frame_rate;
  const auto width = visual.width;
  const auto height = visual.height;
  const auto editorial_duration = input.timeline.duration();
  const auto frame_count = output_frame_count(editorial_duration, frame_rate);
  if (frame_count <= 0) {
    throw ExportPlanError{ExportPlanErrorCode::EmptyTimeline, "timeline has no output frames"};
  }
  const auto output_duration = output_frame_time(domain::FrameNumber{frame_count}, frame_rate);
  const auto samples = exporting::audio_sample_count(output_duration);

  return ExportPlan{input, profile, master ? std::optional{master->id()} : std::nullopt, frame_rate, editorial_duration,
                    output_duration, frame_count, width, height, samples};
}

} // namespace pulsar::application::exporting
