#include <pulsar/application/audio/audio_contributor_resolver.hpp>

#include <algorithm>
#include <stdexcept>

namespace pulsar::application {
namespace {

[[nodiscard]] const MediaAsset* find_asset(const ProjectSnapshot& snapshot,
                                           domain::MediaId media_id) {
  const auto found = std::find_if(snapshot.assets.begin(), snapshot.assets.end(),
                                  [media_id](const MediaAsset& asset) {
                                    return asset.id() == media_id;
                                  });
  return found == snapshot.assets.end() ? nullptr : &*found;
}

void resolve_track(const ProjectSnapshot& snapshot, const domain::Track& track,
                   std::size_t track_order, domain::TimeRange requested,
                   bool any_solo, AudioContributorResolution& result) {
  for (const auto& clip : track.clips()) {
    if (!clip.timeline_range().overlaps(requested)) {
      continue;
    }
    const auto* asset = find_asset(snapshot, clip.media_id());
    if (asset == nullptr || asset->availability() == MediaAvailability::Unavailable) {
      result.unavailable.push_back(
          {track.id(), clip.id(), clip.media_id(), clip.timeline_range(),
           asset == nullptr ? std::nullopt : asset->availability_reason()});
      continue;
    }
    if (!asset->audio().has_value()) {
      if (track.kind() == domain::TrackKind::Audio) {
        result.unavailable.push_back(
            {track.id(), clip.id(), clip.media_id(), clip.timeline_range(),
             "asset has no usable audio stream"});
      }
      continue;
    }
    const auto& audio = *asset->audio();
    result.contributors.push_back(
        {track.id(), track.kind(), track_order, clip.id(), clip.media_id(),
         asset->source_locator(), audio.stream_index, clip.timeline_range(), clip.source_range(),
          std::nullopt, clip.audio_gain_db(), clip.muted(), audio.sample_rate,
          audio.channel_count, audio.time_base, clip.fade_in(), clip.fade_out(),
          track.audio_state().gain_db,
          track.audio_state().muted || (any_solo && !track.audio_state().solo),
          clip.distant_music(), clip.telephone(), clip.underwater()});
  }
}

} // namespace

AudioContributorResolution resolve_audio_contributors(const ProjectSnapshot& snapshot,
                                                       domain::TimeRange timeline_range) {
  if (timeline_range.start() < domain::MediaTime::zero()) {
    throw std::invalid_argument{"audio contributor range cannot start before timeline zero"};
  }
  AudioContributorResolution result;
  const auto& tracks = snapshot.timeline.tracks();
  const bool any_solo = std::any_of(tracks.begin(), tracks.end(), [&](const auto& track) {
    return (track.kind() == domain::TrackKind::Audio || track.id() == snapshot.video_track_id) &&
           track.audio_state().solo;
  });

  // Embedded program audio always precedes independent tracks in mix order.
  for (std::size_t index = 0; index < tracks.size(); ++index) {
    if (tracks[index].id() == snapshot.video_track_id &&
        tracks[index].kind() == domain::TrackKind::Video) {
      resolve_track(snapshot, tracks[index], index, timeline_range, any_solo, result);
    }
  }
  for (std::size_t index = 0; index < tracks.size(); ++index) {
    if (tracks[index].kind() == domain::TrackKind::Audio) {
      resolve_track(snapshot, tracks[index], index, timeline_range, any_solo, result);
    }
  }
  return result;
}

domain::MediaTime contributor_source_position(
    const AudioContributor& contributor, domain::MediaTime timeline_position,
    std::optional<domain::MediaTime> runtime_stream_origin) {
  if (!contributor.timeline_range.contains(timeline_position)) {
    throw std::invalid_argument{"timeline position is outside audio contributor"};
  }
  const auto origin = runtime_stream_origin.value_or(
      contributor.stream_origin.value_or(domain::MediaTime::zero()));
  return origin + contributor.source_range.start() +
         (timeline_position - contributor.timeline_range.start());
}

} // namespace pulsar::application
