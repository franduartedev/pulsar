#include <pulsar/application/timeline/timeline_clip_resolver.hpp>

#include <stdexcept>

namespace pulsar::application {
namespace {

[[nodiscard]] const MediaAsset* find_asset(const ProjectSnapshot& snapshot,
                                           domain::MediaId media_id) {
  for (const auto& asset : snapshot.assets) {
    if (asset.id() == media_id) {
      return &asset;
    }
  }
  return nullptr;
}

} // namespace

domain::MediaTime clip_relative_position(const ResolvedClip& clip,
                                          domain::MediaTime timeline_position) {
  if (!clip.timeline_range.contains(timeline_position)) {
    throw std::invalid_argument{"timeline position is outside resolved clip range"};
  }
  return timeline_position - clip.timeline_range.start();
}

domain::MediaTime clip_source_position(const ResolvedClip& clip,
                                       domain::MediaTime timeline_position) {
  return clip.source_range.start() + clip_relative_position(clip, timeline_position);
}

domain::MediaTime audio_source_position(const ResolvedClip& clip,
                                        domain::MediaTime timeline_position,
                                        std::optional<domain::MediaTime> stream_origin) {
  return stream_origin.value_or(domain::MediaTime::zero()) +
         clip_source_position(clip, timeline_position);
}

TimelineResolveOutcome resolve_clip_at(const ProjectSnapshot& snapshot,
                                       domain::TrackId track_id,
                                       domain::MediaTime timeline_position) {
  if (timeline_position < domain::MediaTime::zero()) {
    throw std::invalid_argument{"timeline resolver position cannot be negative"};
  }
  if (snapshot.timeline.find_track(track_id) == nullptr) {
    throw std::invalid_argument{"timeline resolver track does not exist"};
  }

  const auto clip = snapshot.timeline.find_clip_at(track_id, timeline_position);
  if (!clip.has_value()) {
    const domain::MediaTime duration = snapshot.timeline.duration();
    if (timeline_position >= duration) {
      return TimelineEnded{timeline_position, duration};
    }
    return TimelineGap{timeline_position};
  }

  const domain::TimeRange timeline_range = clip->timeline_range();
  const domain::TimeRange source_range = clip->source_range();
  const MediaAsset* asset = find_asset(snapshot, clip->media_id());
  if (asset == nullptr) {
    return MissingClipMedia{clip->id(), clip->media_id(), timeline_position, timeline_range,
                            std::nullopt};
  }
  if (asset->availability() == MediaAvailability::Unavailable) {
    return MissingClipMedia{clip->id(), clip->media_id(), timeline_position, timeline_range,
                            asset->availability_reason()};
  }

  // A0: expose the asset's audio identity (if any) for later M10 consumers.
  // The resolver algorithm is unchanged; no timestamp mapping is computed.
  std::optional<AudioSourceBinding> audio;
  if (asset->audio().has_value()) {
    audio = AudioSourceBinding{asset->source_locator(), *asset->audio()};
  }
  ResolvedClip resolved{clip->id(), clip->media_id(), timeline_position, timeline_range,
                        timeline_position - timeline_range.start(), source_range,
                        domain::MediaTime::zero(), clip->audio_gain_db(), clip->muted(),
                        asset->video_stream_index(),
                        asset->source_locator(), std::move(audio)};
  resolved.source_time = clip_source_position(resolved, timeline_position);
  return resolved;
}

} // namespace pulsar::application
