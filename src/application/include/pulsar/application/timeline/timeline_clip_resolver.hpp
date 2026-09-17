#pragma once

#include <pulsar/application/project/project_snapshot.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <variant>

namespace pulsar::application {

struct ResolvedClip {
  domain::ClipId clip_id;
  domain::MediaId media_id;
  domain::MediaTime timeline_position;
  domain::TimeRange timeline_range;
  domain::MediaTime clip_offset;
  domain::TimeRange source_range;
  domain::MediaTime source_time;
  int video_stream_index;
  std::filesystem::path source_locator;
};

struct TimelineGap {
  domain::MediaTime position;
};

struct MissingClipMedia {
  domain::ClipId clip_id;
  domain::MediaId media_id;
  domain::MediaTime timeline_position;
  domain::TimeRange timeline_range;
  std::optional<std::string> reason;
};

struct TimelineEnded {
  domain::MediaTime position;
  domain::MediaTime duration;
};

using TimelineResolveOutcome =
    std::variant<ResolvedClip, TimelineGap, MissingClipMedia, TimelineEnded>;

[[nodiscard]] TimelineResolveOutcome resolve_clip_at(const ProjectSnapshot& snapshot,
                                                     domain::TrackId track_id,
                                                     domain::MediaTime timeline_position);

} // namespace pulsar::application
