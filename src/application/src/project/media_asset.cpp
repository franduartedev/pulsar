#include <pulsar/application/project/media_asset.hpp>

#include <stdexcept>
#include <utility>

namespace pulsar::application {

MediaAsset::MediaAsset(domain::MediaId id, std::filesystem::path source_locator,
                       int video_stream_index, domain::MediaTime playable_duration,
                       VideoAssetMetadata metadata, MediaAvailability availability,
                       std::optional<std::string> availability_reason,
                       std::optional<AudioAssetMetadata> audio)
    : id_{id}, source_locator_{std::move(source_locator)}, video_stream_index_{video_stream_index},
      playable_duration_{playable_duration}, metadata_{std::move(metadata)},
      availability_{availability}, availability_reason_{std::move(availability_reason)},
      audio_{std::move(audio)} {
  if (source_locator_.empty()) {
    throw std::invalid_argument{"media asset source locator cannot be empty"};
  }
  if (video_stream_index_ < 0) {
    throw std::invalid_argument{"media asset video stream index cannot be negative"};
  }
  if (audio_.has_value()) {
    if (audio_->stream_index < 0) {
      throw std::invalid_argument{"media asset audio stream index cannot be negative"};
    }
    if (audio_->sample_rate <= 0) {
      throw std::invalid_argument{"media asset audio sample rate must be positive"};
    }
    if (audio_->channel_count <= 0) {
      throw std::invalid_argument{"media asset audio channel count must be positive"};
    }
  }
  if (playable_duration_ <= domain::MediaTime::zero()) {
    throw std::invalid_argument{"media asset playable duration must be positive"};
  }
  if (metadata_.width <= 0 || metadata_.height <= 0) {
    throw std::invalid_argument{"media asset dimensions must be positive"};
  }
}

} // namespace pulsar::application
