#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/time_range.hpp>
#include <pulsar/domain/timeline/visual_transform.hpp>

#include <cstdint>

namespace pulsar::domain {

class Clip {
public:
  static constexpr double minimum_audio_gain_db = -60.0;
  static constexpr double maximum_audio_gain_db = 12.0;
  static constexpr std::int64_t maximum_fade_seconds = 60;

  // `fade_in` / `fade_out`: linear audio fades at the clip's start / end (zero = none). Their sum
  // may exceed a clip that was trimmed afterwards; the mixer limits them to the clip length.
  Clip(ClipId id, MediaId media_id, TimeRange source_range, MediaTime timeline_start,
       double audio_gain_db = 0.0, bool muted = false, MediaTime fade_in = MediaTime::zero(),
       MediaTime fade_out = MediaTime::zero(), VisualTransform visual_transform = {});

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
  [[nodiscard]] MediaTime fade_in() const noexcept { return fade_in_; }
  [[nodiscard]] MediaTime fade_out() const noexcept { return fade_out_; }
  [[nodiscard]] const VisualTransform& visual_transform() const noexcept { return visual_transform_; }

  [[nodiscard]] MediaTime duration() const;
  [[nodiscard]] TimeRange timeline_range() const;

private:
  ClipId id_;
  MediaId media_id_;
  TimeRange source_range_;
  MediaTime timeline_start_;
  double audio_gain_db_;
  bool muted_;
  MediaTime fade_in_;
  MediaTime fade_out_;
  VisualTransform visual_transform_;
};

} // namespace pulsar::domain
