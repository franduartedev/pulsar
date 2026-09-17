#include <pulsar/domain/timeline/track.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

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

std::optional<Clip> Track::find_clip_at(MediaTime timeline_position) const {
  for (const auto& clip : clips_) {
    const TimeRange range = clip.timeline_range();
    if (range.contains(timeline_position)) {
      return clip;
    }
    if (range.start() > timeline_position) {
      break;
    }
  }

  return std::nullopt;
}

void Track::trim_clip_start(ClipId id, MediaTime timeline_start) {
  const auto iterator = std::find_if(
      clips_.begin(), clips_.end(), [id](const Clip& clip) { return clip.id() == id; });
  if (iterator == clips_.end()) {
    throw std::invalid_argument{"clip id does not exist in track"};
  }

  const Clip original = *iterator;
  const TimeRange timeline_range = original.timeline_range();
  if (timeline_start <= timeline_range.start() || timeline_start >= timeline_range.end()) {
    throw std::invalid_argument{"trim start must be strictly inside clip timeline range"};
  }

  const MediaTime offset = timeline_start - timeline_range.start();
  const TimeRange source_range{original.source_range().start() + offset,
                               original.source_range().end()};
  *iterator = Clip{original.id(), original.media_id(), source_range, timeline_start};
}

void Track::trim_clip_end(ClipId id, MediaTime timeline_end) {
  const auto iterator = std::find_if(
      clips_.begin(), clips_.end(), [id](const Clip& clip) { return clip.id() == id; });
  if (iterator == clips_.end()) {
    throw std::invalid_argument{"clip id does not exist in track"};
  }

  const Clip original = *iterator;
  const TimeRange timeline_range = original.timeline_range();
  if (timeline_end <= timeline_range.start() || timeline_end >= timeline_range.end()) {
    throw std::invalid_argument{"trim end must be strictly inside clip timeline range"};
  }

  const MediaTime duration = timeline_end - timeline_range.start();
  const TimeRange source_range{original.source_range().start(),
                               original.source_range().start() + duration};
  *iterator = Clip{original.id(), original.media_id(), source_range, original.timeline_start()};
}

void Track::split_clip(ClipId id, MediaTime split_timeline_position, ClipId right_clip_id) {
  const auto iterator = std::find_if(
      clips_.begin(), clips_.end(), [id](const Clip& clip) { return clip.id() == id; });
  if (iterator == clips_.end()) {
    throw std::invalid_argument{"clip id does not exist in track"};
  }
  if (find_clip(right_clip_id) != nullptr) {
    throw std::invalid_argument{"right split clip id already exists in track"};
  }

  const Clip original = *iterator;
  const TimeRange timeline_range = original.timeline_range();
  if (split_timeline_position <= timeline_range.start() ||
      split_timeline_position >= timeline_range.end()) {
    throw std::invalid_argument{"split position must be strictly inside clip timeline range"};
  }

  const MediaTime left_duration = split_timeline_position - timeline_range.start();
  const MediaTime source_split = original.source_range().start() + left_duration;
  const Clip left{original.id(), original.media_id(),
                  TimeRange{original.source_range().start(), source_split},
                  original.timeline_start()};
  const Clip right{right_clip_id, original.media_id(),
                   TimeRange{source_split, original.source_range().end()},
                   split_timeline_position};

  Track replacement{id_, kind_};
  for (const auto& existing : clips_) {
    if (existing.id() == id) {
      replacement.insert_clip(left);
      replacement.insert_clip(right);
    } else {
      replacement.insert_clip(existing);
    }
  }
  clips_ = std::move(replacement.clips_);
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
