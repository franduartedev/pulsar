#pragma once

#include <pulsar/application/commands/command.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <cstddef>
#include <cstdint>
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

  [[nodiscard]] domain::TrackId add_track(domain::TrackKind kind);
  void remove_empty_visual_track(domain::TrackId track_id);
  void reorder_visual_track(domain::TrackId track_id, std::size_t visual_index);
  void set_track_audio_state(domain::TrackId track_id, domain::AudioTrackState state);
  void set_project_canvas(ProjectCanvas canvas);

  [[nodiscard]] domain::ClipId add_clip(domain::MediaId media_id,
                                         domain::TimeRange source_range,
                                         domain::MediaTime timeline_start);
  [[nodiscard]] domain::ClipId add_clip(domain::TrackId track_id, domain::MediaId media_id,
                                         domain::TimeRange source_range,
                                         domain::MediaTime timeline_start);
  void remove_clip(domain::ClipId clip_id);
  // Titles: the new title must carry document.next_title_id(). Overlap with another title throws.
  domain::TitleId add_title(domain::Title title);
  void replace_title(domain::Title title);
  void remove_title(domain::TitleId id);
  void replace_subtitles(std::vector<SubtitleCue> subtitles);
  void add_transition(domain::Transition transition);
  void remove_transition(domain::Transition transition);
  void move_clip(domain::ClipId clip_id, domain::TrackId destination_track_id,
                 domain::MediaTime timeline_start);
  void trim_clip_start(domain::ClipId clip_id, domain::MediaTime timeline_start);
  void trim_clip_end(domain::ClipId clip_id, domain::MediaTime timeline_end);
  void set_clip_audio_gain(domain::ClipId clip_id, double gain_db);
  void set_clip_muted(domain::ClipId clip_id, bool muted);
  // Linear audio fades at the clip start / end (zero = none); one undo step. No-op when unchanged.
  void set_clip_fades(domain::ClipId clip_id, domain::MediaTime fade_in, domain::MediaTime fade_out);
  void set_clip_distant_music(domain::ClipId clip_id, domain::DistantMusicSettings settings);
  void set_clip_telephone(domain::ClipId clip_id, domain::TelephoneSettings settings);
  void set_clip_underwater(domain::ClipId clip_id, domain::UnderwaterSettings settings);
  void set_clip_audio_effect_order(domain::ClipId clip_id,
                                   std::vector<domain::AudioEffectType> order);
  // Visual transform (position/scale/rotation/opacity); one undo step. No-op when unchanged.
  void set_clip_visual_transform(domain::ClipId clip_id, domain::VisualTransform transform);
  // Direct-manipulation transaction. Preview updates mutate through the command
  // processor but replace one history entry; end commits one logical undo step.
  void begin_clip_visual_transform(domain::ClipId clip_id);
  void update_clip_visual_transform(domain::VisualTransform transform);
  void end_clip_visual_transform() noexcept;
  void cancel_clip_visual_transform();
  [[nodiscard]] bool clip_visual_transform_active() const noexcept {
    return interactive_transform_.has_value();
  }
  [[nodiscard]] domain::ClipId split_clip(domain::ClipId clip_id,
                                           domain::MediaTime split_timeline_position);
  [[nodiscard]] domain::MediaId import_asset(const std::filesystem::path& source_locator,
                                              const MediaProbe& probe);
  void relink_asset(domain::MediaId media_id, const std::filesystem::path& source_locator,
                    const MediaProbe& probe);

  [[nodiscard]] bool can_undo() const noexcept;
  [[nodiscard]] bool can_redo() const noexcept;
  [[nodiscard]] std::size_t undo_size() const noexcept;
  [[nodiscard]] std::size_t redo_size() const noexcept;

  // Several commands that undo/redo as ONE history step. Edits made between
  // begin_group() and end_group() share a group id; undo()/redo() walk the whole
  // group. Prefer run_as_one_edit(): it rolls the group back if a command throws.
  void begin_group() noexcept;
  void end_group() noexcept;
  template <typename Fn>
  void run_as_one_edit(Fn&& edit) {
    begin_group();
    const auto group = open_group_;
    try {
      edit();
    } catch (...) {
      // Roll back while the group is still open so older history is not trimmed away
      // to make room for the entries that are about to be discarded anyway.
      try { rollback_group(group); } catch (...) {}
      end_group();
      throw;
    }
    end_group();
  }

  bool undo();
  bool redo();

private:
  struct ExecutedCommand {
    Command forward;
    Command inverse;
    HistoryStateId before_state_id;
    HistoryStateId after_state_id;
    std::uint64_t group{0};  // 0 = standalone; otherwise shared by one grouped edit
    std::optional<ProjectCanvas> canvas_before;
    std::optional<ProjectCanvas> canvas_after;
  };

  enum class ApplyMode { NewEdit, HistoryRestore };

  static void apply(ProjectDocument& document, const Command& command, ApplyMode mode);
  [[nodiscard]] static Command build_inverse(const ProjectDocument& document,
                                              const Command& command);
  void trim_undo_to_capacity();
  bool undo_one();
  bool redo_one();
  void rollback_group(std::uint64_t group);

  struct InteractiveTransform {
    domain::ClipId clip_id;
    bool has_history_entry{false};
  };

  ProjectDocument& document_;
  std::size_t history_capacity_;
  std::deque<ExecutedCommand> undo_stack_;
  std::deque<ExecutedCommand> redo_stack_;
  std::uint64_t next_group_{1};
  std::uint64_t open_group_{0};
  std::optional<InteractiveTransform> interactive_transform_;
};

} // namespace pulsar::application
