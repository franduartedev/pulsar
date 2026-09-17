#pragma once

#include <pulsar/application/playback/playback_time.hpp>
#include <pulsar/application/playback/video_source.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <variant>

namespace pulsar::application {

struct PresentSample {
  VideoSample sample;
  std::size_t skipped_presentation_count;
  bool terminal_end_unknown;
};

struct HoldPresentation {
  VideoSample sample;
  bool terminal_end_unknown;
};

struct PresentationGap {
  StreamTimestamp start;
  std::optional<StreamTimestamp> end;
};

struct PresentationStarved {};

struct PresentationEnded {
  std::optional<StreamTimestamp> end;
  bool exact_end_known;
};

struct PresentationTemporalError {
  std::string message;
};

using PresentationDecision = std::variant<PresentSample, HoldPresentation, PresentationGap,
                                          PresentationStarved, PresentationEnded,
                                          PresentationTemporalError>;

struct PresentationSchedulerStats {
  std::size_t skipped_presentation_count{0};
  std::size_t starvation_count{0};
};

class PresentationScheduler {
public:
  explicit PresentationScheduler(SingleStreamPlaybackClockMap time_map);

  void reset();
  void ingest(VideoReadOutcome outcome);
  [[nodiscard]] PresentationDecision evaluate(PlaybackPosition position);

  [[nodiscard]] PresentationSchedulerStats stats() const noexcept {
    return stats_;
  }

  [[nodiscard]] std::size_t buffered_sample_count() const noexcept;

private:
  [[nodiscard]] PresentationDecision evaluate_target(StreamTimestamp target);
  [[nodiscard]] PresentationDecision decide_for_current(StreamTimestamp target);
  [[nodiscard]] bool advance_past_current();
  void accept_sample(VideoSample sample);
  void set_temporal_error(std::string message);
  [[nodiscard]] std::optional<StreamTimestamp> sample_start(const VideoSample& sample) const;
  [[nodiscard]] std::optional<StreamTimestamp> explicit_sample_end(const VideoSample& sample) const;
  [[nodiscard]] bool was_last_presented(const VideoSample& sample) const noexcept;
  [[nodiscard]] PresentationDecision present_or_hold(const VideoSample& sample,
                                                     bool terminal_end_unknown);
  [[nodiscard]] PresentationDecision starved();

  SingleStreamPlaybackClockMap time_map_;
  std::optional<VideoSample> current_;
  std::optional<VideoSample> lookahead_;
  bool source_ended_{false};
  std::optional<StreamTimestamp> known_presentation_end_;
  bool terminal_end_known_{false};
  std::optional<std::uint64_t> last_presented_ordinal_;
  PresentationSchedulerStats stats_;
  std::optional<std::string> temporal_error_;
};

} // namespace pulsar::application
