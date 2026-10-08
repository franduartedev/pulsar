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

domain::TrackId CommandProcessor::add_track(domain::TrackKind kind) {
  const domain::TrackId id = document_.next_track_id();
  execute(AddTrackCommand{id, kind});
  return id;
}

void CommandProcessor::execute(Command command) {
  document_.ensure_new_edit_possible();
  const HistoryStateId before = document_.history_state_id();
  const HistoryStateId after = document_.next_history_state_id();
  ExecutedCommand executed{command, build_inverse(document_, command), before, after, open_group_};

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
    // Inside a grouped edit nothing is trimmed: the group would otherwise evict its own
    // earlier members (or older history needed by a rollback). end_group() trims.
    if (open_group_ == 0) trim_undo_to_capacity();
    redo_stack_.clear();
  }
}

domain::ClipId CommandProcessor::add_clip(domain::MediaId media_id,
                                           domain::TimeRange source_range,
                                           domain::MediaTime timeline_start) {
  return add_clip(document_.video_track_id(), media_id, source_range, timeline_start);
}

domain::ClipId CommandProcessor::add_clip(domain::TrackId track_id, domain::MediaId media_id,
                                           domain::TimeRange source_range,
                                           domain::MediaTime timeline_start) {
  const domain::ClipId id = document_.next_clip_id();
  execute(AddClipCommand{track_id,
                         domain::Clip{id, media_id, source_range, timeline_start}});
  return id;
}

domain::TitleId CommandProcessor::add_title(domain::Title title) {
  const auto id = title.id();
  execute(AddTitleCommand{std::move(title)});
  return id;
}

void CommandProcessor::replace_title(domain::Title title) { execute(ReplaceTitleCommand{std::move(title)}); }

void CommandProcessor::remove_title(domain::TitleId id) { execute(RemoveTitleCommand{id}); }

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

void CommandProcessor::set_clip_audio_gain(domain::ClipId clip_id, double gain_db) {
  if (const auto* clip = document_.timeline().find_clip(clip_id);
      clip != nullptr && clip->audio_gain_db() == gain_db) {
    return;
  }
  execute(SetClipAudioGainCommand{clip_id, gain_db});
}

void CommandProcessor::set_clip_muted(domain::ClipId clip_id, bool muted) {
  if (const auto* clip = document_.timeline().find_clip(clip_id);
      clip != nullptr && clip->muted() == muted) {
    return;
  }
  execute(SetClipMutedCommand{clip_id, muted});
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

void CommandProcessor::begin_group() noexcept {
  if (open_group_ == 0) open_group_ = next_group_++;
}

void CommandProcessor::end_group() noexcept {
  open_group_ = 0;
  if (history_capacity_ != 0) trim_undo_to_capacity();
}

bool CommandProcessor::undo() {
  if (undo_stack_.empty()) return false;
  const auto group = undo_stack_.back().group;
  if (!undo_one()) return false;
  while (group != 0 && !undo_stack_.empty() && undo_stack_.back().group == group) {
    if (!undo_one()) break;
  }
  return true;
}

bool CommandProcessor::redo() {
  if (redo_stack_.empty()) return false;
  const auto group = redo_stack_.back().group;
  if (!redo_one()) return false;
  while (group != 0 && !redo_stack_.empty() && redo_stack_.back().group == group) {
    if (!redo_one()) break;
  }
  return true;
}

bool CommandProcessor::undo_one() {
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

bool CommandProcessor::redo_one() {
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

// A grouped edit that failed half way leaves nothing behind: its commands are
// undone and dropped from both stacks (the group was the newest history).
void CommandProcessor::rollback_group(std::uint64_t group) {
  while (!undo_stack_.empty() && undo_stack_.back().group == group) {
    if (!undo_one()) break;
  }
  while (!redo_stack_.empty() && redo_stack_.back().group == group) redo_stack_.pop_back();
}

void CommandProcessor::apply(ProjectDocument& document, const Command& command, ApplyMode mode) {
  std::visit(
      [&document, mode](const auto& concrete_command) {
        using CommandType = std::decay_t<decltype(concrete_command)>;
        auto& timeline = document.timeline_mutable();

        if constexpr (std::is_same_v<CommandType, AddTrackCommand>) {
          if (concrete_command.kind == domain::TrackKind::Video &&
              document.timeline().find_track(document.video_track_id()) != nullptr) {
            throw std::invalid_argument{"project already has a video track"};
          }
          if (mode == ApplyMode::NewEdit) {
            if (concrete_command.track_id != document.allocator_.next_track()) {
              throw std::invalid_argument{"new track id is not the next document id"};
            }
            document.allocator_.ensure_track_consumable();
          }
          timeline.add_track(domain::Track{concrete_command.track_id, concrete_command.kind});
          if (mode == ApplyMode::NewEdit) document.allocator_.consume_track();
        } else if constexpr (std::is_same_v<CommandType, RemoveTrackCommand>) {
          if (concrete_command.track_id == document.video_track_id()) {
            throw std::invalid_argument{"cannot remove the video track"};
          }
          timeline.remove_track(concrete_command.track_id);
        } else if constexpr (std::is_same_v<CommandType, AddClipCommand>) {
          if (mode == ApplyMode::NewEdit) {
            if (concrete_command.clip.id() != document.allocator_.next_clip()) {
              throw std::invalid_argument{"new clip id is not the next document id"};
            }
            document.validate_clip_for_track(concrete_command.track_id, concrete_command.clip);
            document.allocator_.ensure_clip_consumable();
          }
          timeline.insert_clip(concrete_command.track_id, concrete_command.clip);
          if (mode == ApplyMode::NewEdit) {
            document.allocator_.consume_clip();
          }
        } else if constexpr (std::is_same_v<CommandType, RemoveClipCommand>) {
          timeline.remove_clip(concrete_command.clip_id);
        } else if constexpr (std::is_same_v<CommandType, MoveClipCommand>) {
          const auto* clip = timeline.find_clip(concrete_command.clip_id);
          if (clip == nullptr) throw std::invalid_argument{"clip id does not exist in timeline"};
          document.validate_clip_for_track(concrete_command.destination_track_id, *clip);
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
        } else if constexpr (std::is_same_v<CommandType, SetClipAudioGainCommand>) {
          timeline.set_clip_audio_gain(concrete_command.clip_id, concrete_command.gain_db);
        } else if constexpr (std::is_same_v<CommandType, SetClipMutedCommand>) {
          timeline.set_clip_muted(concrete_command.clip_id, concrete_command.muted);
        } else if constexpr (std::is_same_v<CommandType, RestoreClipCommand>) {
          document.validate_clip_for_track(concrete_command.track_id, concrete_command.clip);
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
        } else if constexpr (std::is_same_v<CommandType, AddTitleCommand>) {
          if (mode == ApplyMode::NewEdit) {
            if (concrete_command.title.id() != document.allocator_.next_title()) {
              throw std::invalid_argument{"new title id is not the next document id"};
            }
            document.allocator_.ensure_title_consumable();
          }
          document.add_title_value(concrete_command.title);
          if (mode == ApplyMode::NewEdit) document.allocator_.consume_title();
        } else if constexpr (std::is_same_v<CommandType, RemoveTitleCommand>) {
          document.remove_title_value(concrete_command.title_id);
        } else if constexpr (std::is_same_v<CommandType, ReplaceTitleCommand>) {
          document.replace_title_value(concrete_command.title);
        }
      },
      command);
}

Command CommandProcessor::build_inverse(const ProjectDocument& document, const Command& command) {
  return std::visit(
      [&document](const auto& concrete_command) -> Command {
        using CommandType = std::decay_t<decltype(concrete_command)>;
        if constexpr (std::is_same_v<CommandType, AddTrackCommand>) {
          return RemoveTrackCommand{concrete_command.track_id};
        } else if constexpr (std::is_same_v<CommandType, RemoveTrackCommand>) {
          throw std::invalid_argument{"removing a track is not a supported new edit"};
        } else if constexpr (std::is_same_v<CommandType, AddClipCommand>) {
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
        } else if constexpr (std::is_same_v<CommandType, SetClipAudioGainCommand> ||
                             std::is_same_v<CommandType, SetClipMutedCommand>) {
          const ClipLocation location = find_clip_location(document, concrete_command.clip_id);
          return RestoreClipCommand{location.track_id, location.clip};
        } else if constexpr (std::is_same_v<CommandType, AddTitleCommand>) {
          return RemoveTitleCommand{concrete_command.title.id()};
        } else if constexpr (std::is_same_v<CommandType, RemoveTitleCommand>) {
          const auto* existing = document.find_title(concrete_command.title_id);
          if (existing == nullptr) throw std::invalid_argument{"title does not exist in document"};
          return AddTitleCommand{*existing};
        } else if constexpr (std::is_same_v<CommandType, ReplaceTitleCommand>) {
          const auto* existing = document.find_title(concrete_command.title.id());
          if (existing == nullptr) throw std::invalid_argument{"title does not exist in document"};
          return ReplaceTitleCommand{*existing};
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
    // Never keep half a grouped edit: drop the whole oldest group. The newest group is
    // never dropped, even when it alone exceeds the capacity (a big ripple edit): the
    // capacity bounds history *between* edits, and one edit is always undoable whole.
    const auto group = undo_stack_.front().group;
    if (group != 0 && group == undo_stack_.back().group) break;
    undo_stack_.pop_front();
    while (group != 0 && !undo_stack_.empty() && undo_stack_.front().group == group) {
      undo_stack_.pop_front();
    }
  }
}

} // namespace pulsar::application
