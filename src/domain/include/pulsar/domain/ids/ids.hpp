#pragma once

#include <cstdint>
#include <stdexcept>

namespace pulsar::domain {
namespace detail {

template <typename Tag> class StrongId {
public:
  explicit constexpr StrongId(std::uint64_t value) : value_{value} {
    if (value_ == 0) {
      throw std::invalid_argument{"domain id value 0 is reserved as invalid"};
    }
  }

  [[nodiscard]] constexpr std::uint64_t value() const noexcept {
    return value_;
  }

private:
  std::uint64_t value_;
};

template <typename Tag>
[[nodiscard]] constexpr bool operator==(StrongId<Tag> lhs, StrongId<Tag> rhs) noexcept {
  return lhs.value() == rhs.value();
}

template <typename Tag>
[[nodiscard]] constexpr bool operator!=(StrongId<Tag> lhs, StrongId<Tag> rhs) noexcept {
  return !(lhs == rhs);
}

} // namespace detail

struct TimelineIdTag;
struct TrackIdTag;
struct ClipIdTag;
struct MediaIdTag;

using TimelineId = detail::StrongId<TimelineIdTag>;
using TrackId = detail::StrongId<TrackIdTag>;
using ClipId = detail::StrongId<ClipIdTag>;
using MediaId = detail::StrongId<MediaIdTag>;

} // namespace pulsar::domain
