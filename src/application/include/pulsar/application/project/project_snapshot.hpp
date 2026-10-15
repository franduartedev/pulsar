#pragma once

#include <pulsar/application/project/media_asset.hpp>
#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/timeline/timeline.hpp>
#include <pulsar/domain/timeline/title.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace pulsar::application {

struct ProjectCanvas {
  int width;
  int height;
  domain::FrameRate frame_rate;
  // A still-only canvas follows the legacy real-video-first rule until the
  // first real VIDEO 1 clip is added. Afterwards the canvas is permanent.
  bool provisional_still{false};
  bool operator==(const ProjectCanvas& other) const noexcept {
    return width == other.width && height == other.height &&
           frame_rate.rational() == other.frame_rate.rational() &&
           provisional_still == other.provisional_still;
  }
};

using HistoryStateId = std::uint64_t;

struct ProjectAllocatorState {
  std::uint64_t next_media_id{1};
  std::uint64_t next_clip_id{1};
  std::uint64_t next_timeline_id{1};
  std::uint64_t next_track_id{1};
  std::uint64_t next_title_id{1};
};

struct ProjectSnapshot {
  domain::Timeline timeline;
  std::vector<MediaAsset> assets;
  domain::TrackId video_track_id;
  ProjectAllocatorState allocator_state;
  HistoryStateId history_state_id;
  HistoryStateId saved_history_state_id;
  HistoryStateId next_history_state_id;
  std::uint64_t document_generation;
  std::vector<domain::Title> titles{};
  // Explicit bottom-to-top order; storage order and IDs have no visual meaning.
  std::vector<domain::TrackId> visual_track_ids{video_track_id};
  std::optional<ProjectCanvas> canvas{};
};

// Throws on incomplete, duplicate, non-video, or missing-primary stack entries.
void validate_visual_track_ids(const domain::Timeline& timeline, domain::TrackId primary,
                              const std::vector<domain::TrackId>& visual_track_ids);

} // namespace pulsar::application
