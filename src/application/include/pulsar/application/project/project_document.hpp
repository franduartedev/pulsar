#pragma once

#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_snapshot.hpp>
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

  [[nodiscard]] domain::TrackId video_track_id() const noexcept { return video_track_id_; }
  [[nodiscard]] std::vector<MediaAsset> assets() const { return assets_; }
  [[nodiscard]] std::optional<MediaAsset> find_asset(domain::MediaId id) const;
  [[nodiscard]] const domain::Timeline& timeline() const noexcept { return timeline_; }
  [[nodiscard]] domain::ClipId next_clip_id() const;

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

  // Transitional marker only; no filesystem save is performed.
  void mark_current_state_saved() noexcept { saved_history_state_id_ = current_history_state_id_; }

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

    void ensure_media_consumable() const;
    void ensure_clip_consumable() const;
    void consume_media();
    void consume_clip();
    void consume_timeline();
    void consume_track();
  };

  [[nodiscard]] MediaAsset build_asset(const std::filesystem::path& source_locator,
                                       const MediaProbe& probe, domain::MediaId id) const;
  void add_asset_value(const MediaAsset& asset);
  void remove_asset_value(domain::MediaId id);
  void validate_new_clip(const domain::Clip& clip) const;
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
  std::vector<MediaAsset> assets_;
  HistoryStateId current_history_state_id_{1};
  HistoryStateId saved_history_state_id_{1};
  HistoryStateId next_history_state_id_{2};
  std::uint64_t document_generation_{0};
};

} // namespace pulsar::application
