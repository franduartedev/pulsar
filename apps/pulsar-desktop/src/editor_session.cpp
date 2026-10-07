#include "editor_session.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace pulsar::desktop {
namespace {

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

EditorSession::EditorSession()
    : EditorSession{[](const std::filesystem::path&, int) {
        return std::unique_ptr<application::PlaybackMediaWorker>{};
      }} {}

EditorSession::EditorSession(application::PlaybackWorkerFactory worker_factory,
                             application::AudioPlaybackSourceFactory audio_source_factory,
                             std::unique_ptr<application::AudioOutput> audio_output)
    : processor_{document_},
      audio_output_{std::move(audio_output)},
      coordinator_{clock_, std::move(worker_factory), std::move(audio_source_factory),
                   audio_output_.get()} {
  synchronize_document_state();
}

EditorSession::EditorSession(application::ProjectRestoreState restore_state,
                             application::PlaybackWorkerFactory worker_factory,
                             application::AudioPlaybackSourceFactory audio_source_factory,
                             std::unique_ptr<application::AudioOutput> audio_output)
    : document_{application::ProjectDocument::restore(std::move(restore_state))},
      processor_{document_},
      audio_output_{std::move(audio_output)},
      coordinator_{clock_, std::move(worker_factory), std::move(audio_source_factory),
                   audio_output_.get()} {
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

domain::ClipId EditorSession::insert_video_asset_at_playhead(domain::MediaId media) {
  return insert_video_asset(media, playback_position());
}

domain::ClipId EditorSession::append_video_asset(domain::MediaId media) {
  return insert_video_asset(media, video_track_end());
}

domain::ClipId EditorSession::insert_video_asset(domain::MediaId media,
                                                 domain::MediaTime timeline_start) {
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
  const auto duration = asset->playable_duration();
  if (duration <= domain::MediaTime::zero() || timeline_start < domain::MediaTime::zero()) {
    throw VideoIngestException{VideoIngestError::InvalidDuration,
                               "media has no usable duration or start is negative"};
  }
  const domain::TimeRange destination{timeline_start, timeline_start + duration};
  const auto* track = document_.timeline().find_track(document_.video_track_id());
  if (track != nullptr) {
    for (const auto& clip : track->clips()) {
      if (destination.overlaps(clip.timeline_range())) {
        throw VideoIngestException{VideoIngestError::NoSpace,
                                   "not enough room on the video track"};
      }
    }
  }
  const auto clip = processor_.add_clip(media, domain::TimeRange{domain::MediaTime::zero(), duration},
                                        timeline_start);
  selected_clip_ = clip;  // set before sync so the rebuilt read model marks it
  synchronize_document_state();
  return clip;
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
  for (std::size_t track_index = 0; track_index < timeline.tracks().size(); ++track_index) {
    const auto& track = timeline.tracks()[track_index];
    track_views_.push_back(TimelineTrackView{track.id(), track.kind(), track_index});
    for (const auto& clip : track.clips()) {
      const auto asset = document_.find_asset(clip.media_id());
      TimelineClipView view{clip.id(), track.id(), track.kind(), track_index,
                            std::to_string(clip.id().value()),
                            asset.has_value() ? asset->source_locator().filename().string() : "Offline",
                            time_text(clip.timeline_range().start()),
                            time_text(clip.timeline_range().end()), time_text(clip.duration()),
                            !asset.has_value() || asset->availability() == application::MediaAvailability::Unavailable,
                            selected_clip_.has_value() && selected_clip_.value() == clip.id()};
      views_.push_back(std::move(view));
    }
  }
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

void EditorSession::delete_selected() {
  if (!selected_clip_.has_value()) throw std::invalid_argument{"no clip selected"};
  processor_.remove_clip(*selected_clip_);
  selected_clip_.reset();
  synchronize_document_state();
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
