#pragma once

#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_snapshot.hpp>
#include <pulsar/application/project/project_restore_state.hpp>
#include <pulsar/domain/timeline/timeline.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace pulsar::application {

class CommandProcessor;

class ProjectDocument {
public:
  ProjectDocument();
  [[nodiscard]] static ProjectDocument restore(const ProjectRestoreState& state);

  [[nodiscard]] domain::TrackId video_track_id() const noexcept { return video_track_id_; }
  [[nodiscard]] const std::vector<domain::TrackId>& visual_track_ids() const noexcept {
    return visual_track_ids_;
  }
  [[nodiscard]] const std::optional<ProjectCanvas>& canvas() const noexcept { return canvas_; }
  [[nodiscard]] domain::TrackId next_track_id() const { return allocator_.next_track(); }
  [[nodiscard]] std::vector<MediaAsset> assets() const { return assets_; }
  // Allocation-free view for UI read models. Valid until the next asset edit.
  [[nodiscard]] const std::vector<MediaAsset>& asset_list() const noexcept { return assets_; }
  // Increments whenever the asset set changes (import, undo/redo of import).
  [[nodiscard]] std::uint64_t asset_revision() const noexcept { return asset_revision_; }
  [[nodiscard]] std::optional<MediaAsset> find_asset(domain::MediaId id) const;
  [[nodiscard]] const domain::Timeline& timeline() const noexcept { return timeline_; }
  [[nodiscard]] domain::ClipId next_clip_id() const;
  // Titles are kept sorted by start and never overlap each other (one title track).
  [[nodiscard]] const std::vector<domain::Title>& titles() const noexcept { return titles_; }
  [[nodiscard]] const std::vector<domain::Transition>& transitions() const noexcept { return transitions_; }
  [[nodiscard]] const domain::Title* find_title(domain::TitleId id) const noexcept;
  [[nodiscard]] domain::TitleId next_title_id() const { return allocator_.next_title(); }

  [[nodiscard]] ProjectSnapshot snapshot() const;
  [[nodiscard]] bool is_dirty() const noexcept {
    return current_history_state_id_ != saved_history_state_id_;
  }
  [[nodiscard]] HistoryStateId history_state_id() const noexcept { return current_history_state_id_; }
  [[nodiscard]] HistoryStateId saved_history_state_id() const noexcept {
    return saved_history_state_id_;
  }
  [[nodiscard]] std::uint64_t document_generation() const noexcept {
    return document_generation_;
  }

  // Marks exactly the history state represented by a persisted snapshot.
  void mark_history_state_saved(HistoryStateId state_id) noexcept {
    saved_history_state_id_ = state_id;
  }
  // Transitional marker retained for callers that save the current snapshot.
  void mark_current_state_saved() noexcept { mark_history_state_saved(current_history_state_id_); }

private:
  friend class CommandProcessor;

  struct IdAllocator {
    ProjectAllocatorState state{};

    [[nodiscard]] domain::MediaId next_media() const { return domain::MediaId{state.next_media_id}; }
    [[nodiscard]] domain::ClipId next_clip() const { return domain::ClipId{state.next_clip_id}; }
    [[nodiscard]] domain::TimelineId next_timeline() const {
      return domain::TimelineId{state.next_timeline_id};
    }
    [[nodiscard]] domain::TrackId next_track() const { return domain::TrackId{state.next_track_id}; }
    [[nodiscard]] domain::TitleId next_title() const { return domain::TitleId{state.next_title_id}; }
    void ensure_title_consumable() const;
    void consume_title();

    void ensure_media_consumable() const;
    void ensure_clip_consumable() const;
    void ensure_track_consumable() const;
    void consume_media();
    void consume_clip();
    void consume_timeline();
    void consume_track();
  };

  [[nodiscard]] MediaAsset build_asset(const std::filesystem::path& source_locator,
                                       const MediaProbe& probe, domain::MediaId id) const;
  void add_asset_value(const MediaAsset& asset);
  void remove_asset_value(domain::MediaId id);
  void add_title_value(const domain::Title& title);
  void remove_title_value(domain::TitleId id);
  void replace_title_value(const domain::Title& title);
  void add_transition_value(const domain::Transition& transition);
  void remove_transition_value(const domain::Transition& transition);
  void validate_transition(const domain::Transition& transition) const;
  void ensure_clip_has_no_transition(domain::ClipId clip_id) const;
  void validate_clip_for_track(domain::TrackId track_id, const domain::Clip& clip) const;
  [[nodiscard]] domain::Timeline& timeline_mutable() noexcept { return timeline_; }
  [[nodiscard]] HistoryStateId next_history_state_id() const noexcept {
    return next_history_state_id_;
  }
  void ensure_new_edit_possible() const;
  void commit_new_edit();
  void commit_history_restore(HistoryStateId state_id);
  void ensure_generation_advance_possible() const;

  IdAllocator allocator_;
  domain::Timeline timeline_;
  domain::TrackId video_track_id_;
  std::vector<domain::TrackId> visual_track_ids_;
  std::optional<ProjectCanvas> canvas_;
  std::vector<MediaAsset> assets_;
  std::vector<domain::Title> titles_;
  std::vector<domain::Transition> transitions_;
  std::uint64_t asset_revision_{0};
  HistoryStateId current_history_state_id_{1};
  HistoryStateId saved_history_state_id_{1};
  HistoryStateId next_history_state_id_{2};
  std::uint64_t document_generation_{0};
};

} // namespace pulsar::application
