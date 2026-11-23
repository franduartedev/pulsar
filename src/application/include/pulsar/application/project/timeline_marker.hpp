#pragma once

#include <pulsar/domain/time/media_time.hpp>

#include <string>

namespace pulsar::application {

// Editorial annotation only: markers never affect timeline composition or export.
struct TimelineMarker {
  domain::MediaTime time;
  std::string label{};

  bool operator==(const TimelineMarker&) const = default;
};

} // namespace pulsar::application
