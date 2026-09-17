#pragma once

#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/playback/timeline_playback_coordinator.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <cstdint>
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

class TimelineViewport {
public:
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
  void seek(domain::MediaTime position);
  [[nodiscard]] domain::MediaTime playback_position() const;

  void synchronize_document_state();
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
};

} // namespace pulsar::desktop
