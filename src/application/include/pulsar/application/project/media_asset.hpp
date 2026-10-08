#pragma once

#include <pulsar/application/media/still_media.hpp>
#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/frame_rate.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

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

// A0 identity-only audio description: which audio stream of the container was
// selected at import plus the clock metadata the probe already reports. No
// sample format, no PCM/output policy, no timeline timestamp mapping.
struct AudioAssetMetadata {
  int stream_index;
  int sample_rate;
  int channel_count;
  domain::Rational time_base;
  std::optional<domain::MediaTime> duration;
};

class MediaAsset {
public:
  MediaAsset(domain::MediaId id, std::filesystem::path source_locator,
             int video_stream_index, domain::MediaTime playable_duration,
             VideoAssetMetadata metadata,
             MediaAvailability availability = MediaAvailability::Available,
             std::optional<std::string> availability_reason = std::nullopt,
             std::optional<AudioAssetMetadata> audio = std::nullopt);

  [[nodiscard]] static MediaAsset audio_only(domain::MediaId id,
                                              std::filesystem::path source_locator,
                                              domain::MediaTime playable_duration,
                                              AudioAssetMetadata audio,
                                              MediaAvailability availability = MediaAvailability::Available,
                                              std::optional<std::string> availability_reason = std::nullopt);

  [[nodiscard]] domain::MediaId id() const noexcept { return id_; }
  [[nodiscard]] const std::filesystem::path& source_locator() const noexcept {
    return source_locator_;
  }
  [[nodiscard]] int video_stream_index() const noexcept { return video_stream_index_; }
  [[nodiscard]] bool has_video() const noexcept { return video_stream_index_ >= 0; }
  [[nodiscard]] domain::MediaTime playable_duration() const noexcept {
    return playable_duration_;
  }
  [[nodiscard]] const VideoAssetMetadata& metadata() const;
  [[nodiscard]] MediaAvailability availability() const noexcept { return availability_; }
  [[nodiscard]] const std::optional<std::string>& availability_reason() const noexcept {
    return availability_reason_;
  }
  [[nodiscard]] const std::optional<AudioAssetMetadata>& audio() const noexcept { return audio_; }

  // A photograph/graphic or a colour card: one picture held for the whole clip.
  [[nodiscard]] bool is_still() const {
    if (!metadata_.has_value()) return false;
    return is_color_card_locator(source_locator_.generic_string()) ||
           (metadata_->container_name.has_value() && is_still_image_container(*metadata_->container_name));
  }
  [[nodiscard]] bool is_color_card() const {
    return metadata_.has_value() && is_color_card_locator(source_locator_.generic_string());
  }
  // Length of the clip created when this asset is placed on the timeline.
  [[nodiscard]] domain::MediaTime default_clip_duration() const {
    return is_still() ? still_default_duration() : playable_duration_;
  }

private:
  domain::MediaId id_;
  std::filesystem::path source_locator_;
  int video_stream_index_;
  domain::MediaTime playable_duration_;
  std::optional<VideoAssetMetadata> metadata_;
  MediaAvailability availability_;
  std::optional<std::string> availability_reason_;
  std::optional<AudioAssetMetadata> audio_;
};

// Size of the picture a project is built around when `asset` is its first (master) video clip.
// A photograph is fitted into 1920x1080 (never enlarged) so a 12 MP picture does not become a
// 4000x3000 export; ordinary video keeps its own size. Always even, as H.264 needs.
[[nodiscard]] inline std::pair<int, int> master_canvas_size(const MediaAsset& asset) {
  long long width = asset.metadata().width;
  long long height = asset.metadata().height;
  if (asset.is_still() && (width > still_canvas_max_width || height > still_canvas_max_height)) {
    if (width * still_canvas_max_height > height * still_canvas_max_width) {
      height = height * still_canvas_max_width / width;
      width = still_canvas_max_width;
    } else {
      width = width * still_canvas_max_height / height;
      height = still_canvas_max_height;
    }
  }
  const auto even = [](long long v) { return static_cast<int>(v < 2 ? 2 : v + (v & 1)); };
  return {even(width), even(height)};
}

// The master is the first real video clip; stills only decide the canvas when nothing else can.
template <typename Range, typename Lookup>
[[nodiscard]] const MediaAsset* pick_master_asset(const Range& clips_in_order, Lookup&& asset_of) {
  const MediaAsset* first_still = nullptr;
  for (const auto& clip : clips_in_order) {
    const MediaAsset* asset = asset_of(clip);
    if (asset == nullptr || !asset->has_video()) continue;
    if (!asset->is_still()) return asset;
    if (first_still == nullptr) first_still = asset;
  }
  return first_still;
}

} // namespace pulsar::application
