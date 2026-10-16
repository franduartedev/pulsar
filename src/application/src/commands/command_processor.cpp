#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/composition/project_canvas.hpp>

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

void CommandProcessor::remove_empty_visual_track(domain::TrackId track_id) {
  execute(RemoveTrackCommand{track_id});
}

void CommandProcessor::reorder_visual_track(domain::TrackId track_id, std::size_t visual_index) {
  const auto& ids = document_.visual_track_ids();
  const auto found = std::find(ids.begin(), ids.end(), track_id);
  if (found != ids.end() && static_cast<std::size_t>(found - ids.begin()) == visual_index) return;
  execute(ReorderVisualTrackCommand{track_id, visual_index});
}

void CommandProcessor::set_track_audio_state(domain::TrackId track_id,
                                              domain::AudioTrackState state) {
  domain::validate_audio_track_state(state);
  const auto* track = document_.timeline().find_track(track_id);
  if (track != nullptr && track->audio_state() == state) return;
  execute(SetTrackAudioStateCommand{track_id, state});
}

void CommandProcessor::execute(Command command) {
  document_.ensure_new_edit_possible();
  const HistoryStateId before = document_.history_state_id();
  const HistoryStateId after = document_.next_history_state_id();
  ExecutedCommand executed{command, build_inverse(document_, command), before, after, open_group_, document_.canvas_, {}};

  if (history_capacity_ != 0) {
    undo_stack_.push_back(executed);
  }
  try {
    apply(document_, command, ApplyMode::NewEdit);
    document_.commit_new_edit();
    if (history_capacity_ != 0) undo_stack_.back().canvas_after = document_.canvas_;
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

void CommandProcessor::add_transition(domain::Transition transition) {
  execute(AddTransitionCommand{std::move(transition)});
}

void CommandProcessor::remove_transition(domain::Transition transition) {
  execute(RemoveTransitionCommand{std::move(transition)});
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

void CommandProcessor::set_clip_fades(domain::ClipId clip_id, domain::MediaTime fade_in,
                                      domain::MediaTime fade_out) {
  if (const auto* clip = document_.timeline().find_clip(clip_id);
      clip != nullptr && clip->fade_in() == fade_in && clip->fade_out() == fade_out) {
    return;
  }
  execute(SetClipFadesCommand{clip_id, fade_in, fade_out});
}

void CommandProcessor::set_clip_visual_transform(domain::ClipId clip_id,
                                                  domain::VisualTransform transform) {
  validate_visual_transform(transform);
  if (const auto* clip = document_.timeline().find_clip(clip_id);
      clip != nullptr && clip->visual_transform() == transform) {
    return;
  }
  execute(SetClipVisualTransformCommand{clip_id, transform});
}

void CommandProcessor::begin_clip_visual_transform(domain::ClipId clip_id) {
  if (interactive_transform_) throw std::logic_error{"visual transform edit already active"};
  if (open_group_ != 0) throw std::logic_error{"cannot begin visual transform inside command group"};
  if (document_.timeline().find_clip(clip_id) == nullptr) {
    throw std::invalid_argument{"clip id does not exist in timeline"};
  }
  interactive_transform_ = InteractiveTransform{clip_id, false};
}

void CommandProcessor::update_clip_visual_transform(domain::VisualTransform transform) {
  if (!interactive_transform_) throw std::logic_error{"no visual transform edit active"};
  validate_visual_transform(transform);
  const auto clip_id = interactive_transform_->clip_id;
  const auto* clip = document_.timeline().find_clip(clip_id);
  if (clip == nullptr) throw std::logic_error{"interactive transform clip disappeared"};
  if (clip->visual_transform() == transform) return;

  if (!interactive_transform_->has_history_entry) {
    set_clip_visual_transform(clip_id, transform);
    interactive_transform_->has_history_entry = history_capacity_ != 0;
    return;
  }

  if (undo_stack_.empty()) throw std::logic_error{"interactive transform history disappeared"};
  auto* prior = std::get_if<SetClipVisualTransformCommand>(&undo_stack_.back().forward);
  if (prior == nullptr || prior->clip_id != clip_id || undo_stack_.back().group != 0) {
    throw std::logic_error{"interactive transform is not the newest command"};
  }
  document_.ensure_new_edit_possible();
  const SetClipVisualTransformCommand update{clip_id, transform};
  apply(document_, update, ApplyMode::NewEdit);
  document_.commit_new_edit();
  undo_stack_.back().forward = update;
  undo_stack_.back().after_state_id = document_.history_state_id();
  undo_stack_.back().canvas_after = document_.canvas_;
  redo_stack_.clear();
}

void CommandProcessor::end_clip_visual_transform() noexcept { interactive_transform_.reset(); }

void CommandProcessor::cancel_clip_visual_transform() {
  if (!interactive_transform_) return;
  const bool has_entry = interactive_transform_->has_history_entry;
  interactive_transform_.reset();
  if (!has_entry) return;
  if (!undo_one()) throw std::logic_error{"interactive transform history disappeared"};
  if (redo_stack_.empty()) throw std::logic_error{"interactive transform rollback failed"};
  redo_stack_.pop_back();
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
  if (interactive_transform_) throw std::logic_error{"cannot undo during visual transform edit"};
  if (undo_stack_.empty()) return false;
  const auto group = undo_stack_.back().group;
  if (!undo_one()) return false;
  while (group != 0 && !undo_stack_.empty() && undo_stack_.back().group == group) {
    if (!undo_one()) break;
  }
  return true;
}

bool CommandProcessor::redo() {
  if (interactive_transform_) throw std::logic_error{"cannot redo during visual transform edit"};
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
    document_.canvas_ = executed.canvas_before;
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
    document_.canvas_ = executed.canvas_after;
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
          if (mode == ApplyMode::NewEdit) {
            if (concrete_command.track_id != document.allocator_.next_track()) {
              throw std::invalid_argument{"new track id is not the next document id"};
            }
            document.allocator_.ensure_track_consumable();
          }
          if (concrete_command.kind == domain::TrackKind::Video) {
            document.visual_track_ids_.reserve(document.visual_track_ids_.size() + 1);
          }
          timeline.add_track(domain::Track{concrete_command.track_id, concrete_command.kind});
          if (concrete_command.kind == domain::TrackKind::Video) {
            document.visual_track_ids_.push_back(concrete_command.track_id);
          }
          if (mode == ApplyMode::NewEdit) document.allocator_.consume_track();
        } else if constexpr (std::is_same_v<CommandType, RemoveTrackCommand>) {
          if (concrete_command.track_id == document.video_track_id()) {
            throw std::invalid_argument{"cannot remove the primary video track"};
          }
          const auto* track = timeline.find_track(concrete_command.track_id);
          if (!track || !track->clips().empty() ||
              (track->kind() != domain::TrackKind::Video && mode == ApplyMode::NewEdit)) {
            throw std::invalid_argument{"only an empty auxiliary visual track can be removed"};
          }
          const bool visual = track->kind() == domain::TrackKind::Video;
          if (visual && std::find(document.visual_track_ids_.begin(), document.visual_track_ids_.end(),
                                  concrete_command.track_id) == document.visual_track_ids_.end()) {
            throw std::invalid_argument{"visual track is not in the stack"};
          }
          timeline.remove_track(concrete_command.track_id);
          if (visual) {
            auto& ids = document.visual_track_ids_;
            ids.erase(std::find(ids.begin(), ids.end(), concrete_command.track_id));
          }
        } else if constexpr (std::is_same_v<CommandType, RestoreEmptyVisualTrackCommand>) {
          auto& ids = document.visual_track_ids_;
          if (concrete_command.visual_index > ids.size() || timeline.find_track(concrete_command.track_id)) {
            throw std::invalid_argument{"invalid visual track restoration"};
          }
          ids.reserve(ids.size() + 1);
          timeline.add_track(domain::Track{concrete_command.track_id, domain::TrackKind::Video});
          ids.insert(ids.begin() + static_cast<std::ptrdiff_t>(concrete_command.visual_index),
                     concrete_command.track_id);
        } else if constexpr (std::is_same_v<CommandType, ReorderVisualTrackCommand>) {
          auto& ids = document.visual_track_ids_;
          const auto found = std::find(ids.begin(), ids.end(), concrete_command.track_id);
          const auto primary = std::find(ids.begin(), ids.end(), document.video_track_id());
          if (found == ids.end() || found == primary || concrete_command.visual_index >= ids.size() ||
              concrete_command.visual_index <= static_cast<std::size_t>(primary - ids.begin())) {
            throw std::invalid_argument{"invalid auxiliary visual track order"};
          }
          ids.erase(found);
          ids.insert(ids.begin() + static_cast<std::ptrdiff_t>(concrete_command.visual_index),
                      concrete_command.track_id);
        } else if constexpr (std::is_same_v<CommandType, SetTrackAudioStateCommand>) {
          timeline.set_track_audio_state(concrete_command.track_id, concrete_command.state);
        } else if constexpr (std::is_same_v<CommandType, AddClipCommand>) {
          bool promote_still_canvas = false;
          if (document.canvas_ && document.canvas_->provisional_still &&
              concrete_command.track_id == document.video_track_id_) {
            const auto* primary = timeline.find_track(document.video_track_id_);
            const bool has_still = primary && !primary->clips().empty();
            const auto asset = document.find_asset(concrete_command.clip.media_id());
            promote_still_canvas = has_still && asset && !asset->is_still();
          }
          if (mode == ApplyMode::NewEdit) {
            if (concrete_command.clip.id() != document.allocator_.next_clip()) {
              throw std::invalid_argument{"new clip id is not the next document id"};
            }
            document.validate_clip_for_track(concrete_command.track_id, concrete_command.clip);
            document.allocator_.ensure_clip_consumable();
          }
          timeline.insert_clip(concrete_command.track_id, concrete_command.clip);
          if (!document.canvas_ && timeline.find_track(concrete_command.track_id)->kind() == domain::TrackKind::Video) {
            // Legacy projects prefer VIDEO 1. A new project can establish its
            // canvas from the first auxiliary visual clip when VIDEO 1 is empty.
            document.canvas_ = legacy_project_canvas(document.snapshot());
            if (!document.canvas_) {
              const auto asset = document.find_asset(concrete_command.clip.media_id());
              if (asset && asset->metadata().nominal_frame_rate) {
                const auto [width, height] = master_canvas_size(*asset);
                document.canvas_ = ProjectCanvas{width, height, *asset->metadata().nominal_frame_rate,
                                                 asset->is_still()};
              }
            }
          } else if (promote_still_canvas) {
            document.canvas_ = legacy_project_canvas(document.snapshot());
          }
          if (mode == ApplyMode::NewEdit) {
            document.allocator_.consume_clip();
          }
        } else if constexpr (std::is_same_v<CommandType, RemoveClipCommand>) {
          document.ensure_clip_has_no_transition(concrete_command.clip_id);
          timeline.remove_clip(concrete_command.clip_id);
        } else if constexpr (std::is_same_v<CommandType, MoveClipCommand>) {
          document.ensure_clip_has_no_transition(concrete_command.clip_id);
          const auto* clip = timeline.find_clip(concrete_command.clip_id);
          if (clip == nullptr) throw std::invalid_argument{"clip id does not exist in timeline"};
          document.validate_clip_for_track(concrete_command.destination_track_id, *clip);
          timeline.move_clip(concrete_command.clip_id, concrete_command.destination_track_id,
                             concrete_command.timeline_start);
        } else if constexpr (std::is_same_v<CommandType, TrimClipStartCommand>) {
          document.ensure_clip_has_no_transition(concrete_command.clip_id);
          timeline.trim_clip_start(concrete_command.clip_id, concrete_command.timeline_start);
        } else if constexpr (std::is_same_v<CommandType, TrimClipEndCommand>) {
          document.ensure_clip_has_no_transition(concrete_command.clip_id);
          timeline.trim_clip_end(concrete_command.clip_id, concrete_command.timeline_end);
        } else if constexpr (std::is_same_v<CommandType, SplitClipCommand>) {
          document.ensure_clip_has_no_transition(concrete_command.clip_id);
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
        } else if constexpr (std::is_same_v<CommandType, SetClipFadesCommand>) {
          timeline.set_clip_fades(concrete_command.clip_id, concrete_command.fade_in, concrete_command.fade_out);
        } else if constexpr (std::is_same_v<CommandType, SetClipVisualTransformCommand>) {
          timeline.set_clip_visual_transform(concrete_command.clip_id, concrete_command.transform);
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
        } else if constexpr (std::is_same_v<CommandType, AddTransitionCommand>) {
          document.add_transition_value(concrete_command.transition);
        } else if constexpr (std::is_same_v<CommandType, RemoveTransitionCommand>) {
          document.remove_transition_value(concrete_command.transition);
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
          const auto* track = document.timeline().find_track(concrete_command.track_id);
          if (!track || track->kind() != domain::TrackKind::Video ||
              track->id() == document.video_track_id() || !track->clips().empty()) {
            throw std::invalid_argument{"only an empty auxiliary visual track can be removed"};
          }
          const auto& ids = document.visual_track_ids();
          const auto found = std::find(ids.begin(), ids.end(), concrete_command.track_id);
          if (found == ids.end()) throw std::invalid_argument{"visual track is not in the stack"};
          return RestoreEmptyVisualTrackCommand{concrete_command.track_id,
              static_cast<std::size_t>(found - ids.begin())};
        } else if constexpr (std::is_same_v<CommandType, ReorderVisualTrackCommand>) {
          const auto& ids = document.visual_track_ids();
          const auto found = std::find(ids.begin(), ids.end(), concrete_command.track_id);
          if (found == ids.end()) throw std::invalid_argument{"visual track is not in the stack"};
          return ReorderVisualTrackCommand{concrete_command.track_id,
                                           static_cast<std::size_t>(found - ids.begin())};
        } else if constexpr (std::is_same_v<CommandType, SetTrackAudioStateCommand>) {
          const auto* track = document.timeline().find_track(concrete_command.track_id);
          if (track == nullptr) throw std::invalid_argument{"track id does not exist in timeline"};
          return SetTrackAudioStateCommand{concrete_command.track_id, track->audio_state()};
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
                             std::is_same_v<CommandType, SetClipMutedCommand> ||
                             std::is_same_v<CommandType, SetClipFadesCommand> ||
                             std::is_same_v<CommandType, SetClipVisualTransformCommand>) {
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
        } else if constexpr (std::is_same_v<CommandType, AddTransitionCommand>) {
          return RemoveTransitionCommand{concrete_command.transition};
        } else if constexpr (std::is_same_v<CommandType, RemoveTransitionCommand>) {
          const auto found = std::find(document.transitions().begin(), document.transitions().end(),
                                       concrete_command.transition);
          if (found == document.transitions().end()) {
            throw std::invalid_argument{"transition does not exist"};
          }
          return AddTransitionCommand{concrete_command.transition};
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
