#include <pulsar/domain/timeline/timeline.hpp>

#include <stdexcept>

namespace pulsar::domain {

Timeline::Timeline(TimelineId id) : id_{id} {}

void Timeline::add_track(Track track) {
  if (find_track(track.id()) != nullptr) {
    throw std::invalid_argument{"track id already exists in timeline"};
  }

  for (const auto& clip : track.clips()) {
    if (find_clip(clip.id()) != nullptr) {
      throw std::invalid_argument{"clip id already exists in timeline"};
    }
  }

  tracks_.push_back(track);
}

void Timeline::insert_clip(TrackId track_id, Clip clip) {
  if (find_clip(clip.id()) != nullptr) {
    throw std::invalid_argument{"clip id already exists in timeline"};
  }

  Track* track = find_track_mutable(track_id);
  if (track == nullptr) {
    throw std::invalid_argument{"track id does not exist in timeline"};
  }

  track->insert_clip(clip);
}

const Track* Timeline::find_track(TrackId id) const noexcept {
  for (const auto& track : tracks_) {
    if (track.id() == id) {
      return &track;
    }
  }

  return nullptr;
}

Track* Timeline::find_track_mutable(TrackId id) noexcept {
  for (auto& track : tracks_) {
    if (track.id() == id) {
      return &track;
    }
  }

  return nullptr;
}

const Clip* Timeline::find_clip(ClipId id) const noexcept {
  for (const auto& track : tracks_) {
    if (const Clip* clip = track.find_clip(id); clip != nullptr) {
      return clip;
    }
  }

  return nullptr;
}

MediaTime Timeline::duration() const {
  MediaTime result = MediaTime::zero();

  for (const auto& track : tracks_) {
    const MediaTime track_duration = track.duration();
    if (track_duration > result) {
      result = track_duration;
    }
  }

  return result;
}

} // namespace pulsar::domain
