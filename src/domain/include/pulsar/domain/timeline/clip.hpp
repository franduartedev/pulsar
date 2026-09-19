#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/time_range.hpp>

namespace pulsar::domain {

class Clip {
public:
  static constexpr double minimum_audio_gain_db = -60.0;
  static constexpr double maximum_audio_gain_db = 12.0;

  Clip(ClipId id, MediaId media_id, TimeRange source_range, MediaTime timeline_start,
       double audio_gain_db = 0.0, bool muted = false);

  [[nodiscard]] constexpr ClipId id() const noexcept {
    return id_;
  }

  [[nodiscard]] constexpr MediaId media_id() const noexcept {
    return media_id_;
  }

  [[nodiscard]] TimeRange source_range() const noexcept {
    return source_range_;
  }

  [[nodiscard]] MediaTime timeline_start() const noexcept {
    return timeline_start_;
  }

  [[nodiscard]] double audio_gain_db() const noexcept { return audio_gain_db_; }
  [[nodiscard]] bool muted() const noexcept { return muted_; }

  [[nodiscard]] MediaTime duration() const;
  [[nodiscard]] TimeRange timeline_range() const;

private:
  ClipId id_;
  MediaId media_id_;
  TimeRange source_range_;
  MediaTime timeline_start_;
  double audio_gain_db_;
  bool muted_;
};

} // namespace pulsar::domain
