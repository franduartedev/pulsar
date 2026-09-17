#include <pulsar/application/project/project_document.hpp>

#include <pulsar/domain/timeline/track_kind.hpp>

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

domain::MediaId ProjectDocument::import_asset(const std::filesystem::path& source_locator,
                                              const MediaProbe& probe) {
  if (source_locator.empty()) {
    throw std::invalid_argument{"media asset source locator cannot be empty"};
  }

  // Probe and validate before consuming an ID or mutating the catalog.
  const MediaProbeResult result = probe.probe(source_locator);
  if (result.video_streams.empty()) {
    throw std::invalid_argument{"media has no video stream"};
  }

  const VideoStreamInfo& stream = result.video_streams.front();
  if (!stream.duration.has_value() || *stream.duration <= domain::MediaTime::zero()) {
    throw std::invalid_argument{"selected video stream has no exact positive duration"};
  }

  ensure_state_advance_possible();
  allocator_.ensure_media_consumable();
  const domain::MediaId id = allocator_.next_media();
  VideoAssetMetadata metadata{stream.codec_name, stream.width, stream.height, stream.time_base,
                              stream.nominal_frame_rate, result.container_name};
  assets_.emplace_back(id, source_locator, stream.stream_index, *stream.duration,
                       std::move(metadata));
  allocator_.consume_media();
  advance_state();
  return id;
}

domain::ClipId ProjectDocument::allocate_clip_id() {
  ensure_state_advance_possible();
  allocator_.ensure_clip_consumable();
  const domain::ClipId id = allocator_.next_clip();
  allocator_.consume_clip();
  advance_state();
  return id;
}

ProjectSnapshot ProjectDocument::snapshot() const {
  return ProjectSnapshot{timeline_, assets_, video_track_id_, allocator_.state,
                         current_history_state_id_, saved_history_state_id_, next_history_state_id_,
                         document_generation_};
}

void ProjectDocument::ensure_state_advance_possible() const {
  if (next_history_state_id_ == std::numeric_limits<HistoryStateId>::max() ||
      document_generation_ == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"project document state identity exhausted"};
  }
}

void ProjectDocument::advance_state() {
  current_history_state_id_ = next_history_state_id_++;
  ++document_generation_;
}

} // namespace pulsar::application
