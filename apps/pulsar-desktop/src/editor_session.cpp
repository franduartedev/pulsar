#include "editor_session.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace pulsar::desktop {
namespace {
// Human label for a clip's media: file name, or "Click Soft" for the built-in
// "pulsar-sound:click_soft" catalogue locators.
std::unordered_map<std::string, std::string>& sound_display_names() {
  static std::unordered_map<std::string, std::string> names;
  return names;
}

[[nodiscard]] std::string clip_display_name(const std::filesystem::path& locator) {
  const std::string text = locator.string();
  constexpr std::string_view prefix = "pulsar-sound:";
  if (application::is_color_card_locator(text)) {
    std::string hex = text.substr(application::color_card_locator_prefix.size());
    for (auto& ch : hex) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return "Color #" + hex;
  }
  if (text.rfind(prefix, 0) != 0) return locator.filename().string();
  std::string name = text.substr(prefix.size());
  if (const auto found = sound_display_names().find(name); found != sound_display_names().end()) return found->second;
  bool word_start = true;
  for (auto& ch : name) {
    if (ch == '_' || ch == '-') { ch = ' '; word_start = true; continue; }
    if (word_start && ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    word_start = false;
  }
  return name;
}

constexpr std::int64_t display_quantum_denominator = 1'000'000;

std::string time_text(domain::MediaTime value) {
  const auto seconds = value.seconds();
  auto whole = seconds.numerator() / seconds.denominator();
  const auto remainder = seconds.numerator() % seconds.denominator();
  auto millis = static_cast<std::int64_t>(
      std::llround(static_cast<long double>(remainder) * 1000.0L /
                   static_cast<long double>(seconds.denominator())));
  if (millis >= 1000) {
    ++whole;
    millis = 0;
  }
  std::ostringstream text;
  text << std::setfill('0') << std::setw(2) << whole / 3600 << ':' << std::setw(2)
       << (whole / 60) % 60 << ':' << std::setw(2) << whole % 60 << '.' << std::setw(3)
       << millis;
  return text.str();
}

} // namespace


const domain::Clip& require_clip(const application::ProjectDocument& document, domain::ClipId id) {
  const auto* clip = document.timeline().find_clip(id);
  if (clip == nullptr) throw std::invalid_argument{"clip no longer exists"};
  return *clip;
}

std::optional<domain::MediaTime> TimelineViewport::time_at(double pixel, double width) const {
  if (!std::isfinite(pixel) || !std::isfinite(width) || width <= 0.0) {
    return std::nullopt;
  }
  if (duration_ <= domain::MediaTime::zero()) return view_start_;
  const double bounded = std::clamp(pixel, 0.0, width);
  const long double fraction = static_cast<long double>(bounded / width);
  const long double seconds = static_cast<long double>(duration_.value()) *
                              static_cast<long double>(duration_.time_base().numerator()) /
                              static_cast<long double>(duration_.time_base().denominator());
  const long double quantized = std::round(seconds * fraction * display_quantum_denominator);
  if (quantized <= 0.0L) return view_start_;
  if (quantized >= seconds * display_quantum_denominator) return view_start_ + duration_;
  if (quantized > static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
    return view_start_ + duration_;
  }
  return view_start_ + domain::MediaTime{static_cast<std::int64_t>(quantized),
                                         domain::Rational{1, display_quantum_denominator}};
}

double TimelineViewport::pixel_at(domain::MediaTime time, double width) const {
  return std::clamp(pixel_at_unclamped(time, width), 0.0, std::max(0.0, width));
}

double TimelineViewport::pixel_at_unclamped(domain::MediaTime time, double width) const {
  if (!std::isfinite(width) || width <= 0.0 || duration_ <= domain::MediaTime::zero()) return 0.0;
  const auto offset = time - view_start_;
  const long double numerator = static_cast<long double>(offset.value()) *
                                static_cast<long double>(offset.time_base().numerator()) /
                                static_cast<long double>(offset.time_base().denominator());
  const long double denominator = static_cast<long double>(duration_.value()) *
                                  static_cast<long double>(duration_.time_base().numerator()) /
                                  static_cast<long double>(duration_.time_base().denominator());
  if (denominator <= 0.0L) return 0.0;
  return static_cast<double>((numerator / denominator) * width);
}

void register_builtin_sound_display_name(std::string sound_id, std::string name) {
  sound_display_names()[std::move(sound_id)] = std::move(name);
}

EditorSession::EditorSession()
    : EditorSession{[](const std::filesystem::path&, int) {
        return std::unique_ptr<application::PlaybackMediaWorker>{};
      }} {}

EditorSession::EditorSession(application::PlaybackWorkerFactory worker_factory,
                             application::AudioPlaybackSourceFactory audio_source_factory,
                             std::unique_ptr<application::AudioOutput> audio_output,
                             std::unique_ptr<application::CompositionPreview> composition_preview)
    : processor_{document_},
      audio_output_{std::move(audio_output)},
      coordinator_{clock_, std::move(worker_factory), std::move(audio_source_factory),
                   audio_output_.get(), std::move(composition_preview)} {
  synchronize_document_state();
}

EditorSession::EditorSession(application::ProjectRestoreState restore_state,
                             application::PlaybackWorkerFactory worker_factory,
                             application::AudioPlaybackSourceFactory audio_source_factory,
                             std::unique_ptr<application::AudioOutput> audio_output,
                             std::unique_ptr<application::CompositionPreview> composition_preview)
    : document_{application::ProjectDocument::restore(std::move(restore_state))},
      processor_{document_},
      audio_output_{std::move(audio_output)},
      coordinator_{clock_, std::move(worker_factory), std::move(audio_source_factory),
                   audio_output_.get(), std::move(composition_preview)} {
  synchronize_document_state();
}

domain::MediaTime EditorSession::video_track_end() const {
  const auto* track = document_.timeline().find_track(document_.video_track_id());
  auto end = domain::MediaTime::zero();
  if (track != nullptr) {
    for (const auto& clip : track->clips()) end = std::max(end, clip.timeline_range().end());
  }
  return end;
}

domain::ClipId EditorSession::insert_audio_asset(domain::MediaId media, domain::TrackId audio_track,
                                                 domain::MediaTime timeline_start) {
  const auto asset = document_.find_asset(media);
  if (!asset.has_value()) throw VideoIngestException{VideoIngestError::UnknownAsset, "unknown media asset"};
  if (asset->has_video() || !asset->audio().has_value()) {
    throw VideoIngestException{VideoIngestError::NotAudioAsset, "only audio-only media can be dropped on an audio track"};
  }
  if (asset->availability() != application::MediaAvailability::Available) {
    throw VideoIngestException{VideoIngestError::UnavailableAsset, "media asset is unavailable"};
  }
  const auto* track = document_.timeline().find_track(audio_track);
  if (track == nullptr || track->kind() != domain::TrackKind::Audio) {
    throw VideoIngestException{VideoIngestError::NotAnAudioTrack, "destination is not an audio track"};
  }
  const auto duration = asset->audio()->duration.has_value() ? *asset->audio()->duration : asset->playable_duration();
  if (duration <= domain::MediaTime::zero() || timeline_start < domain::MediaTime::zero()) {
    throw VideoIngestException{VideoIngestError::InvalidDuration, "media has no usable duration or start is negative"};
  }
  const domain::TimeRange destination{timeline_start, timeline_start + duration};
  for (const auto& clip : track->clips()) {
    if (destination.overlaps(clip.timeline_range())) {
      throw VideoIngestException{VideoIngestError::NoSpace, "not enough room on the audio track"};
    }
  }
  const auto clip = processor_.add_clip(audio_track, media, domain::TimeRange{domain::MediaTime::zero(), duration}, timeline_start);
  selected_clip_ = clip;
  synchronize_document_state();
  return clip;
}

domain::ClipId EditorSession::insert_audio_asset_auto(domain::MediaId media, domain::MediaTime timeline_start,
                                                      bool force_new_track) {
  const auto asset = document_.find_asset(media);
  if (!asset.has_value()) throw VideoIngestException{VideoIngestError::UnknownAsset, "unknown media asset"};
  if (asset->has_video() || !asset->audio().has_value()) {
    throw VideoIngestException{VideoIngestError::NotAudioAsset, "only audio-only media goes on an audio track"};
  }
  const auto duration = asset->audio()->duration.has_value() ? *asset->audio()->duration : asset->playable_duration();
  if (duration <= domain::MediaTime::zero() || timeline_start < domain::MediaTime::zero()) {
    throw VideoIngestException{VideoIngestError::InvalidDuration, "media has no usable duration or start is negative"};
  }
  const domain::TimeRange destination{timeline_start, timeline_start + duration};
  std::optional<domain::TrackId> free_track;
  if (!force_new_track) {
    for (const auto& track : document_.timeline().tracks()) {
      if (track.kind() != domain::TrackKind::Audio) continue;
      const bool busy = std::any_of(track.clips().begin(), track.clips().end(),
                                    [&](const auto& clip) { return destination.overlaps(clip.timeline_range()); });
      if (!busy) { free_track = track.id(); break; }
    }
  }
  std::optional<domain::ClipId> clip;
  processor_.run_as_one_edit([&] {
    const auto track = free_track.has_value() ? *free_track : processor_.add_track(domain::TrackKind::Audio);
    clip = insert_audio_asset(media, track, timeline_start);
  });
  return *clip;
}

domain::ClipId EditorSession::insert_video_asset_at_playhead(domain::MediaId media) {
  return insert_video_asset(media, playback_position());
}

domain::ClipId EditorSession::append_video_asset(domain::MediaId media) {
  return insert_video_asset(media, video_track_end());
}

domain::ClipId EditorSession::insert_video_asset(domain::MediaId media,
                                                  domain::MediaTime timeline_start,
                                                  std::optional<domain::TrackId> target) {
  const auto asset = document_.find_asset(media);
  if (!asset.has_value()) {
    throw VideoIngestException{VideoIngestError::UnknownAsset, "unknown media asset"};
  }
  if (!asset->has_video()) {
    throw VideoIngestException{VideoIngestError::NotVideoAsset,
                               "audio-only media cannot be placed on the video track"};
  }
  if (asset->availability() != application::MediaAvailability::Available) {
    throw VideoIngestException{VideoIngestError::UnavailableAsset, "media asset is unavailable"};
  }
  const auto duration = asset->default_clip_duration();
  if (duration <= domain::MediaTime::zero() || timeline_start < domain::MediaTime::zero()) {
    throw VideoIngestException{VideoIngestError::InvalidDuration,
                               "media has no usable duration or start is negative"};
  }
  const domain::TimeRange destination{timeline_start, timeline_start + duration};
  const auto track_id = target.value_or(document_.video_track_id());
  const auto* track = document_.timeline().find_track(track_id);
  if (!track || track->kind() != domain::TrackKind::Video) {
    throw VideoIngestException{VideoIngestError::NotAVisualTrack, "destination is not a visual track"};
  }
  for (const auto& clip : track->clips()) {
    if (destination.overlaps(clip.timeline_range())) {
      throw VideoIngestException{VideoIngestError::NoSpace,
                                 "not enough room on the visual track"};
    }
  }
  const auto clip = processor_.add_clip(track_id, media, domain::TimeRange{domain::MediaTime::zero(), duration},
                                         timeline_start);
  selected_clip_ = clip;  // set before sync so the rebuilt read model marks it
  synchronize_document_state();
  return clip;
}

domain::TrackId EditorSession::add_visual_track() {
  const auto id = processor_.add_track(domain::TrackKind::Video);
  synchronize_document_state();
  return id;
}

void EditorSession::remove_empty_visual_track(domain::TrackId track) {
  processor_.remove_empty_visual_track(track);
  synchronize_document_state();
}

void EditorSession::reorder_visual_track(domain::TrackId track, std::size_t index) {
  processor_.reorder_visual_track(track, index);
  synchronize_document_state();
}

void EditorSession::move_clip_to_visual_track(domain::ClipId clip, domain::TrackId track) {
  const auto* source = document_.timeline().find_clip(clip);
  const auto* target = document_.timeline().find_track(track);
  if (!source || !target || target->kind() != domain::TrackKind::Video) {
    throw std::invalid_argument{"clip or visual target does not exist"};
  }
  processor_.move_clip(clip, track, source->timeline_start());
  selected_clip_ = clip;
  synchronize_document_state();
}

domain::MediaTime EditorSession::timeline_duration() const noexcept {
  return document_.timeline().duration();
}

void EditorSession::synchronize_document_state() {
  if (synchronized_document_generation_.has_value() &&
      synchronized_document_generation_.value() == document_.document_generation()) {
    ++sync_skipped_count_;
    return;
  }
  const auto start = std::chrono::steady_clock::now();
  coordinator_.set_snapshot(document_.snapshot());
  synchronized_document_generation_ = document_.document_generation();
  rebuild_read_model();
  sync_durations_.push(std::chrono::steady_clock::now() - start);
  ++sync_performed_count_;
}

EditorSyncDiagnostics EditorSession::synchronization_diagnostics() const {
  EditorSyncDiagnostics diagnostics;
  diagnostics.performed_count = sync_performed_count_;
  diagnostics.skipped_count = sync_skipped_count_;
  diagnostics.recent_durations = sync_durations_;
  return diagnostics;
}

application::ProjectSaveResult EditorSession::save_project(
    const std::filesystem::path& destination,
    const application::ProjectRepository& repository) {
  const application::ProjectSnapshot snapshot = document_.snapshot();
  const application::ProjectSaveResult result = repository.save(destination, snapshot);
  document_.mark_history_state_saved(snapshot.history_state_id);
  return result;
}

void EditorSession::rebuild_read_model() {
  views_.clear();
  track_views_.clear();
  const auto& timeline = document_.timeline();
  // Viewer stack is bottom-to-top, timeline lanes are top-to-bottom.
  for (auto it = document_.visual_track_ids().rbegin(); it != document_.visual_track_ids().rend(); ++it) {
    track_views_.push_back(TimelineTrackView{*it, domain::TrackKind::Video, track_views_.size()});
  }
  for (const auto& track : timeline.tracks()) {
    if (track.kind() == domain::TrackKind::Audio) {
      track_views_.push_back(TimelineTrackView{track.id(), track.kind(), track_views_.size()});
    }
  }
  for (const auto& track_view : track_views_) {
    const auto& track = *timeline.find_track(track_view.id);
    const auto track_index = track_view.index;
    for (const auto& clip : track.clips()) {
      const auto asset = document_.find_asset(clip.media_id());
      TimelineClipView view{clip.id(), track.id(), track.kind(), track_index,
                            std::to_string(clip.id().value()),
                            asset.has_value() ? clip_display_name(asset->source_locator()) : "Offline",
                            time_text(clip.timeline_range().start()),
                            time_text(clip.timeline_range().end()), time_text(clip.duration()),
                            !asset.has_value() || asset->availability() == application::MediaAvailability::Unavailable,
                            selected_clip_.has_value() && selected_clip_.value() == clip.id(),
                             asset.has_value() && asset->audio().has_value() &&
                                 (track.kind() == domain::TrackKind::Audio || track.id() == document_.video_track_id())};
      views_.push_back(std::move(view));
    }
  }
  if (selected_title_.has_value() && document_.find_title(*selected_title_) == nullptr) selected_title_.reset();
  if (selected_clip_.has_value() &&
      std::none_of(views_.begin(), views_.end(), [this](const auto& view) {
        return view.id == selected_clip_.value();
      })) {
    selected_clip_.reset();
    for (auto& view : views_) view.selected = false;
  }
  ++read_model_generation_;
}

void EditorSession::play() { coordinator_.play(); }

void EditorSession::pause() { coordinator_.pause(); }

application::TimelinePresentation EditorSession::update_playback() { return coordinator_.update(); }

application::PlaybackIntent EditorSession::playback_intent() const noexcept { return coordinator_.intent(); }

void EditorSession::seek(domain::MediaTime position) {
  coordinator_.seek(position);
}


domain::MediaTime EditorSession::playback_position() const {
  return coordinator_.timeline_position();
}

namespace {
[[nodiscard]] domain::MediaTime abs_diff(domain::MediaTime a, domain::MediaTime b) {
  return a < b ? b - a : a - b;
}
} // namespace

// Snap candidates: every other clip's start/end (any track), the playhead, zero.
// O(clips) and only evaluated during an editorial gesture.
static std::vector<domain::MediaTime> snap_edges(const application::ProjectDocument& document,
                                                 std::optional<domain::ClipId> exclude,
                                                 domain::MediaTime playhead) {
  std::vector<domain::MediaTime> edges;
  edges.push_back(domain::MediaTime::zero());
  edges.push_back(playhead);
  for (const auto& track : document.timeline().tracks()) {
    for (const auto& clip : track.clips()) {
      if (exclude.has_value() && clip.id() == *exclude) continue;
      edges.push_back(clip.timeline_range().start());
      edges.push_back(clip.timeline_range().end());
    }
  }
  return edges;
}

MoveResolution EditorSession::resolve_move(domain::ClipId clip_id, domain::MediaTime press_time,
                                           domain::MediaTime current_time,
                                           domain::MediaTime tolerance) const {
  const auto& original = require_clip(document_, clip_id);
  const auto duration = original.timeline_range().end() - original.timeline_range().start();
  const auto original_start = original.timeline_range().start();
  const auto original_end = original.timeline_range().end();

  // A clip can never pass through or sit on another one: its start is confined
  // to [lo, hi], the free span between its immediate neighbours on its own
  // track (and zero). Exact MediaTime; nothing else is ever moved.
  const domain::Track* track = nullptr;
  for (const auto& candidate_track : document_.timeline().tracks()) {
    if (candidate_track.find_clip(clip_id) != nullptr) { track = &candidate_track; break; }
  }
  auto lo = domain::MediaTime::zero();
  std::optional<domain::MediaTime> hi;
  if (track != nullptr) {
    for (const auto& other : track->clips()) {
      if (other.id() == clip_id) continue;
      if (other.timeline_range().end() <= original_start) lo = std::max(lo, other.timeline_range().end());
      else if (other.timeline_range().start() >= original_end) {
        const auto limit = other.timeline_range().start() - duration;
        hi = hi.has_value() ? std::min(*hi, limit) : limit;
      }
    }
  }
  const auto confine = [&](domain::MediaTime start) {
    if (start < lo) start = lo;
    if (hi.has_value() && start > *hi) start = *hi;
    return start;
  };
  const auto delta = current_time - press_time;
  const auto desired = confine(original.timeline_start() + delta);
  MoveResolution result{desired, false, desired};
  if (tolerance <= domain::MediaTime::zero()) return result;

  bool found = false;
  domain::MediaTime best_distance = tolerance;
  domain::MediaTime best_start = desired;
  domain::MediaTime best_edge = desired;
  for (const auto& edge : snap_edges(document_, clip_id, playback_position())) {
    // Align this clip's start to the edge, or its end to the edge.
    for (const bool by_end : {false, true}) {
      const auto moved = by_end ? desired + duration : desired;
      const auto distance = abs_diff(moved, edge);
      if (distance > tolerance) continue;
      if (found && !(distance < best_distance)) continue;
      const auto candidate = by_end ? edge - duration : edge;
      if (candidate < lo || (hi.has_value() && candidate > *hi)) continue;  // would overlap a neighbour
      found = true;
      best_distance = distance;
      best_start = candidate;
      best_edge = edge;
    }
  }
  if (found) result = MoveResolution{best_start, true, best_edge};
  return result;
}

EdgeResolution EditorSession::resolve_trim_edge(domain::ClipId clip_id, domain::MediaTime target,
                                                domain::MediaTime tolerance) const {
  EdgeResolution result{target, false};
  if (tolerance <= domain::MediaTime::zero()) return result;
  bool found = false;
  domain::MediaTime best = tolerance;
  for (const auto& edge : snap_edges(document_, clip_id, playback_position())) {
    const auto distance = abs_diff(target, edge);
    if (distance > tolerance || (found && !(distance < best))) continue;
    found = true;
    best = distance;
    result = EdgeResolution{edge, true};
  }
  return result;
}

void EditorSession::move_clip(domain::ClipId clip_id, domain::MediaTime press_time,
                              domain::MediaTime current_time, domain::MediaTime snap_tolerance) {
  const auto resolution = resolve_move(clip_id, press_time, current_time, snap_tolerance);
  const auto& original = require_clip(document_, clip_id);
  // Re-express the resolved start as an exact delta so the existing move path
  // (and its validation) stays the single place that mutates.
  move_clip(clip_id, original.timeline_start(), resolution.new_start);
}

void EditorSession::trim_clip_start(domain::ClipId clip_id, domain::MediaTime target,
                                    domain::MediaTime snap_tolerance) {
  trim_clip_start(clip_id, resolve_trim_edge(clip_id, target, snap_tolerance).time);
}

void EditorSession::trim_clip_end(domain::ClipId clip_id, domain::MediaTime target,
                                  domain::MediaTime snap_tolerance) {
  trim_clip_end(clip_id, resolve_trim_edge(clip_id, target, snap_tolerance).time);
}

void EditorSession::move_clip(domain::ClipId clip_id, double press_pixel, double current_pixel,
                              double width) {
  const auto press = time_at(press_pixel, width);
  const auto current = time_at(current_pixel, width);
  if (!press.has_value() || !current.has_value()) throw std::invalid_argument{"invalid move gesture"};
  move_clip(clip_id, *press, *current);
}

void EditorSession::move_clip(domain::ClipId clip_id, domain::MediaTime press_time,
                              domain::MediaTime current_time) {
  const auto& original = require_clip(document_, clip_id);
  const auto delta = current_time - press_time;
  std::optional<domain::TrackId> source_track;
  for (const auto& track : document_.timeline().tracks()) {
    if (track.find_clip(clip_id) != nullptr) {
      source_track = track.id();
      break;
    }
  }
  if (!source_track.has_value()) throw std::invalid_argument{"clip track no longer exists"};
  processor_.move_clip(clip_id, *source_track, original.timeline_start() + delta);
  selected_clip_ = clip_id;
  synchronize_document_state();
}

void EditorSession::trim_clip_start(domain::ClipId clip_id, double pixel, double width) {
  const auto target = time_at(pixel, width);
  if (!target.has_value()) throw std::invalid_argument{"invalid trim gesture"};
  trim_clip_start(clip_id, *target);
}

void EditorSession::trim_clip_start(domain::ClipId clip_id, domain::MediaTime timeline_start) {
  processor_.trim_clip_start(clip_id, timeline_start);
  synchronize_document_state();
}

void EditorSession::trim_clip_end(domain::ClipId clip_id, double pixel, double width) {
  const auto target = time_at(pixel, width);
  if (!target.has_value()) throw std::invalid_argument{"invalid trim gesture"};
  trim_clip_end(clip_id, *target);
}

void EditorSession::trim_clip_end(domain::ClipId clip_id, domain::MediaTime timeline_end) {
  processor_.trim_clip_end(clip_id, timeline_end);
  synchronize_document_state();
}

void EditorSession::set_clip_audio_gain(domain::ClipId clip_id, double gain_db) {
  processor_.set_clip_audio_gain(clip_id, gain_db);
  synchronize_document_state();
}

void EditorSession::set_clip_muted(domain::ClipId clip_id, bool muted) {
  processor_.set_clip_muted(clip_id, muted);
  synchronize_document_state();
}

bool EditorSession::can_split() const {
  if (!selected_clip_.has_value()) return false;
  const auto* clip = document_.timeline().find_clip(*selected_clip_);
  if (clip == nullptr) return false;
  const auto position = playback_position();
  return position > clip->timeline_range().start() && position < clip->timeline_range().end();
}

domain::ClipId EditorSession::split_selected_at_playhead() {
  if (!can_split()) throw std::invalid_argument{"split point must be inside the clip"};
  const auto id = *selected_clip_;
  const auto right = processor_.split_clip(id, playback_position());
  synchronize_document_state();
  return right;
}

void EditorSession::select_title(std::optional<domain::TitleId> title) {
  if (title.has_value() && document_.find_title(*title) == nullptr) title.reset();
  const bool changed = selected_title_ != title || (title.has_value() && selected_clip_.has_value());
  selected_title_ = title;
  if (title.has_value() && selected_clip_.has_value()) {
    selected_clip_.reset();
    rebuild_read_model();
  } else if (changed) {
    ++read_model_generation_;
  }
}

domain::TitleId EditorSession::add_title_at_playhead() {
  const auto total = timeline_duration();
  const auto start = playback_position();
  if (total <= domain::MediaTime::zero() || start >= total) {
    throw TitleException{TitleError::NoVideo, "there is no video at the playhead"};
  }
  auto end = start + domain::MediaTime{3, domain::Rational{1, 1}};
  if (end > total) end = total;
  for (const auto& other : document_.titles()) {
    if (other.range().contains(start)) throw TitleException{TitleError::NoSpace, "playhead is inside another title"};
    if (other.start() > start && other.start() < end) end = other.start();
  }
  if (end - start < domain::MediaTime{1, domain::Rational{1, 4}}) {
    throw TitleException{TitleError::NoSpace, "no room for a title here"};
  }
  const auto id = document_.next_title_id();
  const auto created = processor_.add_title(domain::Title{id, "Título", domain::TimeRange{start, end}});
  selected_title_ = created;
  selected_clip_.reset();
  synchronize_document_state();
  rebuild_read_model();
  return created;
}

void EditorSession::replace_title(domain::Title title) {
  if (document_.find_title(title.id()) == nullptr) throw TitleException{TitleError::Unknown, "unknown title"};
  processor_.replace_title(std::move(title));
  synchronize_document_state();
}

void EditorSession::set_clip_fades(domain::ClipId clip, domain::MediaTime fade_in, domain::MediaTime fade_out) {
  const auto* existing = document_.timeline().find_clip(clip);
  if (existing == nullptr) throw FadeException{FadeError::NoClip, "unknown clip"};
  if (!clip_has_audio(clip)) throw FadeException{FadeError::NoAudio, "the clip has no audio to fade"};
  const domain::MediaTime maximum{domain::Clip::maximum_fade_seconds, domain::Rational{1, 1}};
  if (fade_in < domain::MediaTime::zero() || fade_out < domain::MediaTime::zero() || fade_in > maximum || fade_out > maximum) {
    throw FadeException{FadeError::TooLong, "fade out of range"};
  }
  processor_.set_clip_fades(clip, fade_in, fade_out);
  synchronize_document_state();
}

void EditorSession::duplicate_selected() {
  if (selected_title_.has_value()) {
    const auto* title = document_.find_title(*selected_title_);
    if (title == nullptr) throw DuplicateException{DuplicateError::NothingSelected, "no title selected"};
    const auto length = title->end() - title->start();
    const domain::TimeRange range{title->end(), title->end() + length};
    for (const auto& other : document_.titles()) {
      if (other.id() != title->id() && other.range().overlaps(range)) {
        throw DuplicateException{DuplicateError::NoSpace, "no room after the title"};
      }
    }
    const auto id = document_.next_title_id();
    const auto copy = processor_.add_title(domain::Title{id, title->text(), range, title->size_permille(), title->color_rgba(),
                                                         title->horizontal(), title->vertical(), title->shadow(),
                                                         title->background(), title->opacity_percent()});
    selected_title_ = copy;
    selected_clip_.reset();
    synchronize_document_state();
    rebuild_read_model();
    return;
  }
  if (!selected_clip_.has_value()) throw DuplicateException{DuplicateError::NothingSelected, "nothing selected"};
  const auto* original = document_.timeline().find_clip(*selected_clip_);
  if (original == nullptr) throw DuplicateException{DuplicateError::NothingSelected, "nothing selected"};
  const domain::Clip source = *original;
  const domain::Track* track = nullptr;
  for (const auto& candidate : document_.timeline().tracks()) {
    if (candidate.find_clip(source.id()) != nullptr) { track = &candidate; break; }
  }
  if (track == nullptr) throw DuplicateException{DuplicateError::NothingSelected, "nothing selected"};
  const auto destination_track = track->id();
  const auto start = source.timeline_range().end();
  const domain::TimeRange destination{start, start + source.duration()};
  for (const auto& other : track->clips()) {
    if (destination.overlaps(other.timeline_range())) {
      throw DuplicateException{DuplicateError::NoSpace, "no room after the clip"};
    }
  }
  domain::ClipId copy = source.id();
  processor_.run_as_one_edit([&] {
    copy = processor_.add_clip(destination_track, source.media_id(), source.source_range(), start);
    if (source.audio_gain_db() != 0.0) processor_.set_clip_audio_gain(copy, source.audio_gain_db());
    if (source.muted()) processor_.set_clip_muted(copy, true);
    if (source.fade_in() > domain::MediaTime::zero() || source.fade_out() > domain::MediaTime::zero()) {
      processor_.set_clip_fades(copy, source.fade_in(), source.fade_out());
    }
  });
  selected_clip_ = copy;
  synchronize_document_state();
}

void EditorSession::delete_selected_title() {
  if (!selected_title_.has_value()) throw TitleException{TitleError::Unknown, "no title selected"};
  processor_.remove_title(*selected_title_);
  selected_title_.reset();
  synchronize_document_state();
}

void EditorSession::delete_selected() {
  if (!selected_clip_.has_value()) throw std::invalid_argument{"no clip selected"};
  processor_.remove_clip(*selected_clip_);
  selected_clip_.reset();
  synchronize_document_state();
}

namespace {
struct RippleMove {
  domain::ClipId clip;
  domain::TrackId track;
  domain::MediaTime new_start;
};
struct RipplePlan {
  domain::ClipId removed;
  std::vector<RippleMove> moves;  // ascending timeline start within each track
  std::vector<domain::Title> shifted_titles;  // titles that start at/after the cut, already moved
};
}

static RipplePlan plan_ripple(const application::ProjectDocument& document, domain::ClipId removed_id) {
  const domain::Clip* removed = document.timeline().find_clip(removed_id);
  if (removed == nullptr) throw RippleDeleteException{RippleDeleteError::NothingSelected, "no clip selected"};
  const auto t1 = removed->timeline_range().end();
  const auto duration = t1 - removed->timeline_start();
  RipplePlan plan{removed_id, {}, {}};
  for (const auto& track : document.timeline().tracks()) {
    struct Span { domain::MediaTime start; domain::MediaTime end; };
    std::vector<Span> final_layout;
    std::vector<RippleMove> track_moves;
    for (const auto& clip : track.clips()) {
      if (clip.id() == removed_id) continue;
      if (clip.timeline_start() >= t1) {
        const auto start = clip.timeline_start() - duration;
        track_moves.push_back(RippleMove{clip.id(), track.id(), start});
        final_layout.push_back(Span{start, clip.timeline_range().end() - duration});
      } else {
        final_layout.push_back(Span{clip.timeline_start(), clip.timeline_range().end()});
      }
    }
    std::sort(final_layout.begin(), final_layout.end(), [](const Span& a, const Span& b) { return a.start < b.start; });
    for (std::size_t i = 1; i < final_layout.size(); ++i) {
      if (final_layout[i].start < final_layout[i - 1].end) {
        throw RippleDeleteException{RippleDeleteError::WouldOverlap, "closing the gap would overlap a clip"};
      }
    }
    std::sort(track_moves.begin(), track_moves.end(),
              [](const RippleMove& a, const RippleMove& b) { return a.new_start < b.new_start; });
    plan.moves.insert(plan.moves.end(), track_moves.begin(), track_moves.end());
  }
  // The title track follows the same rule: titles that start at/after the cut move with it.
  {
    struct Span { domain::MediaTime start; domain::MediaTime end; };
    std::vector<Span> final_layout;
    for (const auto& title : document.titles()) {
      if (title.start() >= t1) {
        const auto moved = title.with_range(domain::TimeRange{title.start() - duration, title.end() - duration});
        plan.shifted_titles.push_back(moved);
        final_layout.push_back(Span{moved.start(), moved.end()});
      } else {
        final_layout.push_back(Span{title.start(), title.end()});
      }
    }
    std::sort(final_layout.begin(), final_layout.end(), [](const Span& a, const Span& b) { return a.start < b.start; });
    for (std::size_t i = 1; i < final_layout.size(); ++i) {
      if (final_layout[i].start < final_layout[i - 1].end) {
        throw RippleDeleteException{RippleDeleteError::WouldOverlap, "closing the gap would overlap a title"};
      }
    }
    std::sort(plan.shifted_titles.begin(), plan.shifted_titles.end(),
              [](const domain::Title& a, const domain::Title& b) { return a.start() < b.start(); });
  }
  return plan;
}

bool EditorSession::can_ripple_delete() const {
  if (!selected_clip_.has_value()) return false;
  try {
    (void)plan_ripple(document_, *selected_clip_);
    return true;
  } catch (const RippleDeleteException&) {
    return false;
  }
}

void EditorSession::ripple_delete_selected() {
  if (!selected_clip_.has_value()) throw RippleDeleteException{RippleDeleteError::NothingSelected, "no clip selected"};
  // Everything that can be refused is checked before the first command runs. Moves go in
  // ascending order per track, so each clip only ever moves into space already vacated.
  const auto plan = plan_ripple(document_, *selected_clip_);
  processor_.run_as_one_edit([&] {
    processor_.remove_clip(plan.removed);
    for (const auto& move : plan.moves) processor_.move_clip(move.clip, move.track, move.new_start);
    for (const auto& title : plan.shifted_titles) processor_.replace_title(title);
  });
  selected_clip_.reset();
  synchronize_document_state();
}

bool EditorSession::trim_selected_to_playhead(bool trim_start) {
  if (!can_split()) return false;  // selected clip with the playhead strictly inside
  const auto id = *selected_clip_;
  const auto position = playback_position();
  if (trim_start) trim_clip_start(id, position); else trim_clip_end(id, position);
  return true;
}

bool EditorSession::select_adjacent_clip(bool next) {
  struct Entry { domain::MediaTime start; std::size_t track; domain::ClipId id; };
  std::vector<Entry> order;
  const auto& tracks = document_.timeline().tracks();
  for (std::size_t index = 0; index < tracks.size(); ++index) {
    for (const auto& clip : tracks[index].clips()) order.push_back(Entry{clip.timeline_start(), index, clip.id()});
  }
  if (order.empty()) return false;
  std::sort(order.begin(), order.end(), [](const Entry& a, const Entry& b) {
    if (a.start != b.start) return a.start < b.start;
    if (a.track != b.track) return a.track < b.track;
    return a.id.value() < b.id.value();
  });
  std::size_t target = next ? 0 : order.size() - 1;
  if (selected_clip_.has_value()) {
    const auto current = std::find_if(order.begin(), order.end(), [&](const Entry& e) { return e.id == *selected_clip_; });
    if (current != order.end()) {
      const auto index = static_cast<std::size_t>(current - order.begin());
      if (next) { if (index + 1 >= order.size()) return false; target = index + 1; }
      else { if (index == 0) return false; target = index - 1; }
    }
  }
  select_clip(order[target].id);
  return true;
}

std::optional<domain::MediaTime> EditorSession::clip_timeline_start(domain::ClipId clip) const {
  const auto* found = document_.timeline().find_clip(clip);
  if (found == nullptr) return std::nullopt;
  return found->timeline_start();
}

namespace {
struct SeparablePlan {
  const domain::Clip* clip;
  application::MediaAsset asset;
};
}

static SeparablePlan plan_separation(const application::ProjectDocument& document, domain::ClipId clip_id) {
  const domain::Clip* clip = nullptr;
  bool on_video_track = false;
  for (const auto& track : document.timeline().tracks()) {
    if (const auto* found = track.find_clip(clip_id)) {
      clip = found;
       on_video_track = track.id() == document.video_track_id();
      break;
    }
  }
  if (clip == nullptr) throw SeparateAudioException{SeparateAudioError::UnknownClip, "unknown clip"};
  if (!on_video_track) throw SeparateAudioException{SeparateAudioError::NotAVideoTrackClip, "clip is not on the video track"};
  const auto asset = document.find_asset(clip->media_id());
  if (!asset.has_value()) throw SeparateAudioException{SeparateAudioError::UnavailableAsset, "media is unavailable"};
  if (asset->availability() != application::MediaAvailability::Available) {
    throw SeparateAudioException{SeparateAudioError::UnavailableAsset, "media is unavailable"};
  }
  if (!asset->audio().has_value()) throw SeparateAudioException{SeparateAudioError::NoEmbeddedAudio, "media has no audio"};
  if (clip->muted()) throw SeparateAudioException{SeparateAudioError::AlreadySilenced, "embedded audio is already silenced"};
  const auto audio_end = asset->audio()->duration.has_value() ? *asset->audio()->duration : asset->playable_duration();
  if (clip->source_range().end() > audio_end) {
    throw SeparateAudioException{SeparateAudioError::AudioShorterThanClip, "the audio stream is shorter than the clip"};
  }
  return SeparablePlan{clip, *asset};
}

bool EditorSession::clip_has_audio(domain::ClipId clip) const {
  const auto found = std::find_if(views_.begin(), views_.end(), [&](const auto& view) { return view.id == clip; });
  return found != views_.end() && found->has_audio &&
      (found->track_kind == domain::TrackKind::Audio || found->track_id == document_.video_track_id());
}

bool EditorSession::can_separate_audio(domain::ClipId video_clip) const {
  try {
    (void)plan_separation(document_, video_clip);
    return true;
  } catch (const SeparateAudioException&) {
    return false;
  }
}

domain::ClipId EditorSession::separate_audio(domain::ClipId video_clip) {
  // Everything that can be refused is checked before the first command runs.
  const auto plan = plan_separation(document_, video_clip);
  const auto media = plan.clip->media_id();
  const auto source_range = plan.clip->source_range();
  const auto timeline_range = plan.clip->timeline_range();
  const auto start = plan.clip->timeline_start();
  const double gain_db = plan.clip->audio_gain_db();

  std::optional<domain::TrackId> audio_track;
  for (const auto& track : document_.timeline().tracks()) {
    if (track.kind() != domain::TrackKind::Audio) continue;
    const bool free = std::none_of(track.clips().begin(), track.clips().end(), [&](const auto& other) {
      return timeline_range.overlaps(other.timeline_range());
    });
    if (free) { audio_track = track.id(); break; }
  }

  domain::ClipId audio_clip{1};
  processor_.run_as_one_edit([&] {
    if (!audio_track.has_value()) audio_track = processor_.add_track(domain::TrackKind::Audio);
    audio_clip = processor_.add_clip(*audio_track, media, source_range, start);
    if (gain_db != 0.0) processor_.set_clip_audio_gain(audio_clip, gain_db);
    processor_.set_clip_muted(video_clip, true);  // the embedded audio now lives on its own clip
  });
  selected_clip_ = audio_clip;
  synchronize_document_state();
  return audio_clip;
}

bool EditorSession::undo() {
  if (!processor_.undo()) return false;
  synchronize_document_state();
  return true;
}

bool EditorSession::redo() {
  if (!processor_.redo()) return false;
  synchronize_document_state();
  return true;
}

void EditorSession::select_clip(std::optional<domain::ClipId> clip) {
  if (clip.has_value()) selected_title_.reset();   // one selection at a time
  if (selected_clip_ == clip) return;
  selected_clip_ = clip;
  rebuild_read_model();
}

std::optional<domain::MediaTime> EditorSession::time_at(double pixel, double width) const {
  return TimelineViewport{timeline_duration()}.time_at(pixel, width);
}

double EditorSession::pixel_at(domain::MediaTime time, double width) const {
  return TimelineViewport{timeline_duration()}.pixel_at(time, width);
}

} // namespace pulsar::desktop
