#include <pulsar/application/playback/presentation_scheduler.hpp>
#include <pulsar/application/playback/frame_coverage.hpp>

#include <type_traits>
#include <utility>

namespace pulsar::application {
namespace {

[[nodiscard]] bool stream_less(StreamTimestamp lhs, StreamTimestamp rhs) {
  return lhs.value() < rhs.value();
}


[[nodiscard]] bool stream_equal(StreamTimestamp lhs, StreamTimestamp rhs) {
  return lhs.value() == rhs.value();
}

} // namespace

PresentationScheduler::PresentationScheduler(SingleStreamPlaybackClockMap time_map)
    : time_map_{time_map} {}

void PresentationScheduler::reset() {
  current_.reset();
  lookahead_.reset();
  source_ended_ = false;
  known_presentation_end_.reset();
  terminal_end_known_ = false;
  last_presented_ordinal_.reset();
  stats_ = PresentationSchedulerStats{};
  temporal_error_.reset();
}

void PresentationScheduler::ingest(VideoReadOutcome outcome) {
  if (temporal_error_.has_value()) {
    return;
  }

  std::visit(
      [this](auto&& value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, VideoSample>) {
          accept_sample(std::move(value));
        } else if constexpr (std::is_same_v<Value, VideoSourceEndOfStream>) {
          source_ended_ = true;
          known_presentation_end_ = value.known_presentation_end;
          terminal_end_known_ = value.terminal_end_known;
        } else if constexpr (std::is_same_v<Value, VideoSourceCancelled>) {
          set_temporal_error("video source read was cancelled");
        } else if constexpr (std::is_same_v<Value, VideoSourceError>) {
          set_temporal_error(value.message);
        }
      },
      std::move(outcome));
}

PresentationDecision PresentationScheduler::evaluate(PlaybackPosition position) {
  if (temporal_error_.has_value()) {
    return PresentationTemporalError{*temporal_error_};
  }

  return evaluate_target(time_map_.to_stream_timestamp(position));
}

std::size_t PresentationScheduler::buffered_sample_count() const noexcept {
  std::size_t count = 0;
  if (current_.has_value()) {
    ++count;
  }
  if (lookahead_.has_value()) {
    ++count;
  }
  return count;
}

PresentationDecision PresentationScheduler::evaluate_target(StreamTimestamp target) {
  while (true) {
    if (!current_.has_value()) {
      if (source_ended_) {
        return PresentationEnded{known_presentation_end_, terminal_end_known_};
      }
      return starved();
    }

    auto start = sample_start(*current_);
    if (!start.has_value()) {
      set_temporal_error("video sample is missing presentation timestamp");
      return PresentationTemporalError{*temporal_error_};
    }

    const auto coverage = frame_coverage(target.value(), start->value(), current_->duration,
        lookahead_ ? std::optional{lookahead_->presentation_timestamp->value()} : std::nullopt,
        source_ended_, known_presentation_end_ ? std::optional{known_presentation_end_->value()} : std::nullopt,
        terminal_end_known_);
    const auto end = coverage.end ? std::optional{StreamTimestamp{*coverage.end}} : std::nullopt;
    switch (coverage.kind) {
    case FrameCoverageKind::Covered: return present_or_hold(*current_, coverage.terminal_end_unknown);
    case FrameCoverageKind::Gap: return PresentationGap{StreamTimestamp{coverage.start}, end};
    case FrameCoverageKind::Advance: (void)advance_past_current(); continue;
    case FrameCoverageKind::NeedMore: return starved();
    case FrameCoverageKind::Ended: return PresentationEnded{end, end.has_value() || terminal_end_known_};
    }
  }
}

bool PresentationScheduler::advance_past_current() {
  if (!lookahead_.has_value()) {
    return false;
  }

  if (!was_last_presented(*current_)) {
    ++stats_.skipped_presentation_count;
  }
  current_ = std::move(lookahead_);
  lookahead_.reset();
  return true;
}

void PresentationScheduler::accept_sample(VideoSample sample) {
  const auto incoming_start = sample_start(sample);
  if (!incoming_start.has_value()) {
    set_temporal_error("video sample is missing presentation timestamp");
    return;
  }

  // Re-exposure of the same physical sample (e.g. a retained pending seek
  // lookahead delivered again) is idempotent, never an error and never a
  // replacement. Same timestamp with a DIFFERENT ordinal still falls through
  // to the existing temporal validation below.
  if (current_.has_value()) {
    const auto current_start = sample_start(*current_);
    if (current_start.has_value() && sample.ordinal == current_->ordinal &&
        stream_equal(*incoming_start, *current_start)) {
      return;
    }
  }
  if (lookahead_.has_value()) {
    const auto lookahead_start = sample_start(*lookahead_);
    if (lookahead_start.has_value() && sample.ordinal == lookahead_->ordinal &&
        stream_equal(*incoming_start, *lookahead_start)) {
      return;
    }
  }

  if (lookahead_.has_value()) {
    set_temporal_error("presentation scheduler lookahead capacity exceeded");
    return;
  }

  if (!current_.has_value()) {
    current_ = std::move(sample);
    return;
  }

  const auto current_start = sample_start(*current_);
  if (!current_start.has_value()) {
    set_temporal_error("video sample is missing presentation timestamp");
    return;
  }

  if (stream_less(*incoming_start, *current_start)) {
    set_temporal_error("video sample timestamps decreased");
    return;
  }

  if (stream_equal(*incoming_start, *current_start)) {
    if (was_last_presented(*current_)) {
      set_temporal_error("duplicate timestamp arrived after that timestamp was presented");
      return;
    }
    current_ = std::move(sample);
    return;
  }

  lookahead_ = std::move(sample);
}

void PresentationScheduler::set_temporal_error(std::string message) {
  temporal_error_ = std::move(message);
}

std::optional<StreamTimestamp> PresentationScheduler::sample_start(const VideoSample& sample) const {
  return sample.presentation_timestamp;
}

bool PresentationScheduler::was_last_presented(const VideoSample& sample) const noexcept {
  return last_presented_ordinal_.has_value() && *last_presented_ordinal_ == sample.ordinal;
}

PresentationDecision PresentationScheduler::present_or_hold(const VideoSample& sample,
                                                            bool terminal_end_unknown) {
  if (was_last_presented(sample)) {
    return HoldPresentation{sample, terminal_end_unknown};
  }

  last_presented_ordinal_ = sample.ordinal;
  return PresentSample{sample, stats_.skipped_presentation_count, terminal_end_unknown};
}

PresentationDecision PresentationScheduler::starved() {
  ++stats_.starvation_count;
  return PresentationStarved{};
}

} // namespace pulsar::application
