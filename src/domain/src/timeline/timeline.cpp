#include <pulsar/domain/timeline/timeline.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

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

void Timeline::remove_track(TrackId track_id) {
  const auto iterator = std::find_if(
      tracks_.begin(), tracks_.end(), [track_id](const Track& track) { return track.id() == track_id; });
  if (iterator == tracks_.end()) {
    throw std::invalid_argument{"track id does not exist in timeline"};
  }
  if (!iterator->clips().empty()) {
    throw std::invalid_argument{"cannot remove a track that contains clips"};
  }
  tracks_.erase(iterator);
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

void Timeline::remove_clip(ClipId clip_id) {
  for (std::size_t track_index = 0; track_index < tracks_.size(); ++track_index) {
    const Track& track = tracks_[track_index];
    if (track.find_clip(clip_id) == nullptr) {
      continue;
    }

    Track replacement = track;
    replacement.remove_clip(clip_id);

    tracks_[track_index] = std::move(replacement);
    return;
  }

  throw std::invalid_argument{"clip id does not exist in timeline"};
}

void Timeline::move_clip(ClipId clip_id, TrackId destination_track_id, MediaTime timeline_start) {
  if (timeline_start < MediaTime::zero()) {
    throw std::invalid_argument{"clip timeline start cannot be negative"};
  }

  const std::size_t destination_index = find_track_index(destination_track_id);

  std::size_t source_index = tracks_.size();
  const Clip* original_clip = nullptr;
  for (std::size_t index = 0; index < tracks_.size(); ++index) {
    original_clip = tracks_[index].find_clip(clip_id);
    if (original_clip != nullptr) {
      source_index = index;
      break;
    }
  }

  if (original_clip == nullptr) {
    throw std::invalid_argument{"clip id does not exist in timeline"};
  }

  const Clip moved_clip{original_clip->id(), original_clip->media_id(),
                        original_clip->source_range(), timeline_start,
                        original_clip->audio_gain_db(), original_clip->muted(),
                        original_clip->fade_in(), original_clip->fade_out(),
                        original_clip->visual_transform(), original_clip->distant_music(), original_clip->telephone(), original_clip->underwater()};

  Track replacement_source = tracks_[source_index];
  replacement_source.remove_clip(clip_id);

  if (source_index == destination_index) {
    replacement_source.insert_clip(moved_clip);
    tracks_[source_index] = std::move(replacement_source);
    return;
  }

  Track replacement_destination = tracks_[destination_index];
  replacement_destination.insert_clip(moved_clip);

  tracks_[source_index] = std::move(replacement_source);
  tracks_[destination_index] = std::move(replacement_destination);
}

void Timeline::trim_clip_start(ClipId clip_id, MediaTime timeline_start) {
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    if (tracks_[i].find_clip(clip_id) == nullptr) continue;
    Track replacement = tracks_[i];
    replacement.trim_clip_start(clip_id, timeline_start);
    tracks_[i] = std::move(replacement);
    return;
  }
  throw std::invalid_argument{"clip id does not exist in timeline"};
}

void Timeline::trim_clip_end(ClipId clip_id, MediaTime timeline_end) {
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    if (tracks_[i].find_clip(clip_id) == nullptr) continue;
    Track replacement = tracks_[i];
    replacement.trim_clip_end(clip_id, timeline_end);
    tracks_[i] = std::move(replacement);
    return;
  }
  throw std::invalid_argument{"clip id does not exist in timeline"};
}

void Timeline::split_clip(ClipId clip_id, MediaTime split_timeline_position, ClipId right_clip_id) {
  if (find_clip(right_clip_id) != nullptr) {
    throw std::invalid_argument{"right split clip id already exists in timeline"};
  }
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    if (tracks_[i].find_clip(clip_id) == nullptr) continue;
    Track replacement = tracks_[i];
    replacement.split_clip(clip_id, split_timeline_position, right_clip_id);
    tracks_[i] = std::move(replacement);
    return;
  }
  throw std::invalid_argument{"clip id does not exist in timeline"};
}

void Timeline::set_clip_audio_gain(domain::ClipId clip_id, double gain_db) {
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    if (tracks_[i].find_clip(clip_id) == nullptr) continue;
    Track replacement = tracks_[i];
    replacement.set_clip_audio_gain(clip_id, gain_db);
    tracks_[i] = std::move(replacement);
    return;
  }
  throw std::invalid_argument{"clip id does not exist in timeline"};
}

void Timeline::set_clip_muted(domain::ClipId clip_id, bool muted) {
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    if (tracks_[i].find_clip(clip_id) == nullptr) continue;
    Track replacement = tracks_[i];
    replacement.set_clip_muted(clip_id, muted);
    tracks_[i] = std::move(replacement);
    return;
  }
  throw std::invalid_argument{"clip id does not exist in timeline"};
}

void Timeline::set_track_audio_state(TrackId track_id, AudioTrackState state) {
  auto* track = find_track_mutable(track_id);
  if (track == nullptr) throw std::invalid_argument{"track id does not exist in timeline"};
  track->set_audio_state(state);
}

void Timeline::set_clip_fades(domain::ClipId clip_id, MediaTime fade_in, MediaTime fade_out) {
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    if (tracks_[i].find_clip(clip_id) == nullptr) continue;
    Track replacement = tracks_[i];
    replacement.set_clip_fades(clip_id, fade_in, fade_out);
    tracks_[i] = std::move(replacement);
    return;
  }
  throw std::invalid_argument{"clip id does not exist in timeline"};
}

void Timeline::set_clip_distant_music(domain::ClipId clip_id, DistantMusicSettings settings) {
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    if (tracks_[i].find_clip(clip_id) == nullptr) continue;
    Track replacement = tracks_[i];
    replacement.set_clip_distant_music(clip_id, settings);
    tracks_[i] = std::move(replacement);
    return;
  }
  throw std::invalid_argument{"clip id does not exist in timeline"};
}

void Timeline::set_clip_telephone(domain::ClipId clip_id, TelephoneSettings settings) {
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    if (tracks_[i].find_clip(clip_id) == nullptr) continue;
    Track replacement = tracks_[i];
    replacement.set_clip_telephone(clip_id, std::move(settings));
    tracks_[i] = std::move(replacement);
    return;
  }
  throw std::invalid_argument{"clip id does not exist in timeline"};
}

void Timeline::set_clip_underwater(domain::ClipId clip_id, UnderwaterSettings settings) {
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    if (tracks_[i].find_clip(clip_id) == nullptr) continue;
    Track replacement = tracks_[i];
    replacement.set_clip_underwater(clip_id, std::move(settings));
    tracks_[i] = std::move(replacement);
    return;
  }
  throw std::invalid_argument{"clip id does not exist in timeline"};
}

void Timeline::set_clip_visual_transform(domain::ClipId clip_id, VisualTransform transform) {
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    if (tracks_[i].find_clip(clip_id) == nullptr) continue;
    Track replacement = tracks_[i];
    replacement.set_clip_visual_transform(clip_id, transform);
    tracks_[i] = std::move(replacement);
    return;
  }
  throw std::invalid_argument{"clip id does not exist in timeline"};
}

void Timeline::restore_clip(TrackId track_id, Clip clip) {
  const std::size_t track_index = find_track_index(track_id);
  for (std::size_t index = 0; index < tracks_.size(); ++index) {
    if (index != track_index && tracks_[index].find_clip(clip.id()) != nullptr) {
      throw std::invalid_argument{"clip id already exists on another track in timeline"};
    }
  }
  Track replacement = tracks_[track_index];
  if (replacement.find_clip(clip.id()) != nullptr) {
    replacement.remove_clip(clip.id());
  }
  replacement.insert_clip(clip);
  tracks_[track_index] = std::move(replacement);
}

void Timeline::restore_split(TrackId track_id, Clip original_clip, ClipId right_clip_id) {
  if (original_clip.id() == right_clip_id) {
    throw std::invalid_argument{"split restore ids must differ"};
  }
  const std::size_t track_index = find_track_index(track_id);
  Track replacement = tracks_[track_index];
  if (replacement.find_clip(right_clip_id) == nullptr) {
    throw std::invalid_argument{"right split clip does not exist in timeline"};
  }
  if (replacement.find_clip(original_clip.id()) != nullptr) {
    replacement.remove_clip(original_clip.id());
  }
  replacement.remove_clip(right_clip_id);
  replacement.insert_clip(original_clip);
  tracks_[track_index] = std::move(replacement);
}

std::optional<Clip> Timeline::find_clip_at(TrackId track_id, MediaTime timeline_position) const {
  const Track* track = find_track(track_id);
  if (track == nullptr) return std::nullopt;
  return track->find_clip_at(timeline_position);
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

std::size_t Timeline::find_track_index(TrackId id) const {
  for (std::size_t index = 0; index < tracks_.size(); ++index) {
    if (tracks_[index].id() == id) {
      return index;
    }
  }

  throw std::invalid_argument{"track id does not exist in timeline"};
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
