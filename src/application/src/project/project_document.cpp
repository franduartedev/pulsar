#include <pulsar/application/project/project_document.hpp>
#include <pulsar/application/composition/project_canvas.hpp>

#include <pulsar/domain/timeline/track_kind.hpp>

#include <algorithm>
#include <unordered_set>
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

void validate_visual_track_ids(const domain::Timeline& timeline, domain::TrackId primary,
                              const std::vector<domain::TrackId>& visual_track_ids) {
  std::unordered_set<std::uint64_t> remaining;
  for (const auto& track : timeline.tracks()) {
    if (track.kind() == domain::TrackKind::Video) remaining.insert(track.id().value());
  }
  if (!remaining.contains(primary.value())) {
    throw std::invalid_argument{"primary video track is absent or not a video track"};
  }
  for (const auto id : visual_track_ids) {
    if (remaining.erase(id.value()) != 1) {
      throw std::invalid_argument{"visual stack contains a duplicate or non-video track"};
    }
  }
  if (!remaining.empty()) {
    throw std::invalid_argument{"visual stack must contain every video track exactly once"};
  }
}

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

void ProjectDocument::IdAllocator::ensure_track_consumable() const {
  if (state.next_track_id == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"cannot allocate another track id"};
  }
}

void ProjectDocument::IdAllocator::ensure_title_consumable() const {
  if (state.next_title_id == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"cannot allocate another title id"};
  }
}

void ProjectDocument::IdAllocator::consume_title() {
  increment_checked(state.next_title_id, "title id");
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
    : allocator_{}, timeline_{allocator_.next_timeline()}, video_track_id_{allocator_.next_track()},
      visual_track_ids_{video_track_id_} {
  allocator_.consume_timeline();
  allocator_.consume_track();
  timeline_.add_track(domain::Track{video_track_id_, domain::TrackKind::Video});
}
ProjectDocument ProjectDocument::restore(const ProjectRestoreState& state) {
  ProjectDocument restored;
  domain::Timeline timeline{state.timeline_id};
  std::unordered_set<std::uint64_t> media_ids;
  std::unordered_set<std::uint64_t> clip_ids;
  std::unordered_set<std::uint64_t> track_ids;

  std::uint64_t max_media_id = 0;
  for (const auto& asset : state.assets) {
    if (!media_ids.insert(asset.id().value()).second) {
      throw std::invalid_argument{"duplicate media id in project restore state"};
    }
    max_media_id = std::max(max_media_id, asset.id().value());
  }

  bool designated_video_found = false;
  std::uint64_t max_track_id = 0;
  std::uint64_t max_clip_id = 0;
  for (const auto& restore_track : state.tracks) {
    if (!track_ids.insert(restore_track.id.value()).second) {
      throw std::invalid_argument{"duplicate track id in project restore state"};
    }
    if (restore_track.id == state.video_track_id) {
      if (restore_track.kind != domain::TrackKind::Video) {
        throw std::invalid_argument{"designated video track is not a video track"};
      }
      designated_video_found = true;
    }
    max_track_id = std::max(max_track_id, restore_track.id.value());
    domain::Track track{restore_track.id, restore_track.kind, restore_track.audio_state};
    for (const auto& clip : restore_track.clips) {
      if (!clip_ids.insert(clip.id().value()).second) {
        throw std::invalid_argument{"duplicate clip id in project restore state"};
      }
      if (!media_ids.contains(clip.media_id().value())) {
        throw std::invalid_argument{"clip references unknown media id"};
      }
      const auto asset = std::find_if(state.assets.begin(), state.assets.end(),
                                      [&clip](const MediaAsset& item) {
                                        return item.id() == clip.media_id();
                                      });
      if (asset == state.assets.end()) {
        throw std::invalid_argument{"clip source range exceeds stored media duration"};
      }
      if (restore_track.kind == domain::TrackKind::Video && !asset->has_video()) {
        throw std::invalid_argument{"video track clip references an asset without video"};
      }
      if (restore_track.kind == domain::TrackKind::Audio && !asset->audio().has_value()) {
        throw std::invalid_argument{"audio track clip references an asset without audio"};
      }
      const auto duration = restore_track.kind == domain::TrackKind::Audio &&
                                    asset->audio().has_value() && asset->audio()->duration.has_value()
                                ? *asset->audio()->duration
                                : asset->playable_duration();
      if (clip.source_range().end() > duration) {
        throw std::invalid_argument{"clip source range exceeds stored media duration"};
      }
      max_clip_id = std::max(max_clip_id, clip.id().value());
      track.insert_clip(clip);
    }
    timeline.add_track(std::move(track));
  }

  if (!designated_video_found) {
    throw std::invalid_argument{"designated video track is absent"};
  }
  validate_visual_track_ids(timeline, state.video_track_id, state.visual_track_ids);
  if (state.canvas && (state.canvas->width <= 0 || state.canvas->height <= 0 ||
                       state.canvas->width > 32768 || state.canvas->height > 32768 ||
                       static_cast<std::int64_t>(state.canvas->width) * state.canvas->height > 64'000'000)) {
    throw std::invalid_argument{"invalid project canvas dimensions"};
  }
  if (state.allocator_state.next_media_id == 0 ||
      state.allocator_state.next_media_id <= max_media_id ||
      state.allocator_state.next_media_id == std::numeric_limits<std::uint64_t>::max()) {
    throw std::invalid_argument{"invalid next media allocator high-water mark"};
  }
  if (state.allocator_state.next_clip_id == 0 ||
      state.allocator_state.next_clip_id <= max_clip_id ||
      state.allocator_state.next_clip_id == std::numeric_limits<std::uint64_t>::max()) {
    throw std::invalid_argument{"invalid next clip allocator high-water mark"};
  }
  if (state.allocator_state.next_timeline_id == 0 ||
      state.allocator_state.next_timeline_id <= state.timeline_id.value() ||
      state.allocator_state.next_timeline_id == std::numeric_limits<std::uint64_t>::max()) {
    throw std::invalid_argument{"invalid next timeline allocator high-water mark"};
  }
  if (state.allocator_state.next_track_id == 0 ||
      state.allocator_state.next_track_id <= max_track_id ||
      state.allocator_state.next_track_id == std::numeric_limits<std::uint64_t>::max()) {
    throw std::invalid_argument{"invalid next track allocator high-water mark"};
  }

  // Titles: unique ids, no overlap between them, allocator above every id.
  std::vector<domain::Title> titles = state.titles;
  std::sort(titles.begin(), titles.end(), [](const domain::Title& a, const domain::Title& b) { return a.start() < b.start(); });
  std::unordered_set<std::uint64_t> title_ids;
  std::uint64_t max_title_id = 0;
  for (std::size_t i = 0; i < titles.size(); ++i) {
    if (!title_ids.insert(titles[i].id().value()).second) {
      throw std::invalid_argument{"duplicate title id in project restore state"};
    }
    if (i > 0 && titles[i].start() < titles[i - 1].end()) {
      throw std::invalid_argument{"titles overlap in project restore state"};
    }
    max_title_id = std::max(max_title_id, titles[i].id().value());
  }
  if (state.allocator_state.next_title_id == 0 || state.allocator_state.next_title_id <= max_title_id ||
      state.allocator_state.next_title_id == std::numeric_limits<std::uint64_t>::max()) {
    throw std::invalid_argument{"invalid next title allocator high-water mark"};
  }

  restored.titles_ = std::move(titles);
  restored.timeline_ = std::move(timeline);
  restored.video_track_id_ = state.video_track_id;
  restored.visual_track_ids_ = state.visual_track_ids;
  restored.assets_ = state.assets;
  restored.transitions_ = state.transitions;
  restored.canvas_ = state.canvas ? state.canvas : legacy_project_canvas(restored.snapshot());
  restored.allocator_.state = state.allocator_state;
  for (const auto& transition : restored.transitions_) restored.validate_transition(transition);
  restored.current_history_state_id_ = 1;
  restored.saved_history_state_id_ = 1;
  restored.next_history_state_id_ = 2;
  restored.document_generation_ = 0;
  return restored;
}

void ProjectDocument::validate_transition(const domain::Transition& transition) const {
  domain::validate_transition_value(transition);
  const auto* track = timeline_.find_track(transition.track_id);
  if (track == nullptr || track->kind() != domain::TrackKind::Video) {
    throw std::invalid_argument{"transition track is not a video track"};
  }
  const auto* outgoing = track->find_clip(transition.outgoing_clip_id);
  const auto* incoming = track->find_clip(transition.incoming_clip_id);
  if (outgoing == nullptr || incoming == nullptr ||
      outgoing->timeline_range().end() != incoming->timeline_range().start()) {
    throw std::invalid_argument{"transition clips are not adjacent at one edit point"};
  }
  if (transition.duration > outgoing->duration() ||
      incoming->source_range().start() < transition.duration) {
    throw std::invalid_argument{"transition duration exceeds available clip handles"};
  }
}

void ProjectDocument::add_transition_value(const domain::Transition& transition) {
  validate_transition(transition);
  const auto duplicate = std::find_if(transitions_.begin(), transitions_.end(), [&](const auto& item) {
    return item.track_id == transition.track_id &&
           (item.outgoing_clip_id == transition.outgoing_clip_id ||
            item.incoming_clip_id == transition.incoming_clip_id);
  });
  if (duplicate != transitions_.end()) throw std::invalid_argument{"edit already has a transition"};
  transitions_.push_back(transition);
}

void ProjectDocument::remove_transition_value(const domain::Transition& transition) {
  const auto found = std::find(transitions_.begin(), transitions_.end(), transition);
  if (found == transitions_.end()) throw std::invalid_argument{"transition does not exist"};
  transitions_.erase(found);
}

void ProjectDocument::ensure_clip_has_no_transition(domain::ClipId clip_id) const {
  if (std::any_of(transitions_.begin(), transitions_.end(), [&](const auto& transition) {
        return transition.outgoing_clip_id == clip_id || transition.incoming_clip_id == clip_id;
      })) {
    throw std::invalid_argument{"remove the transition before changing this edit"};
  }
}

const domain::Title* ProjectDocument::find_title(domain::TitleId id) const noexcept {
  for (const auto& title : titles_) {
    if (title.id() == id) return &title;
  }
  return nullptr;
}

void ProjectDocument::add_title_value(const domain::Title& title) {
  if (find_title(title.id()) != nullptr) throw std::invalid_argument{"title id already exists in document"};
  for (const auto& other : titles_) {
    if (title.range().overlaps(other.range())) throw std::invalid_argument{"title overlaps another title"};
  }
  titles_.push_back(title);
  std::sort(titles_.begin(), titles_.end(), [](const domain::Title& a, const domain::Title& b) { return a.start() < b.start(); });
}

void ProjectDocument::remove_title_value(domain::TitleId id) {
  const auto found = std::find_if(titles_.begin(), titles_.end(), [id](const domain::Title& t) { return t.id() == id; });
  if (found == titles_.end()) throw std::invalid_argument{"title does not exist in document"};
  titles_.erase(found);
}

void ProjectDocument::replace_title_value(const domain::Title& title) {
  const auto found = std::find_if(titles_.begin(), titles_.end(), [&](const domain::Title& t) { return t.id() == title.id(); });
  if (found == titles_.end()) throw std::invalid_argument{"title does not exist in document"};
  for (const auto& other : titles_) {
    if (other.id() != title.id() && title.range().overlaps(other.range())) {
      throw std::invalid_argument{"title overlaps another title"};
    }
  }
  *found = title;
  std::sort(titles_.begin(), titles_.end(), [](const domain::Title& a, const domain::Title& b) { return a.start() < b.start(); });
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
  // A0 selection rule: first probed audio stream wins, by its real FFmpeg
  // stream index. No language/default-track heuristics.
  std::optional<AudioAssetMetadata> audio;
  if (!result.audio_streams.empty()) {
    const AudioStreamInfo& audio_stream = result.audio_streams.front();
    audio = AudioAssetMetadata{audio_stream.stream_index, audio_stream.sample_rate,
                               audio_stream.channel_count, audio_stream.time_base,
                               audio_stream.duration};
  }
  if (result.video_streams.empty() && !audio.has_value()) {
    throw std::invalid_argument{"media has no usable video or audio stream"};
  }
  if (result.video_streams.empty()) {
    const auto duration = audio->duration.has_value() ? audio->duration : result.duration;
    if (!duration.has_value() || *duration <= domain::MediaTime::zero()) {
      throw std::invalid_argument{"audio-only media has no exact positive duration"};
    }
    return MediaAsset::audio_only(id, source_locator, *duration, *audio,
                                  MediaAvailability::Available, std::nullopt);
  }
  const VideoStreamInfo& stream = result.video_streams.front();
  if (!stream.duration.has_value() || *stream.duration <= domain::MediaTime::zero()) {
    throw std::invalid_argument{"selected video stream has no exact positive duration"};
  }

  VideoAssetMetadata metadata{stream.codec_name, stream.width, stream.height, stream.time_base,
                              stream.nominal_frame_rate, result.container_name};
  return MediaAsset{id, source_locator, stream.stream_index, *stream.duration,
                    std::move(metadata), MediaAvailability::Available, std::nullopt,
                    std::move(audio)};
}

void ProjectDocument::add_asset_value(const MediaAsset& asset) {
  if (find_asset(asset.id()).has_value()) {
    throw std::invalid_argument{"media asset id already exists in document"};
  }
  assets_.push_back(asset);
  ++asset_revision_;
}

void ProjectDocument::replace_asset_value(const MediaAsset& asset) {
  const auto found = std::find_if(assets_.begin(), assets_.end(),
                                  [id = asset.id()](const MediaAsset& item) {
                                    return item.id() == id;
                                  });
  if (found == assets_.end()) {
    throw std::invalid_argument{"media asset does not exist in document"};
  }
  *found = asset;
  ++asset_revision_;
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
  ++asset_revision_;
}

void ProjectDocument::validate_clip_for_track(domain::TrackId track_id,
                                               const domain::Clip& clip) const {
  const auto* track = timeline_.find_track(track_id);
  if (track == nullptr) {
    throw std::invalid_argument{"clip destination track does not exist"};
  }
  const auto asset = find_asset(clip.media_id());
  if (!asset.has_value()) {
    throw std::invalid_argument{"clip references an unknown media asset"};
  }
  if (asset->availability() != MediaAvailability::Available) {
    throw std::invalid_argument{"clip references unavailable media"};
  }
  if (track->kind() == domain::TrackKind::Video && !asset->has_video()) {
    throw std::invalid_argument{"video track clip requires an asset with video"};
  }
  if (track->kind() == domain::TrackKind::Audio && !asset->audio().has_value()) {
    throw std::invalid_argument{"audio track clip requires an asset with usable audio"};
  }
  const auto duration = track->kind() == domain::TrackKind::Audio && asset->audio()->duration.has_value()
                            ? *asset->audio()->duration
                            : asset->playable_duration();
  if (clip.source_range().end() > duration) {
    throw std::invalid_argument{"clip source range exceeds media duration"};
  }
}

ProjectSnapshot ProjectDocument::snapshot() const {
  return ProjectSnapshot{timeline_, assets_, video_track_id_, allocator_.state,
                         current_history_state_id_, saved_history_state_id_, next_history_state_id_,
                           document_generation_, titles_, visual_track_ids_, canvas_, transitions_};
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
