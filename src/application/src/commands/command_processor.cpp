#include <pulsar/application/commands/command_processor.hpp>

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace pulsar::application {
namespace {

struct ClipLocation {
  domain::TrackId track_id;
  domain::Clip clip;
};

[[nodiscard]] ClipLocation find_clip_location(const ProjectDocument& document,
                                              domain::ClipId clip_id) {
  for (const auto& track : document.timeline().tracks()) {
    if (const domain::Clip* clip = track.find_clip(clip_id); clip != nullptr) {
      return ClipLocation{track.id(), *clip};
    }
  }
  throw std::invalid_argument{"clip id does not exist in timeline"};
}

} // namespace

CommandProcessor::CommandProcessor(ProjectDocument& document, std::size_t history_capacity)
    : document_{document}, history_capacity_{history_capacity} {}

void CommandProcessor::execute(Command command) {
  document_.ensure_new_edit_possible();
  const HistoryStateId before = document_.history_state_id();
  const HistoryStateId after = document_.next_history_state_id();
  ExecutedCommand executed{command, build_inverse(document_, command), before, after};

  if (history_capacity_ != 0) {
    undo_stack_.push_back(executed);
  }
  try {
    apply(document_, command, ApplyMode::NewEdit);
    document_.commit_new_edit();
  } catch (...) {
    if (history_capacity_ != 0) {
      undo_stack_.pop_back();
    }
    throw;
  }

  if (history_capacity_ == 0) {
    redo_stack_.clear();
  } else {
    trim_undo_to_capacity();
    redo_stack_.clear();
  }
}

domain::ClipId CommandProcessor::add_clip(domain::MediaId media_id,
                                           domain::TimeRange source_range,
                                           domain::MediaTime timeline_start) {
  const domain::ClipId id = document_.next_clip_id();
  execute(AddClipCommand{document_.video_track_id(),
                         domain::Clip{id, media_id, source_range, timeline_start}});
  return id;
}

void CommandProcessor::remove_clip(domain::ClipId clip_id) {
  execute(RemoveClipCommand{clip_id});
}

void CommandProcessor::move_clip(domain::ClipId clip_id, domain::TrackId destination_track_id,
                                 domain::MediaTime timeline_start) {
  execute(MoveClipCommand{clip_id, destination_track_id, timeline_start});
}

void CommandProcessor::trim_clip_start(domain::ClipId clip_id, domain::MediaTime timeline_start) {
  execute(TrimClipStartCommand{clip_id, timeline_start});
}

void CommandProcessor::trim_clip_end(domain::ClipId clip_id, domain::MediaTime timeline_end) {
  execute(TrimClipEndCommand{clip_id, timeline_end});
}

domain::ClipId CommandProcessor::split_clip(domain::ClipId clip_id,
                                             domain::MediaTime split_timeline_position) {
  const domain::ClipId right_id = document_.next_clip_id();
  execute(SplitClipCommand{clip_id, split_timeline_position, right_id});
  return right_id;
}

domain::MediaId CommandProcessor::import_asset(const std::filesystem::path& source_locator,
                                               const MediaProbe& probe) {
  document_.ensure_new_edit_possible();
  document_.allocator_.ensure_media_consumable();
  const domain::MediaId id = document_.allocator_.next_media();
  MediaAsset asset = document_.build_asset(source_locator, probe, id);
  execute(ImportAssetCommand{std::move(asset)});
  return id;
}

bool CommandProcessor::can_undo() const noexcept { return !undo_stack_.empty(); }

bool CommandProcessor::can_redo() const noexcept { return !redo_stack_.empty(); }

std::size_t CommandProcessor::undo_size() const noexcept { return undo_stack_.size(); }

std::size_t CommandProcessor::redo_size() const noexcept { return redo_stack_.size(); }

bool CommandProcessor::undo() {
  if (undo_stack_.empty()) {
    return false;
  }
  document_.ensure_generation_advance_possible();
  const ExecutedCommand executed = undo_stack_.back();
  redo_stack_.push_back(executed);
  try {
    apply(document_, redo_stack_.back().inverse, ApplyMode::HistoryRestore);
    document_.commit_history_restore(executed.before_state_id);
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
  document_.ensure_generation_advance_possible();
  const ExecutedCommand executed = redo_stack_.back();
  undo_stack_.push_back(executed);
  try {
    apply(document_, undo_stack_.back().forward, ApplyMode::HistoryRestore);
    document_.commit_history_restore(executed.after_state_id);
  } catch (...) {
    undo_stack_.pop_back();
    throw;
  }
  redo_stack_.pop_back();
  trim_undo_to_capacity();
  return true;
}

void CommandProcessor::apply(ProjectDocument& document, const Command& command, ApplyMode mode) {
  std::visit(
      [&document, mode](const auto& concrete_command) {
        using CommandType = std::decay_t<decltype(concrete_command)>;
        auto& timeline = document.timeline_mutable();

        if constexpr (std::is_same_v<CommandType, AddClipCommand>) {
          if (mode == ApplyMode::NewEdit) {
            if (concrete_command.clip.id() != document.allocator_.next_clip()) {
              throw std::invalid_argument{"new clip id is not the next document id"};
            }
            document.validate_new_clip(concrete_command.clip);
            document.allocator_.ensure_clip_consumable();
          }
          timeline.insert_clip(concrete_command.track_id, concrete_command.clip);
          if (mode == ApplyMode::NewEdit) {
            document.allocator_.consume_clip();
          }
        } else if constexpr (std::is_same_v<CommandType, RemoveClipCommand>) {
          timeline.remove_clip(concrete_command.clip_id);
        } else if constexpr (std::is_same_v<CommandType, MoveClipCommand>) {
          timeline.move_clip(concrete_command.clip_id, concrete_command.destination_track_id,
                             concrete_command.timeline_start);
        } else if constexpr (std::is_same_v<CommandType, TrimClipStartCommand>) {
          timeline.trim_clip_start(concrete_command.clip_id, concrete_command.timeline_start);
        } else if constexpr (std::is_same_v<CommandType, TrimClipEndCommand>) {
          timeline.trim_clip_end(concrete_command.clip_id, concrete_command.timeline_end);
        } else if constexpr (std::is_same_v<CommandType, SplitClipCommand>) {
          if (mode == ApplyMode::NewEdit) {
            if (concrete_command.right_clip_id != document.allocator_.next_clip()) {
              throw std::invalid_argument{"new split id is not the next document id"};
            }
            document.allocator_.ensure_clip_consumable();
          }
          timeline.split_clip(concrete_command.clip_id, concrete_command.split_timeline_position,
                              concrete_command.right_clip_id);
          if (mode == ApplyMode::NewEdit) {
            document.allocator_.consume_clip();
          }
        } else if constexpr (std::is_same_v<CommandType, RestoreClipCommand>) {
          timeline.restore_clip(concrete_command.track_id, concrete_command.clip);
        } else if constexpr (std::is_same_v<CommandType, RestoreSplitCommand>) {
          timeline.restore_split(concrete_command.track_id, concrete_command.original_clip,
                                 concrete_command.right_clip_id);
        } else if constexpr (std::is_same_v<CommandType, ImportAssetCommand>) {
          if (mode == ApplyMode::NewEdit) {
            if (concrete_command.asset.id() != document.allocator_.next_media()) {
              throw std::invalid_argument{"new media id is not the next document id"};
            }
            document.allocator_.ensure_media_consumable();
          }
          document.add_asset_value(concrete_command.asset);
          if (mode == ApplyMode::NewEdit) {
            document.allocator_.consume_media();
          }
        } else if constexpr (std::is_same_v<CommandType, RemoveAssetCommand>) {
          document.remove_asset_value(concrete_command.media_id);
        }
      },
      command);
}

Command CommandProcessor::build_inverse(const ProjectDocument& document, const Command& command) {
  return std::visit(
      [&document](const auto& concrete_command) -> Command {
        using CommandType = std::decay_t<decltype(concrete_command)>;
        if constexpr (std::is_same_v<CommandType, AddClipCommand>) {
          return RemoveClipCommand{concrete_command.clip.id()};
        } else if constexpr (std::is_same_v<CommandType, RemoveClipCommand>) {
          const ClipLocation location = find_clip_location(document, concrete_command.clip_id);
          return AddClipCommand{location.track_id, location.clip};
        } else if constexpr (std::is_same_v<CommandType, MoveClipCommand>) {
          const ClipLocation location = find_clip_location(document, concrete_command.clip_id);
          return MoveClipCommand{concrete_command.clip_id, location.track_id,
                                 location.clip.timeline_start()};
        } else if constexpr (std::is_same_v<CommandType, TrimClipStartCommand> ||
                             std::is_same_v<CommandType, TrimClipEndCommand>) {
          const ClipLocation location = find_clip_location(document, concrete_command.clip_id);
          return RestoreClipCommand{location.track_id, location.clip};
        } else if constexpr (std::is_same_v<CommandType, SplitClipCommand>) {
          const ClipLocation location = find_clip_location(document, concrete_command.clip_id);
          return RestoreSplitCommand{location.track_id, location.clip,
                                     concrete_command.right_clip_id};
        } else if constexpr (std::is_same_v<CommandType, ImportAssetCommand>) {
          return RemoveAssetCommand{concrete_command.asset.id()};
        } else {
          throw std::invalid_argument{"inverse-only command cannot start a new history edit"};
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
