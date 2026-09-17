#pragma once

#include <pulsar/domain/time/media_time.hpp>


namespace pulsar::application {

class PlaybackPosition {
public:
  explicit PlaybackPosition(domain::MediaTime value);

  [[nodiscard]] static PlaybackPosition zero();

  [[nodiscard]] domain::MediaTime value() const noexcept {
    return value_;
  }

private:
  domain::MediaTime value_;
};

class StreamTimestamp {
public:
  explicit StreamTimestamp(domain::MediaTime value) noexcept : value_{value} {}

  [[nodiscard]] domain::MediaTime value() const noexcept {
    return value_;
  }

private:
  domain::MediaTime value_;
};

class MonotonicInstant {
public:
  explicit MonotonicInstant(domain::MediaTime value);

  [[nodiscard]] static MonotonicInstant zero();

  [[nodiscard]] domain::MediaTime value() const noexcept {
    return value_;
  }

  [[nodiscard]] domain::MediaTime elapsed_since(MonotonicInstant earlier) const;

private:
  domain::MediaTime value_;
};

[[nodiscard]] bool operator==(PlaybackPosition lhs, PlaybackPosition rhs);
[[nodiscard]] bool operator!=(PlaybackPosition lhs, PlaybackPosition rhs);
[[nodiscard]] bool operator==(StreamTimestamp lhs, StreamTimestamp rhs);
[[nodiscard]] bool operator!=(StreamTimestamp lhs, StreamTimestamp rhs);
[[nodiscard]] bool operator==(MonotonicInstant lhs, MonotonicInstant rhs);
[[nodiscard]] bool operator!=(MonotonicInstant lhs, MonotonicInstant rhs);

} // namespace pulsar::application
