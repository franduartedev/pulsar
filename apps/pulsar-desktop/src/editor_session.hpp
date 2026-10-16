#pragma once

#include "duration_sample_ring.hpp"

#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/audio/audio_output.hpp>
#include <pulsar/application/playback/audio_playback_session.hpp>
#include <pulsar/application/playback/timeline_playback_coordinator.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/application/project/project_repository.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace pulsar::desktop {

struct TimelineClipView {
  domain::ClipId id;
  domain::TrackId track_id;
  domain::TrackKind track_kind;
  std::size_t track_index{0};
  std::string key;
  std::string name;
  std::string start;
  std::string end;
  std::string duration;
  bool missing{false};
  bool selected{false};
  bool has_audio{false};  // the clip's media carries an audio stream (so mute means something)
};

struct TimelineTrackView {
  domain::TrackId id;
  domain::TrackKind kind;
  std::size_t index{0};
};

// Bounded observation of synchronize_document_state() for D3 evidence.
// performed_count counts real synchronizations only; skipped_count counts
// early-return no-ops. Durations cover the full performed body (snapshot +
// generation update + read-model rebuild). Single-writer, no locks.
struct EditorSyncDiagnostics {
  std::uint64_t performed_count{0};
  std::uint64_t skipped_count{0};
  DurationSampleRing recent_durations;
};

class TimelineViewport {public:
  explicit TimelineViewport(domain::MediaTime duration) : duration_{duration} {}
  TimelineViewport(domain::MediaTime view_start, domain::MediaTime view_duration)
      : duration_{view_duration}, view_start_{view_start} {}
  [[nodiscard]] std::optional<domain::MediaTime> time_at(double pixel, double width) const;
  [[nodiscard]] double pixel_at(domain::MediaTime time, double width) const;
  [[nodiscard]] double pixel_at_unclamped(domain::MediaTime time, double width) const;

private:
  domain::MediaTime duration_;
  domain::MediaTime view_start_{domain::MediaTime::zero()};
};

// Why a video ingest request was refused. The document is never mutated when
// one of these is thrown.
enum class VideoIngestError {
  UnknownAsset,
  NotVideoAsset,
  UnavailableAsset,
  InvalidDuration,
  NoSpace,
  NotAudioAsset,
  NotAnAudioTrack,
  NotAVisualTrack,
};

class VideoIngestException final : public std::invalid_argument {
public:
  VideoIngestException(VideoIngestError code, const char* message)
      : std::invalid_argument{message}, code_{code} {}
  [[nodiscard]] VideoIngestError code() const noexcept { return code_; }

private:
  VideoIngestError code_;
};

// Magnetic snapping. All values are exact MediaTime; only the tolerance (a few
// screen pixels converted by the caller) is approximate, and it is used only
// to decide *whether* to snap, never as the resulting time.
struct MoveResolution {
  domain::MediaTime new_start;
  bool snapped{false};
  domain::MediaTime guide;  // the edge snapped to (valid when snapped)
};
struct EdgeResolution {
  domain::MediaTime time;
  bool snapped{false};
};

// Why "Separar audio" was refused. The document is never mutated.
enum class SeparateAudioError {
  UnknownClip,
  NotAVideoTrackClip,
  UnavailableAsset,
  NoEmbeddedAudio,
  AlreadySilenced,       // embedded audio is muted (also what a previous separation leaves)
  AudioShorterThanClip,  // the source's audio stream does not cover the clip's source range
};

class SeparateAudioException final : public std::invalid_argument {
public:
  SeparateAudioException(SeparateAudioError code, const char* message)
      : std::invalid_argument{message}, code_{code} {}
  [[nodiscard]] SeparateAudioError code() const noexcept { return code_; }

private:
  SeparateAudioError code_;
};

// Why "Eliminar y cerrar hueco" was refused. The document is never mutated.
enum class RippleDeleteError {
  NothingSelected,
  WouldOverlap,  // closing the gap would put a later clip on top of one that stays
};

class RippleDeleteException final : public std::invalid_argument {
public:
  RippleDeleteException(RippleDeleteError code, const char* message)
      : std::invalid_argument{message}, code_{code} {}
  [[nodiscard]] RippleDeleteError code() const noexcept { return code_; }

private:
  RippleDeleteError code_;
};

enum class TitleError {
  NoVideo,     // nothing to put a title on yet
  NoSpace,     // the playhead is inside another title / no room before the next one
  Unknown,
};

class TitleException final : public std::invalid_argument {
public:
  TitleException(TitleError code, const char* message) : std::invalid_argument{message}, code_{code} {}
  [[nodiscard]] TitleError code() const noexcept { return code_; }

private:
  TitleError code_;
};

enum class DuplicateError {
  NothingSelected,
  NoSpace,  // the copy goes right after the original and something else is already there
};

class DuplicateException final : public std::invalid_argument {
public:
  DuplicateException(DuplicateError code, const char* message) : std::invalid_argument{message}, code_{code} {}
  [[nodiscard]] DuplicateError code() const noexcept { return code_; }

private:
  DuplicateError code_;
};

enum class FadeError {
  NoClip,
  NoAudio,   // the clip has no sound to fade (a photo, a colour card, a silenced clip)
  TooLong,
};

class FadeException final : public std::invalid_argument {
public:
  FadeException(FadeError code, const char* message) : std::invalid_argument{message}, code_{code} {}
  [[nodiscard]] FadeError code() const noexcept { return code_; }

private:
  FadeError code_;
};

// Names shown on the timeline for the built-in sounds (the catalogue's interface-language names).
void register_builtin_sound_display_name(std::string sound_id, std::string name);

class EditorSession final {
public:
  EditorSession();
  explicit EditorSession(application::PlaybackWorkerFactory worker_factory,
                         application::AudioPlaybackSourceFactory audio_source_factory = {},
                         std::unique_ptr<application::AudioOutput> audio_output = {},
                         std::unique_ptr<application::CompositionPreview> composition_preview = {});
  // Builds a completely fresh session from loaded restore state: new document,
  // new processor/history, new coordinator, no selection. Throws without
  // mutating any existing session; callers replace atomically on success.
  explicit EditorSession(application::ProjectRestoreState restore_state,
                         application::PlaybackWorkerFactory worker_factory,
                         application::AudioPlaybackSourceFactory audio_source_factory = {},
                         std::unique_ptr<application::AudioOutput> audio_output = {},
                         std::unique_ptr<application::CompositionPreview> composition_preview = {});
  ~EditorSession() = default;
  EditorSession(const EditorSession&) = delete;
  EditorSession& operator=(const EditorSession&) = delete;
  EditorSession(EditorSession&&) = delete;
  EditorSession& operator=(EditorSession&&) = delete;

  [[nodiscard]] application::ProjectDocument& document() noexcept { return document_; }
  [[nodiscard]] const application::ProjectDocument& document() const noexcept { return document_; }
  [[nodiscard]] application::CommandProcessor& processor() noexcept { return processor_; }
  [[nodiscard]] const std::vector<TimelineClipView>& timeline_views() const noexcept { return views_; }
  [[nodiscard]] const std::vector<TimelineTrackView>& track_views() const noexcept { return track_views_; }
  [[nodiscard]] domain::TrackId add_visual_track();
  void remove_empty_visual_track(domain::TrackId track);
  void reorder_visual_track(domain::TrackId track, std::size_t bottom_to_top_index);
  void move_clip_to_visual_track(domain::ClipId clip, domain::TrackId track);
  [[nodiscard]] std::optional<domain::ClipId> selected_clip() const noexcept { return selected_clip_; }
  [[nodiscard]] domain::MediaTime timeline_duration() const noexcept;
  [[nodiscard]] std::uint64_t read_model_generation() const noexcept { return read_model_generation_; }
  [[nodiscard]] bool is_dirty() const noexcept { return document_.is_dirty(); }
  void play();
  void pause();
  [[nodiscard]] application::TimelinePresentation update_playback();
  [[nodiscard]] application::PlaybackIntent playback_intent() const noexcept;
  [[nodiscard]] const application::AudioMixPlaybackMetrics& audio_metrics() const noexcept {
    return coordinator_.audio_metrics();
  }
  [[nodiscard]] application::CompositionPreviewMetrics composition_metrics() const {
    return coordinator_.composition_metrics();
  }
  void seek(domain::MediaTime position);
  [[nodiscard]] domain::MediaTime playback_position() const;
  void move_clip(domain::ClipId clip, double press_pixel, double current_pixel, double width);
  void move_clip(domain::ClipId clip, domain::MediaTime press_time,
                 domain::MediaTime current_time);
  // Where a drag would land, with magnetic snapping to other clips' edges, the
  // playhead and zero. Never produces an overlap on the clip's own track and
  // never moves anything else. Pure: does not mutate the document.
  [[nodiscard]] MoveResolution resolve_move(domain::ClipId clip, domain::MediaTime press_time,
                                            domain::MediaTime current_time,
                                            domain::MediaTime tolerance) const;
  [[nodiscard]] EdgeResolution resolve_trim_edge(domain::ClipId clip, domain::MediaTime target,
                                                 domain::MediaTime tolerance) const;
  void move_clip(domain::ClipId clip, domain::MediaTime press_time,
                 domain::MediaTime current_time, domain::MediaTime snap_tolerance);
  void trim_clip_start(domain::ClipId clip, domain::MediaTime target, domain::MediaTime snap_tolerance);
  void trim_clip_end(domain::ClipId clip, domain::MediaTime target, domain::MediaTime snap_tolerance);
  void trim_clip_start(domain::ClipId clip, double pixel, double width);
  void trim_clip_start(domain::ClipId clip, domain::MediaTime timeline_start);
  void trim_clip_end(domain::ClipId clip, double pixel, double width);
  void trim_clip_end(domain::ClipId clip, domain::MediaTime timeline_end);
  void set_clip_audio_gain(domain::ClipId clip, double gain_db);
  void set_clip_muted(domain::ClipId clip, bool muted);
  // Linear audio fades of a clip with sound (zero = none). One undo step.
  void set_clip_fades(domain::ClipId clip, domain::MediaTime fade_in, domain::MediaTime fade_out);
  // Visual transform of a clip (position/scale/rotation/opacity). One undo step.
  void set_clip_visual_transform(domain::ClipId clip, domain::VisualTransform transform);
  void reset_clip_visual_transform(domain::ClipId clip);
  void begin_clip_visual_transform(domain::ClipId clip);
  void update_clip_visual_transform(domain::VisualTransform transform);
  void end_clip_visual_transform();
  void cancel_clip_visual_transform();
  // "Duplicar": a copy of the selected clip (or title) right after the original, on the same
  // track, with the same source range, gain, mute and fades. One undo step; the copy becomes the
  // selection. Throws DuplicateException{NoSpace} (nothing changes) when the room is taken.
  [[nodiscard]] bool can_duplicate_selected() const noexcept { return selected_clip_.has_value() || selected_title_.has_value(); }
  void duplicate_selected();
  [[nodiscard]] bool can_split() const;
  [[nodiscard]] domain::ClipId split_selected_at_playhead();
  void delete_selected();

  // ---- Titles (static text over the video; one title track, titles never overlap) -------
  [[nodiscard]] const std::vector<domain::Title>& titles() const noexcept { return document_.titles(); }
  [[nodiscard]] std::optional<domain::TitleId> selected_title() const noexcept { return selected_title_; }
  void select_title(std::optional<domain::TitleId> title);   // selecting a title deselects the clip
  // New title at the playhead: up to 3 s, shortened to the free room and to the end of the video.
  domain::TitleId add_title_at_playhead();
  void replace_title(domain::Title title);                    // text / time / style: ONE undo step
  void delete_selected_title();
  // "Eliminar y cerrar hueco": removes the selected clip (t0..t1) and moves EVERY clip of
  // any track that starts at or after t1 earlier by exactly t1 - t0. Clips that start
  // before t1 stay, nothing is trimmed, nothing may overlap afterwards (otherwise
  // RippleDeleteException{WouldOverlap}, nothing changes). One undo/redo step, however
  // many clips move. Everything goes through ordinary commands.
  [[nodiscard]] bool can_ripple_delete() const;
  void ripple_delete_selected();
  // Q / W: cut the selected clip's start / end back to the playhead. Returns false (and
  // changes nothing) unless the playhead is strictly inside the selected clip.
  [[nodiscard]] bool can_trim_to_playhead() const { return can_split(); }
  bool trim_selected_to_playhead(bool trim_start);
  // Alt+Left / Alt+Right: selection order is timeline start, then track order, then id.
  // With no selection "next" picks the first clip and "previous" the last. Returns
  // whether the selection changed; never moves the playhead.
  bool select_adjacent_clip(bool next);
  // Start of a clip on the timeline (for revealing it in a zoomed view).
  [[nodiscard]] std::optional<domain::MediaTime> clip_timeline_start(domain::ClipId clip) const;
  // "Separar audio": the embedded audio of a VIDEO 1 clip becomes an ordinary
  // audio clip with exactly the same timeline start and source range (same
  // media, so sample-for-sample the same sound), and the video clip's embedded
  // audio is muted. Gain and mute state move with the audio. One undo/redo step.
  [[nodiscard]] bool can_separate_audio(domain::ClipId video_clip) const;
  [[nodiscard]] bool clip_has_audio(domain::ClipId clip) const;
  [[nodiscard]] domain::ClipId separate_audio(domain::ClipId video_clip);
  // Ingest an already imported video asset into VIDEO 1 with its full playable
  // range. No ripple, overwrite or split: a request that would overlap an
  // existing clip throws VideoIngestException{NoSpace} and changes nothing.
  // One undoable command; selects the new clip.
  [[nodiscard]] domain::ClipId insert_video_asset(domain::MediaId media,
                                                   domain::MediaTime timeline_start,
                                                   std::optional<domain::TrackId> target = std::nullopt);
  [[nodiscard]] domain::ClipId insert_video_asset_at_playhead(domain::MediaId media);
  // Drag & drop of an audio-only library item onto an AUDIO track at `timeline_start`.
  // Same contract as the video ingest: overlap throws NoSpace and changes nothing.
  [[nodiscard]] domain::ClipId insert_audio_asset(domain::MediaId media, domain::TrackId audio_track,
                                                  domain::MediaTime timeline_start);
  // Library "Insertar" for audio: the first audio track with room at `timeline_start`, or a NEW audio
  // track when every existing one is busy there (so music and sounds can always be placed). One undo
  // step; same refusals as insert_audio_asset. `force_new_track` always makes a new track (drop below
  // the last lane).
  [[nodiscard]] domain::ClipId insert_audio_asset_auto(domain::MediaId media, domain::MediaTime timeline_start,
                                                       bool force_new_track = false);
  // Appends right after the last clip of VIDEO 1 (zero when it is empty).
  [[nodiscard]] domain::ClipId append_video_asset(domain::MediaId media);
  // End of the last clip on VIDEO 1: the authority for "append", independent of
  // longer audio tracks.
  [[nodiscard]] domain::MediaTime video_track_end() const;
  [[nodiscard]] bool undo();
  [[nodiscard]] bool redo();
  [[nodiscard]] bool can_undo() const noexcept { return processor_.can_undo(); }
  [[nodiscard]] bool can_redo() const noexcept { return processor_.can_redo(); }

  void synchronize_document_state();
  [[nodiscard]] EditorSyncDiagnostics synchronization_diagnostics() const;
  // Persists exactly the snapshot captured here; marks that captured history
  // state saved only after the repository reports success. Depends only on
  // the application repository port, never on a concrete adapter.
  [[nodiscard]] application::ProjectSaveResult save_project(
      const std::filesystem::path& destination,
      const application::ProjectRepository& repository);
  void rebuild_read_model();
  void select_clip(std::optional<domain::ClipId> clip);
  [[nodiscard]] std::optional<domain::MediaTime> time_at(double pixel, double width) const;
  [[nodiscard]] double pixel_at(domain::MediaTime time, double width) const;

private:
  application::ProjectDocument document_;
  application::CommandProcessor processor_;
  application::SteadyMonotonicClock clock_;
  std::unique_ptr<application::AudioOutput> audio_output_;
  application::TimelinePlaybackCoordinator coordinator_;
  std::optional<domain::ClipId> selected_clip_;
  std::optional<domain::TitleId> selected_title_;
  std::vector<TimelineClipView> views_;
  std::vector<TimelineTrackView> track_views_;
  std::uint64_t read_model_generation_{0};
  std::optional<std::uint64_t> synchronized_document_generation_;
  std::uint64_t sync_performed_count_{0};
  std::uint64_t sync_skipped_count_{0};
  DurationSampleRing sync_durations_;
};

} // namespace pulsar::desktop
