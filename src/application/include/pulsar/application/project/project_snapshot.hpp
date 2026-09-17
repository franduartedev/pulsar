#pragma once

#include <pulsar/application/project/media_asset.hpp>
#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/timeline/timeline.hpp>

#include <cstdint>
#include <vector>

namespace pulsar::application {

using HistoryStateId = std::uint64_t;

struct ProjectAllocatorState {
  std::uint64_t next_media_id{1};
  std::uint64_t next_clip_id{1};
  std::uint64_t next_timeline_id{1};
  std::uint64_t next_track_id{1};
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
};

} // namespace pulsar::application
