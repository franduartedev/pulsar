#pragma once

#include <pulsar/application/commands/command.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <cstddef>
#include <deque>
#include <filesystem>

namespace pulsar::application {

class CommandProcessor {
public:
  static constexpr std::size_t default_history_capacity = 256;

  explicit CommandProcessor(ProjectDocument& document,
                            std::size_t history_capacity = default_history_capacity);

  CommandProcessor(const CommandProcessor&) = delete;
  CommandProcessor& operator=(const CommandProcessor&) = delete;
  CommandProcessor(CommandProcessor&&) = delete;
  CommandProcessor& operator=(CommandProcessor&&) = delete;
  ~CommandProcessor() = default;

  void execute(Command command);

  [[nodiscard]] domain::ClipId add_clip(domain::MediaId media_id,
                                         domain::TimeRange source_range,
                                         domain::MediaTime timeline_start);
  void remove_clip(domain::ClipId clip_id);
  void move_clip(domain::ClipId clip_id, domain::TrackId destination_track_id,
                 domain::MediaTime timeline_start);
  void trim_clip_start(domain::ClipId clip_id, domain::MediaTime timeline_start);
  void trim_clip_end(domain::ClipId clip_id, domain::MediaTime timeline_end);
  [[nodiscard]] domain::ClipId split_clip(domain::ClipId clip_id,
                                           domain::MediaTime split_timeline_position);
  [[nodiscard]] domain::MediaId import_asset(const std::filesystem::path& source_locator,
                                              const MediaProbe& probe);

  [[nodiscard]] bool can_undo() const noexcept;
  [[nodiscard]] bool can_redo() const noexcept;
  [[nodiscard]] std::size_t undo_size() const noexcept;
  [[nodiscard]] std::size_t redo_size() const noexcept;

  bool undo();
  bool redo();

private:
  struct ExecutedCommand {
    Command forward;
    Command inverse;
    HistoryStateId before_state_id;
    HistoryStateId after_state_id;
  };

  enum class ApplyMode { NewEdit, HistoryRestore };

  static void apply(ProjectDocument& document, const Command& command, ApplyMode mode);
  [[nodiscard]] static Command build_inverse(const ProjectDocument& document,
                                              const Command& command);
  void trim_undo_to_capacity();

  ProjectDocument& document_;
  std::size_t history_capacity_;
  std::deque<ExecutedCommand> undo_stack_;
  std::deque<ExecutedCommand> redo_stack_;
};

} // namespace pulsar::application
