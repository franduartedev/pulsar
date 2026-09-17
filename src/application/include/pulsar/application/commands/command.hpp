#pragma once

#include <pulsar/application/project/media_asset.hpp>
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

struct TrimClipStartCommand {
  domain::ClipId clip_id;
  domain::MediaTime timeline_start;
};

struct TrimClipEndCommand {
  domain::ClipId clip_id;
  domain::MediaTime timeline_end;
};

struct SplitClipCommand {
  domain::ClipId clip_id;
  domain::MediaTime split_timeline_position;
  domain::ClipId right_clip_id;
};

// Value-only inverse operations. They are not user-facing editing tools.
struct RestoreClipCommand {
  domain::TrackId track_id;
  domain::Clip clip;
};

struct RestoreSplitCommand {
  domain::TrackId track_id;
  domain::Clip original_clip;
  domain::ClipId right_clip_id;
};

struct ImportAssetCommand {
  MediaAsset asset;
};

struct RemoveAssetCommand {
  domain::MediaId media_id;
};

using Command = std::variant<AddClipCommand, RemoveClipCommand, MoveClipCommand,
                             TrimClipStartCommand, TrimClipEndCommand, SplitClipCommand,
                             RestoreClipCommand, RestoreSplitCommand, ImportAssetCommand,
                             RemoveAssetCommand>;

} // namespace pulsar::application
