#pragma once

#include <pulsar/application/project/media_asset.hpp>
#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/title.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>

#include <variant>

namespace pulsar::application {

struct AddTrackCommand {
  domain::TrackId track_id;
  domain::TrackKind kind;
};

struct RemoveTrackCommand {
  domain::TrackId track_id;
};

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

struct SetClipAudioGainCommand {
  domain::ClipId clip_id;
  double gain_db;
};

struct SetClipMutedCommand {
  domain::ClipId clip_id;
  bool muted;
};

struct SetClipFadesCommand {
  domain::ClipId clip_id;
  domain::MediaTime fade_in;
  domain::MediaTime fade_out;
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

struct AddTitleCommand {
  domain::Title title;
};

struct RemoveTitleCommand {
  domain::TitleId title_id;
};

// Replaces the title with the same id (text, time range, style): one command for every edit.
struct ReplaceTitleCommand {
  domain::Title title;
};

using Command = std::variant<AddTrackCommand, RemoveTrackCommand, AddClipCommand, RemoveClipCommand, MoveClipCommand,
                             TrimClipStartCommand, TrimClipEndCommand, SplitClipCommand,
                             SetClipAudioGainCommand, SetClipMutedCommand, SetClipFadesCommand, RestoreClipCommand,
                             RestoreSplitCommand, ImportAssetCommand, RemoveAssetCommand,
                             AddTitleCommand, RemoveTitleCommand, ReplaceTitleCommand>;

} // namespace pulsar::application
