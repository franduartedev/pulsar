#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/application/composition/title_compositor.hpp>

#include <pulsar/domain/timeline/track_kind.hpp>

#include <limits>
#include <unordered_map>
#include <utility>

namespace pulsar::application::exporting {
namespace {

domain::FrameRate audio_rate() { return domain::FrameRate{48'000, 1}; }

[[nodiscard]] std::int64_t checked_even_dimension(int dimension) {
  if (dimension <= 0) {
    throw ExportPlanError{ExportPlanErrorCode::InvalidDimensions,
                          "video dimensions must be strictly positive"};
  }
  const auto value = static_cast<std::int64_t>(dimension);
  if ((value % 2) == 0) return value;
  if (value == std::numeric_limits<std::int64_t>::max()) {
    throw ExportPlanError{ExportPlanErrorCode::Overflow,
                          "normalized video dimension overflows"};
  }
  return value + 1;
}

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

[[nodiscard]] domain::FrameRate require_frame_rate(const MediaAsset& asset) {
  if (!asset.has_video() || !asset.metadata().nominal_frame_rate.has_value()) {
    throw ExportPlanError{ExportPlanErrorCode::InvalidFrameRate,
                          "video master has no nominal frame rate"};
  }
  try {
    return *asset.metadata().nominal_frame_rate;
  } catch (const std::invalid_argument&) {
    throw ExportPlanError{ExportPlanErrorCode::InvalidFrameRate, "video frame rate is invalid"};
  }
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

  const MediaAsset* master = nullptr;
  for (const auto& clip : video_track->clips()) {
    const auto& asset = find_asset(assets, clip.media_id());
    if (asset.has_video()) {
      master = &asset;
      break;
    }
  }
  if (master == nullptr) {
    throw ExportPlanError{ExportPlanErrorCode::NoVideoMaster,
                          "project has no video asset on its video track"};
  }

  const auto frame_rate = require_frame_rate(*master);
  auto width = checked_even_dimension(master->metadata().width);
  auto height = checked_even_dimension(master->metadata().height);
  if (profile.max_output_width() > 0 && profile.max_output_height() > 0 &&
      (width > profile.max_output_width() || height > profile.max_output_height())) {
    const auto fit = composition::fit_geometry(static_cast<int>(width), static_cast<int>(height),
                                               profile.max_output_width(), profile.max_output_height());
    width = checked_even_dimension(fit.scaled_width);
    height = checked_even_dimension(fit.scaled_height);
  }
  const auto editorial_duration = input.timeline.duration();
  const auto frame_count = output_frame_count(editorial_duration, frame_rate);
  if (frame_count <= 0) {
    throw ExportPlanError{ExportPlanErrorCode::EmptyTimeline, "timeline has no output frames"};
  }
  const auto output_duration = output_frame_time(domain::FrameNumber{frame_count}, frame_rate);
  const auto samples = exporting::audio_sample_count(output_duration);

  return ExportPlan{input, profile, master->id(), frame_rate, editorial_duration,
                    output_duration, frame_count, width, height, samples};
}

} // namespace pulsar::application::exporting
