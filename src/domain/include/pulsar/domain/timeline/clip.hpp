#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/time_range.hpp>

namespace pulsar::domain {

class Clip {
public:
  Clip(ClipId id, MediaId media_id, TimeRange source_range, MediaTime timeline_start);

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

  [[nodiscard]] MediaTime duration() const;
  [[nodiscard]] TimeRange timeline_range() const;

private:
  ClipId id_;
  MediaId media_id_;
  TimeRange source_range_;
  MediaTime timeline_start_;
};

} // namespace pulsar::domain
