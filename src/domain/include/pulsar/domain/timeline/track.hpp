#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <optional>
#include <vector>

namespace pulsar::domain {

class Track {
public:
  Track(TrackId id, TrackKind kind);

  [[nodiscard]] constexpr TrackId id() const noexcept {
    return id_;
  }

  [[nodiscard]] constexpr TrackKind kind() const noexcept {
    return kind_;
  }

  [[nodiscard]] const std::vector<Clip>& clips() const noexcept {
    return clips_;
  }

  void insert_clip(Clip clip);

  // Editorial policy for A9.0: clips do not overlap within one track. Multiple
  // audio tracks are allowed, so future mixing can combine independent tracks
  // without changing this track-local invariant.

  // Returned pointers/references are observations only. Do not retain them across mutation.
  [[nodiscard]] const Clip* find_clip(ClipId id) const noexcept;
  // Returns a value copy so callers cannot retain a domain reference across mutation.
  [[nodiscard]] std::optional<Clip> find_clip_at(MediaTime timeline_position) const;
  [[nodiscard]] MediaTime duration() const;

private:
  friend class Timeline;

  void remove_clip(ClipId id);
  void trim_clip_start(ClipId id, MediaTime timeline_start);
  void trim_clip_end(ClipId id, MediaTime timeline_end);
  void split_clip(ClipId id, MediaTime split_timeline_position, ClipId right_clip_id);
  void set_clip_audio_gain(ClipId id, double gain_db);
  void set_clip_muted(ClipId id, bool muted);
  void set_clip_fades(ClipId id, MediaTime fade_in, MediaTime fade_out);

  TrackId id_;
  TrackKind kind_;
  std::vector<Clip> clips_;
};

} // namespace pulsar::domain
