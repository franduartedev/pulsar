#pragma once

#include <pulsar/application/project/media_asset.hpp>
#include <pulsar/application/project/project_snapshot.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>

#include <vector>

namespace pulsar::application {

struct ProjectRestoreTrack {
  domain::TrackId id;
  domain::TrackKind kind;
  std::vector<domain::Clip> clips;
  domain::AudioTrackState audio_state{};
};

// Neutral, fully decoded project data. It contains no codec, filesystem, or
// decoder state; the outer file adapter refreshes MediaAsset availability
// before handing it to ProjectDocument::restore.
struct ProjectRestoreState {
  domain::TimelineId timeline_id;
  std::vector<ProjectRestoreTrack> tracks;
  domain::TrackId video_track_id;
  std::vector<MediaAsset> assets;
  ProjectAllocatorState allocator_state;
  std::vector<domain::Title> titles{};
  std::vector<domain::TrackId> visual_track_ids{video_track_id};
  std::optional<ProjectCanvas> canvas{};
  std::vector<domain::Transition> transitions{};
};

} // namespace pulsar::application
