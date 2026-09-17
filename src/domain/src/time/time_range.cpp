#include <pulsar/domain/time/time_range.hpp>

#include <stdexcept>

namespace pulsar::domain {

TimeRange::TimeRange(MediaTime start, MediaTime end) : start_{start}, end_{end} {
  if (end_ < start_) {
    throw std::invalid_argument{"time range end cannot be before start"};
  }
}

MediaTime TimeRange::duration() const {
  return end_ - start_;
}

bool TimeRange::contains(MediaTime time) const {
  return start_ <= time && time < end_;
}

bool TimeRange::overlaps(TimeRange other) const {
  if (start_ == end_ || other.start_ == other.end_) {
    return false;
  }

  return start_ < other.end_ && other.start_ < end_;
}

} // namespace pulsar::domain
