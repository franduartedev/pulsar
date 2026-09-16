#include <pulsar/application/commands/command_processor.hpp>

#include <stdexcept>
#include <type_traits>

namespace pulsar::application {
namespace {

struct ClipLocation {
  domain::TrackId track_id;
  domain::Clip clip;
};

[[nodiscard]] ClipLocation find_clip_location(const domain::Timeline& timeline,
                                              domain::ClipId clip_id) {
  for (const auto& track : timeline.tracks()) {
    if (const domain::Clip* clip = track.find_clip(clip_id); clip != nullptr) {
      return ClipLocation{track.id(), *clip};
    }
  }

  throw std::invalid_argument{"clip id does not exist in timeline"};
}

} // namespace

CommandProcessor::CommandProcessor(domain::Timeline& timeline, std::size_t history_capacity)
    : timeline_{timeline}, history_capacity_{history_capacity} {}

void CommandProcessor::execute(Command command) {
  ExecutedCommand executed{command, build_inverse_for(timeline_, command)};

  if (history_capacity_ == 0) {
    execute_existing(timeline_, executed.forward);
    redo_stack_.clear();
    return;
  }

  undo_stack_.push_back(executed);
  try {
    execute_existing(timeline_, undo_stack_.back().forward);
  } catch (...) {
    undo_stack_.pop_back();
    throw;
  }

  trim_undo_to_capacity();
  redo_stack_.clear();
}

bool CommandProcessor::can_undo() const noexcept {
  return !undo_stack_.empty();
}

bool CommandProcessor::can_redo() const noexcept {
  return !redo_stack_.empty();
}

std::size_t CommandProcessor::undo_size() const noexcept {
  return undo_stack_.size();
}

std::size_t CommandProcessor::redo_size() const noexcept {
  return redo_stack_.size();
}

bool CommandProcessor::undo() {
  if (undo_stack_.empty()) {
    return false;
  }

  ExecutedCommand executed = undo_stack_.back();
  redo_stack_.push_back(executed);
  try {
    execute_existing(timeline_, redo_stack_.back().inverse);
  } catch (...) {
    redo_stack_.pop_back();
    throw;
  }

  undo_stack_.pop_back();
  return true;
}

bool CommandProcessor::redo() {
  if (redo_stack_.empty()) {
    return false;
  }

  ExecutedCommand executed = redo_stack_.back();
  undo_stack_.push_back(executed);
  try {
    execute_existing(timeline_, undo_stack_.back().forward);
  } catch (...) {
    undo_stack_.pop_back();
    throw;
  }

  redo_stack_.pop_back();
  trim_undo_to_capacity();
  return true;
}

void CommandProcessor::execute_existing(domain::Timeline& timeline, const Command& command) {
  std::visit(
      [&timeline](const auto& concrete_command) {
        using CommandType = std::decay_t<decltype(concrete_command)>;

        if constexpr (std::is_same_v<CommandType, AddClipCommand>) {
          timeline.insert_clip(concrete_command.track_id, concrete_command.clip);
        } else if constexpr (std::is_same_v<CommandType, RemoveClipCommand>) {
          timeline.remove_clip(concrete_command.clip_id);
        } else if constexpr (std::is_same_v<CommandType, MoveClipCommand>) {
          timeline.move_clip(concrete_command.clip_id, concrete_command.destination_track_id,
                             concrete_command.timeline_start);
        }
      },
      command);
}

Command CommandProcessor::build_inverse_for(domain::Timeline& timeline, const Command& command) {
  return std::visit(
      [&timeline](const auto& concrete_command) -> Command {
        using CommandType = std::decay_t<decltype(concrete_command)>;

        if constexpr (std::is_same_v<CommandType, AddClipCommand>) {
          return RemoveClipCommand{concrete_command.clip.id()};
        } else if constexpr (std::is_same_v<CommandType, RemoveClipCommand>) {
          const ClipLocation location = find_clip_location(timeline, concrete_command.clip_id);
          return AddClipCommand{location.track_id, location.clip};
        } else if constexpr (std::is_same_v<CommandType, MoveClipCommand>) {
          const ClipLocation location = find_clip_location(timeline, concrete_command.clip_id);
          return MoveClipCommand{concrete_command.clip_id, location.track_id,
                                 location.clip.timeline_start()};
        }
      },
      command);
}

void CommandProcessor::trim_undo_to_capacity() {
  while (undo_stack_.size() > history_capacity_) {
    undo_stack_.pop_front();
  }
}

} // namespace pulsar::application
