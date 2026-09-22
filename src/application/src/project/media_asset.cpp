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
  if (video_stream_index_ < 0) metadata_.reset();
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
  if (metadata_.has_value() && (metadata_->width <= 0 || metadata_->height <= 0)) {
    throw std::invalid_argument{"media asset dimensions must be positive"};
  }
}

MediaAsset MediaAsset::audio_only(domain::MediaId id, std::filesystem::path source_locator,
                                  domain::MediaTime playable_duration,
                                  AudioAssetMetadata audio, MediaAvailability availability,
                                  std::optional<std::string> availability_reason) {
  if (audio.duration.has_value() && *audio.duration <= domain::MediaTime::zero()) {
    throw std::invalid_argument{"audio asset duration must be positive when present"};
  }
  return MediaAsset{id, std::move(source_locator), -1, playable_duration,
                    VideoAssetMetadata{}, availability, std::move(availability_reason),
                    std::move(audio)};
}

const VideoAssetMetadata& MediaAsset::metadata() const {
  if (!metadata_.has_value()) {
    throw std::logic_error{"audio-only media asset has no video metadata"};
  }
  return *metadata_;
}

} // namespace pulsar::application
