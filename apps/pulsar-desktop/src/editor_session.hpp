#pragma once

#include "duration_sample_ring.hpp"

#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/playback/timeline_playback_coordinator.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/application/project/project_repository.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pulsar::desktop {

struct TimelineClipView {
  domain::ClipId id;
  std::string key;
  std::string name;
  std::string start;
  std::string end;
  std::string duration;
  bool missing{false};
  bool selected{false};
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
  [[nodiscard]] std::optional<domain::MediaTime> time_at(double pixel, double width) const;
  [[nodiscard]] double pixel_at(domain::MediaTime time, double width) const;

private:
  domain::MediaTime duration_;
};

class EditorSession final {
public:
  EditorSession();
  explicit EditorSession(application::PlaybackWorkerFactory worker_factory);
  ~EditorSession() = default;
  EditorSession(const EditorSession&) = delete;
  EditorSession& operator=(const EditorSession&) = delete;
  EditorSession(EditorSession&&) = delete;
  EditorSession& operator=(EditorSession&&) = delete;

  [[nodiscard]] application::ProjectDocument& document() noexcept { return document_; }
  [[nodiscard]] const application::ProjectDocument& document() const noexcept { return document_; }
  [[nodiscard]] application::CommandProcessor& processor() noexcept { return processor_; }
  [[nodiscard]] const std::vector<TimelineClipView>& timeline_views() const noexcept { return views_; }
  [[nodiscard]] std::optional<domain::ClipId> selected_clip() const noexcept { return selected_clip_; }
  [[nodiscard]] domain::MediaTime timeline_duration() const noexcept;
  [[nodiscard]] std::uint64_t read_model_generation() const noexcept { return read_model_generation_; }
  void play();
  void pause();
  [[nodiscard]] application::TimelinePresentation update_playback();
  [[nodiscard]] application::PlaybackIntent playback_intent() const noexcept;
  void seek(domain::MediaTime position);
  [[nodiscard]] domain::MediaTime playback_position() const;
  void move_clip(domain::ClipId clip, double press_pixel, double current_pixel, double width);
  void trim_clip_start(domain::ClipId clip, double pixel, double width);
  void trim_clip_end(domain::ClipId clip, double pixel, double width);
  [[nodiscard]] bool can_split() const;
  [[nodiscard]] domain::ClipId split_selected_at_playhead();
  void delete_selected();
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
  application::TimelinePlaybackCoordinator coordinator_;
  std::optional<domain::ClipId> selected_clip_;
  std::vector<TimelineClipView> views_;
  std::uint64_t read_model_generation_{0};
  std::optional<std::uint64_t> synchronized_document_generation_;
  std::uint64_t sync_performed_count_{0};
  std::uint64_t sync_skipped_count_{0};
  DurationSampleRing sync_durations_;
};

} // namespace pulsar::desktop
