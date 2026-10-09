#include <pulsar/application/playback/frame_coverage.hpp>

namespace pulsar::application {

FrameCoverage frame_coverage(domain::MediaTime target, domain::MediaTime pts,
                             std::optional<domain::MediaTime> duration,
                             std::optional<domain::MediaTime> next_pts, bool source_ended,
                             std::optional<domain::MediaTime> known_end, bool terminal_end_known) {
  using Kind = FrameCoverageKind;
  if (next_pts && target >= *next_pts) return {Kind::Advance, *next_pts, std::nullopt};
  if (target < pts) return {Kind::Gap, target, pts};
  if (duration && *duration > domain::MediaTime::zero()) {
    const auto end = pts + *duration;
    if (target < end) return {Kind::Covered, pts, next_pts && *next_pts < end ? *next_pts : end};
    if (next_pts) return {Kind::Gap, end, next_pts};
    return {source_ended ? Kind::Ended : Kind::NeedMore, end, end};
  }
  if (next_pts || target == pts) return {Kind::Covered, pts, next_pts};
  if (!source_ended) return {Kind::NeedMore, pts, std::nullopt};
  if (!terminal_end_known) return {Kind::Covered, pts, std::nullopt, true};
  if (known_end && target < *known_end) return {Kind::Covered, pts, known_end};
  return {Kind::Ended, pts, known_end};
}

} // namespace pulsar::application
