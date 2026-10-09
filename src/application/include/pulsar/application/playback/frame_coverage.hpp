#pragma once

#include <pulsar/domain/time/media_time.hpp>
#include <optional>

namespace pulsar::application {

enum class FrameCoverageKind { Covered, Gap, Advance, NeedMore, Ended };

struct FrameCoverage {
  FrameCoverageKind kind;
  domain::MediaTime start;
  std::optional<domain::MediaTime> end;
  bool terminal_end_unknown{false};
};

// All times use the SAME coordinate system (source-relative or stream-absolute).
// A positive explicit duration defines [PTS, PTS+duration), truncated by next PTS.
// Missing/nonpositive duration: cover up to next PTS; if EOF has no known end,
// hold the final picture (the caller's editorial clip range still bounds it).
// Without lookahead/EOF, coverage beyond an unknown end needs more source data.
[[nodiscard]] FrameCoverage frame_coverage(
    domain::MediaTime target, domain::MediaTime pts,
    std::optional<domain::MediaTime> duration,
    std::optional<domain::MediaTime> next_pts, bool source_ended,
    std::optional<domain::MediaTime> known_end = std::nullopt,
    bool terminal_end_known = false);

} // namespace pulsar::application
