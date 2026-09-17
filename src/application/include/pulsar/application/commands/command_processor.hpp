#pragma once

#include <pulsar/application/commands/command.hpp>
#include <pulsar/domain/timeline/timeline.hpp>

#include <cstddef>
#include <deque>

namespace pulsar::application {

class CommandProcessor {
public:
  static constexpr std::size_t default_history_capacity = 256;

  // A processor is the single active editorial writer/history for its Timeline.
  // The Timeline/document must outlive the processor; replacing it creates a new history.
  explicit CommandProcessor(domain::Timeline& timeline,
                            std::size_t history_capacity = default_history_capacity);

  CommandProcessor(const CommandProcessor&) = delete;
  CommandProcessor& operator=(const CommandProcessor&) = delete;
  CommandProcessor(CommandProcessor&&) = delete;
  CommandProcessor& operator=(CommandProcessor&&) = delete;
  ~CommandProcessor() = default;

  void execute(Command command);

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
  };

  static void execute_existing(domain::Timeline& timeline, const Command& command);
  [[nodiscard]] static Command build_inverse_for(domain::Timeline& timeline,
                                                 const Command& command);
  void trim_undo_to_capacity();

  domain::Timeline& timeline_;
  std::size_t history_capacity_;
  std::deque<ExecutedCommand> undo_stack_;
  std::deque<ExecutedCommand> redo_stack_;
};

} // namespace pulsar::application
