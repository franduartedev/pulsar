#include <pulsar/domain/timeline/track.hpp>

#include <algorithm>
#include <stdexcept>

namespace pulsar::domain {

Track::Track(TrackId id, TrackKind kind) : id_{id}, kind_{kind} {}

void Track::insert_clip(Clip clip) {
  if (find_clip(clip.id()) != nullptr) {
    throw std::invalid_argument{"clip id already exists in track"};
  }

  const TimeRange range = clip.timeline_range();
  for (const auto& existing : clips_) {
    if (range.overlaps(existing.timeline_range())) {
      throw std::invalid_argument{"clip overlaps an existing clip in track"};
    }
  }

  const auto insertion = std::lower_bound(
      clips_.begin(), clips_.end(), range.start(), [](const Clip& existing, MediaTime start) {
        return existing.timeline_range().start() < start;
      });
  clips_.insert(insertion, clip);
}

const Clip* Track::find_clip(ClipId id) const noexcept {
  for (const auto& clip : clips_) {
    if (clip.id() == id) {
      return &clip;
    }
  }

  return nullptr;
}

void Track::remove_clip(ClipId id) {
  const auto clip =
      std::find_if(clips_.begin(), clips_.end(), [id](const Clip& item) { return item.id() == id; });

  if (clip == clips_.end()) {
    throw std::invalid_argument{"clip id does not exist in track"};
  }

  clips_.erase(clip);
}

MediaTime Track::duration() const {
  MediaTime result = MediaTime::zero();

  for (const auto& clip : clips_) {
    const MediaTime end = clip.timeline_range().end();
    if (end > result) {
      result = end;
    }
  }

  return result;
}

} // namespace pulsar::domain
