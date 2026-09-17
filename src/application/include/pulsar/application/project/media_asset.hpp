#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/frame_rate.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace pulsar::application {

enum class MediaAvailability {
  Available,
  Unavailable,
};

struct VideoAssetMetadata {
  std::string codec_name;
  int width;
  int height;
  domain::Rational time_base;
  std::optional<domain::FrameRate> nominal_frame_rate;
  std::optional<std::string> container_name;
};

class MediaAsset {
public:
  MediaAsset(domain::MediaId id, std::filesystem::path source_locator,
             int video_stream_index, domain::MediaTime playable_duration,
             VideoAssetMetadata metadata,
             MediaAvailability availability = MediaAvailability::Available,
             std::optional<std::string> availability_reason = std::nullopt);

  [[nodiscard]] domain::MediaId id() const noexcept { return id_; }
  [[nodiscard]] const std::filesystem::path& source_locator() const noexcept {
    return source_locator_;
  }
  [[nodiscard]] int video_stream_index() const noexcept { return video_stream_index_; }
  [[nodiscard]] domain::MediaTime playable_duration() const noexcept {
    return playable_duration_;
  }
  [[nodiscard]] const VideoAssetMetadata& metadata() const noexcept { return metadata_; }
  [[nodiscard]] MediaAvailability availability() const noexcept { return availability_; }
  [[nodiscard]] const std::optional<std::string>& availability_reason() const noexcept {
    return availability_reason_;
  }

private:
  domain::MediaId id_;
  std::filesystem::path source_locator_;
  int video_stream_index_;
  domain::MediaTime playable_duration_;
  VideoAssetMetadata metadata_;
  MediaAvailability availability_;
  std::optional<std::string> availability_reason_;
};

} // namespace pulsar::application
