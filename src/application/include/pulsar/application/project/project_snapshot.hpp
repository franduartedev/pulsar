#pragma once

#include <pulsar/application/project/media_asset.hpp>
#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/timeline/timeline.hpp>
#include <pulsar/domain/timeline/title.hpp>

#include <cstdint>
#include <vector>

namespace pulsar::application {

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
};

// Throws on incomplete, duplicate, non-video, or missing-primary stack entries.
void validate_visual_track_ids(const domain::Timeline& timeline, domain::TrackId primary,
                              const std::vector<domain::TrackId>& visual_track_ids);

} // namespace pulsar::application
