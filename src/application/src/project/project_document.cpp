#include <pulsar/application/project/project_document.hpp>

#include <pulsar/domain/timeline/track_kind.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace pulsar::application {
namespace {

void increment_checked(std::uint64_t& value, const char* what) {
  if (value == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{std::string{"cannot allocate another "} + what};
  }
  ++value;
}

} // namespace

void ProjectDocument::IdAllocator::ensure_media_consumable() const {
  if (state.next_media_id == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"cannot allocate another media id"};
  }
}

void ProjectDocument::IdAllocator::ensure_clip_consumable() const {
  if (state.next_clip_id == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"cannot allocate another clip id"};
  }
}

void ProjectDocument::IdAllocator::consume_media() {
  increment_checked(state.next_media_id, "media id");
}

void ProjectDocument::IdAllocator::consume_clip() {
  increment_checked(state.next_clip_id, "clip id");
}

void ProjectDocument::IdAllocator::consume_timeline() {
  increment_checked(state.next_timeline_id, "timeline id");
}

void ProjectDocument::IdAllocator::consume_track() {
  increment_checked(state.next_track_id, "track id");
}

ProjectDocument::ProjectDocument()
    : allocator_{}, timeline_{allocator_.next_timeline()}, video_track_id_{allocator_.next_track()} {
  allocator_.consume_timeline();
  allocator_.consume_track();
  timeline_.add_track(domain::Track{video_track_id_, domain::TrackKind::Video});
}

std::optional<MediaAsset> ProjectDocument::find_asset(domain::MediaId id) const {
  for (const auto& asset : assets_) {
    if (asset.id() == id) {
      return asset;
    }
  }
  return std::nullopt;
}

domain::ClipId ProjectDocument::next_clip_id() const {
  return allocator_.next_clip();
}

MediaAsset ProjectDocument::build_asset(const std::filesystem::path& source_locator,
                                        const MediaProbe& probe, domain::MediaId id) const {
  if (source_locator.empty()) {
    throw std::invalid_argument{"media asset source locator cannot be empty"};
  }

  const MediaProbeResult result = probe.probe(source_locator);
  if (result.video_streams.empty()) {
    throw std::invalid_argument{"media has no video stream"};
  }
  const VideoStreamInfo& stream = result.video_streams.front();
  if (!stream.duration.has_value() || *stream.duration <= domain::MediaTime::zero()) {
    throw std::invalid_argument{"selected video stream has no exact positive duration"};
  }

  VideoAssetMetadata metadata{stream.codec_name, stream.width, stream.height, stream.time_base,
                              stream.nominal_frame_rate, result.container_name};
  return MediaAsset{id, source_locator, stream.stream_index, *stream.duration,
                    std::move(metadata)};
}

void ProjectDocument::add_asset_value(const MediaAsset& asset) {
  if (find_asset(asset.id()).has_value()) {
    throw std::invalid_argument{"media asset id already exists in document"};
  }
  assets_.push_back(asset);
}

void ProjectDocument::remove_asset_value(domain::MediaId id) {
  for (const auto& track : timeline_.tracks()) {
    for (const auto& clip : track.clips()) {
      if (clip.media_id() == id) {
        throw std::invalid_argument{"cannot remove media asset referenced by a clip"};
      }
    }
  }
  const auto iterator = std::find_if(assets_.begin(), assets_.end(),
                                     [id](const MediaAsset& asset) { return asset.id() == id; });
  if (iterator == assets_.end()) {
    throw std::invalid_argument{"media asset does not exist in document"};
  }
  assets_.erase(iterator);
}

void ProjectDocument::validate_new_clip(const domain::Clip& clip) const {
  const auto asset = find_asset(clip.media_id());
  if (!asset.has_value()) {
    throw std::invalid_argument{"clip references an unknown media asset"};
  }
  if (asset->availability() != MediaAvailability::Available) {
    throw std::invalid_argument{"clip references unavailable media"};
  }
  if (clip.source_range().end() > asset->playable_duration()) {
    throw std::invalid_argument{"clip source range exceeds media duration"};
  }
}

ProjectSnapshot ProjectDocument::snapshot() const {
  return ProjectSnapshot{timeline_, assets_, video_track_id_, allocator_.state,
                         current_history_state_id_, saved_history_state_id_, next_history_state_id_,
                         document_generation_};
}

void ProjectDocument::ensure_new_edit_possible() const {
  ensure_generation_advance_possible();
  if (next_history_state_id_ == std::numeric_limits<HistoryStateId>::max()) {
    throw std::overflow_error{"project history state identity exhausted"};
  }
}

void ProjectDocument::ensure_generation_advance_possible() const {
  if (document_generation_ == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"project document generation exhausted"};
  }
}

void ProjectDocument::commit_new_edit() {
  ensure_new_edit_possible();
  current_history_state_id_ = next_history_state_id_++;
  ++document_generation_;
}

void ProjectDocument::commit_history_restore(HistoryStateId state_id) {
  ensure_generation_advance_possible();
  current_history_state_id_ = state_id;
  ++document_generation_;
}

} // namespace pulsar::application
