#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/track.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <vector>

namespace pulsar::domain {

class Timeline {
public:
  explicit Timeline(TimelineId id);

  [[nodiscard]] constexpr TimelineId id() const noexcept {
    return id_;
  }

  [[nodiscard]] const std::vector<Track>& tracks() const noexcept {
    return tracks_;
  }

  void add_track(Track track);
  void insert_clip(TrackId track_id, Clip clip);

  [[nodiscard]] const Track* find_track(TrackId id) const noexcept;
  [[nodiscard]] const Clip* find_clip(ClipId id) const noexcept;
  [[nodiscard]] MediaTime duration() const;

private:
  [[nodiscard]] Track* find_track_mutable(TrackId id) noexcept;

  TimelineId id_;
  std::vector<Track> tracks_;
};

} // namespace pulsar::domain
