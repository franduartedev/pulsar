#pragma once

#include <pulsar/application/project/project_snapshot.hpp>
#include <pulsar/domain/time/frame_number.hpp>
#include <pulsar/domain/time/frame_rate.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace pulsar::application::exporting {

enum class ExportContainer { Mp4 };
enum class ExportVideoCodec { H264 };
enum class ExportPixelFormat { Yuv420p };
enum class ExportFrameRateMode { Cfr };
enum class ExportAudioCodec { AacLc };
enum class ExportVideoGapPolicy { Black };
enum class ExportAudioGapPolicy { Silence };
enum class ExportMissingMediaPolicy { Fail };
enum class ExportGeometryPolicy { AspectPreservingLetterbox };
enum class ExportPresentationMetadataPolicy { RejectUnsupported };

class ExportProfile final {
public:
  [[nodiscard]] static ExportProfile beta_v01() noexcept;

  [[nodiscard]] ExportContainer container() const noexcept { return container_; }
  [[nodiscard]] ExportVideoCodec video_codec() const noexcept { return video_codec_; }
  [[nodiscard]] ExportPixelFormat pixel_format() const noexcept { return pixel_format_; }
  [[nodiscard]] ExportFrameRateMode frame_rate_mode() const noexcept { return frame_rate_mode_; }
  [[nodiscard]] ExportAudioCodec audio_codec() const noexcept { return audio_codec_; }
  [[nodiscard]] std::int32_t audio_sample_rate() const noexcept { return audio_sample_rate_; }
  [[nodiscard]] std::int32_t audio_channels() const noexcept { return audio_channels_; }
  [[nodiscard]] std::int32_t audio_bitrate() const noexcept { return audio_bitrate_; }
  [[nodiscard]] ExportVideoGapPolicy video_gap_policy() const noexcept { return video_gap_policy_; }
  [[nodiscard]] ExportAudioGapPolicy audio_gap_policy() const noexcept { return audio_gap_policy_; }
  [[nodiscard]] ExportMissingMediaPolicy missing_media_policy() const noexcept {
    return missing_media_policy_;
  }
  [[nodiscard]] ExportGeometryPolicy geometry_policy() const noexcept { return geometry_policy_; }
  [[nodiscard]] ExportPresentationMetadataPolicy presentation_metadata_policy() const noexcept {
    return presentation_metadata_policy_;
  }

private:
  ExportContainer container_{ExportContainer::Mp4};
  ExportVideoCodec video_codec_{ExportVideoCodec::H264};
  ExportPixelFormat pixel_format_{ExportPixelFormat::Yuv420p};
  ExportFrameRateMode frame_rate_mode_{ExportFrameRateMode::Cfr};
  ExportAudioCodec audio_codec_{ExportAudioCodec::AacLc};
  std::int32_t audio_sample_rate_{48'000};
  std::int32_t audio_channels_{2};
  std::int32_t audio_bitrate_{192'000};
  ExportVideoGapPolicy video_gap_policy_{ExportVideoGapPolicy::Black};
  ExportAudioGapPolicy audio_gap_policy_{ExportAudioGapPolicy::Silence};
  ExportMissingMediaPolicy missing_media_policy_{ExportMissingMediaPolicy::Fail};
  ExportGeometryPolicy geometry_policy_{ExportGeometryPolicy::AspectPreservingLetterbox};
  ExportPresentationMetadataPolicy presentation_metadata_policy_{
      ExportPresentationMetadataPolicy::RejectUnsupported};
};

enum class ExportPlanErrorCode {
  EmptyTimeline,
  NoVideoMaster,
  InvalidFrameRate,
  InvalidDimensions,
  MissingMedia,
  UnsupportedPresentationMetadata,
  Overflow,
  InvalidProject,
};

class ExportPlanError final : public std::runtime_error {
public:
  ExportPlanError(ExportPlanErrorCode code, std::string message)
      : std::runtime_error{std::move(message)}, code_{code} {}

  [[nodiscard]] ExportPlanErrorCode code() const noexcept { return code_; }

private:
  ExportPlanErrorCode code_;
};

[[nodiscard]] std::int64_t output_frame_index_at(domain::MediaTime time,
                                                  domain::FrameRate frame_rate,
                                                  domain::RoundingMode rounding);
[[nodiscard]] domain::MediaTime output_frame_time(domain::FrameNumber frame,
                                                   domain::FrameRate frame_rate);
[[nodiscard]] std::int64_t output_frame_count(domain::MediaTime duration,
                                               domain::FrameRate frame_rate);
[[nodiscard]] std::int64_t audio_sample_index_at(domain::MediaTime time,
                                                  domain::RoundingMode rounding);
[[nodiscard]] domain::MediaTime audio_sample_time(std::int64_t sample_index);
[[nodiscard]] std::int64_t audio_sample_count(domain::MediaTime duration);

class ExportPlan final {
public:
  [[nodiscard]] static ExportPlan build(const ProjectSnapshot& snapshot,
                                        ExportProfile profile = ExportProfile::beta_v01());

  ExportPlan(const ExportPlan&) = default;
  ExportPlan& operator=(const ExportPlan&) = delete;
  ExportPlan(ExportPlan&&) noexcept = default;
  ExportPlan& operator=(ExportPlan&&) noexcept = delete;
  ~ExportPlan() = default;

  [[nodiscard]] const ProjectSnapshot& snapshot() const noexcept { return snapshot_; }
  [[nodiscard]] const ExportProfile& profile() const noexcept { return profile_; }
  [[nodiscard]] std::uint64_t document_generation() const noexcept {
    return snapshot_.document_generation;
  }
  [[nodiscard]] domain::MediaId master_video_asset_id() const noexcept { return master_video_asset_id_; }
  [[nodiscard]] domain::FrameRate frame_rate() const noexcept { return frame_rate_; }
  [[nodiscard]] domain::MediaTime editorial_duration() const noexcept { return editorial_duration_; }
  [[nodiscard]] domain::MediaTime output_duration() const noexcept { return output_duration_; }
  [[nodiscard]] std::int64_t frame_count() const noexcept { return frame_count_; }
  [[nodiscard]] std::int64_t output_width() const noexcept { return output_width_; }
  [[nodiscard]] std::int64_t output_height() const noexcept { return output_height_; }
  [[nodiscard]] std::int32_t audio_sample_rate() const noexcept {
    return profile_.audio_sample_rate();
  }
  [[nodiscard]] std::int32_t audio_channels() const noexcept { return profile_.audio_channels(); }
  [[nodiscard]] std::int64_t audio_sample_count() const noexcept { return audio_sample_count_; }

private:
  ExportPlan(ProjectSnapshot snapshot, ExportProfile profile, domain::MediaId master_video_asset_id,
             domain::FrameRate frame_rate, domain::MediaTime editorial_duration,
             domain::MediaTime output_duration, std::int64_t frame_count,
             std::int64_t output_width, std::int64_t output_height,
             std::int64_t audio_sample_count)
      : snapshot_{std::move(snapshot)}, profile_{profile}, master_video_asset_id_{master_video_asset_id},
        frame_rate_{frame_rate}, editorial_duration_{editorial_duration}, output_duration_{output_duration},
        frame_count_{frame_count}, output_width_{output_width}, output_height_{output_height},
        audio_sample_count_{audio_sample_count} {}

  ProjectSnapshot snapshot_;
  ExportProfile profile_;
  domain::MediaId master_video_asset_id_;
  domain::FrameRate frame_rate_;
  domain::MediaTime editorial_duration_;
  domain::MediaTime output_duration_;
  std::int64_t frame_count_;
  std::int64_t output_width_;
  std::int64_t output_height_;
  std::int64_t audio_sample_count_;
};

} // namespace pulsar::application::exporting
