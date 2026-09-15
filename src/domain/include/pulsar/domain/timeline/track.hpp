#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>
#include <pulsar/domain/time/media_time.hpp>

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

  [[nodiscard]] const Clip* find_clip(ClipId id) const noexcept;
  [[nodiscard]] MediaTime duration() const;

private:
  TrackId id_;
  TrackKind kind_;
  std::vector<Clip> clips_;
};

} // namespace pulsar::domain
