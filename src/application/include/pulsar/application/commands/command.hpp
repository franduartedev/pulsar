#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/timeline/clip.hpp>

#include <variant>

namespace pulsar::application {

struct AddClipCommand {
  domain::TrackId track_id;
  domain::Clip clip;
};

struct RemoveClipCommand {
  domain::ClipId clip_id;
};

struct MoveClipCommand {
  domain::ClipId clip_id;
  domain::TrackId destination_track_id;
  domain::MediaTime timeline_start;
};

using Command = std::variant<AddClipCommand, RemoveClipCommand, MoveClipCommand>;

} // namespace pulsar::application
