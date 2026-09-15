#pragma once

#include <pulsar/domain/time/media_time.hpp>

namespace pulsar::domain {

class TimeRange {
public:
  TimeRange(MediaTime start, MediaTime end);

  [[nodiscard]] MediaTime start() const noexcept {
    return start_;
  }

  [[nodiscard]] MediaTime end() const noexcept {
    return end_;
  }

  [[nodiscard]] MediaTime duration() const;
  [[nodiscard]] bool contains(MediaTime time) const;
  [[nodiscard]] bool overlaps(TimeRange other) const;

private:
  MediaTime start_;
  MediaTime end_;
};

} // namespace pulsar::domain
