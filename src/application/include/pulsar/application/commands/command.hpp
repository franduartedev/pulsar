#pragma once

#include <pulsar/application/project/media_asset.hpp>
#include <pulsar/application/project/project_snapshot.hpp>
#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/title.hpp>
#include <pulsar/domain/timeline/transition.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>
#include <pulsar/domain/timeline/track.hpp>

#include <variant>

namespace pulsar::application {

struct AddTrackCommand {
  domain::TrackId track_id;
  domain::TrackKind kind;
};

struct RemoveTrackCommand {
  domain::TrackId track_id;
};

struct RestoreEmptyVisualTrackCommand {
  domain::TrackId track_id;
  std::size_t visual_index;
};

struct ReorderVisualTrackCommand {
  domain::TrackId track_id;
  std::size_t visual_index; // bottom-to-top position
};

struct SetTrackAudioStateCommand {
  domain::TrackId track_id;
  domain::AudioTrackState state;
};

struct SetProjectCanvasCommand {
  std::optional<ProjectCanvas> canvas;
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

struct SetClipDistantMusicCommand {
  domain::ClipId clip_id;
  domain::DistantMusicSettings settings;
};

struct SetClipTelephoneCommand {
  domain::ClipId clip_id;
  domain::TelephoneSettings settings;
};

struct SetClipVisualTransformCommand {
  domain::ClipId clip_id;
  domain::VisualTransform transform;
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

struct ReplaceAssetCommand {
  MediaAsset asset;
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

struct ReplaceSubtitlesCommand {
  std::vector<SubtitleCue> subtitles;
};

struct AddTransitionCommand { domain::Transition transition; };
struct RemoveTransitionCommand { domain::Transition transition; };

using Command = std::variant<AddTrackCommand, RemoveTrackCommand, RestoreEmptyVisualTrackCommand,
                             ReorderVisualTrackCommand, SetTrackAudioStateCommand,
                             SetProjectCanvasCommand,
                             AddClipCommand, RemoveClipCommand, MoveClipCommand,
                             TrimClipStartCommand, TrimClipEndCommand, SplitClipCommand,
                             SetClipAudioGainCommand, SetClipMutedCommand, SetClipFadesCommand,
                             SetClipDistantMusicCommand,
                             SetClipTelephoneCommand,
                             SetClipVisualTransformCommand, RestoreClipCommand,
                              RestoreSplitCommand, ImportAssetCommand, RemoveAssetCommand,
                              ReplaceAssetCommand,
                              AddTitleCommand, RemoveTitleCommand, ReplaceTitleCommand,
                              ReplaceSubtitlesCommand,
                              AddTransitionCommand, RemoveTransitionCommand>;

} // namespace pulsar::application
