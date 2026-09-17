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
  void remove_clip(ClipId clip_id);
  void move_clip(ClipId clip_id, TrackId destination_track_id, MediaTime timeline_start);

  // Returned pointers/references are observations only. Do not retain them across
  // Timeline or Track mutation; vector storage and copied replacement tracks may invalidate them.
  [[nodiscard]] const Track* find_track(TrackId id) const noexcept;
  [[nodiscard]] const Clip* find_clip(ClipId id) const noexcept;
  [[nodiscard]] MediaTime duration() const;

private:
  [[nodiscard]] Track* find_track_mutable(TrackId id) noexcept;
  [[nodiscard]] std::size_t find_track_index(TrackId id) const;

  TimelineId id_;
  std::vector<Track> tracks_;
};

} // namespace pulsar::domain
