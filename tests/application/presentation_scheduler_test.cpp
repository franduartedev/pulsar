#include <pulsar/application/playback/presentation_scheduler.hpp>
#include <pulsar/application/playback/video_source.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace {

using pulsar::application::HoldPresentation;
using pulsar::application::PlaybackPosition;
using pulsar::application::PresentSample;
using pulsar::application::PresentationDecision;
using pulsar::application::PresentationEnded;
using pulsar::application::PresentationGap;
using pulsar::application::PresentationScheduler;
using pulsar::application::PresentationStarved;
using pulsar::application::PresentationTemporalError;
using pulsar::application::SingleStreamPlaybackClockMap;
using pulsar::application::StreamTimestamp;
using pulsar::application::VideoReadOutcome;
using pulsar::application::VideoSample;
using pulsar::application::VideoSeekBeforeFirst;
using pulsar::application::VideoSeekEndOfStream;
using pulsar::application::VideoSeekGap;
using pulsar::application::VideoSeekHit;
using pulsar::application::VideoSeekOutcome;
using pulsar::application::VideoSource;
using pulsar::application::VideoSourceCancelled;
using pulsar::application::VideoSourceDescriptor;
using pulsar::application::VideoSourceEndOfStream;
using pulsar::application::VideoSourceError;
using pulsar::domain::MediaTime;
using pulsar::domain::Rational;

int fail(std::string_view message) {
  std::cerr << "presentation scheduler test failed: " << message << '\n';
  return 1;
}

MediaTime seconds(std::int64_t value) {
  return MediaTime{value, Rational{1, 1}};
}

MediaTime millis(std::int64_t value) {
  return MediaTime{value, Rational{1, 1000}};
}

MediaTime fraction(std::int64_t numerator, std::int64_t denominator) {
  return MediaTime{numerator, Rational{1, denominator}};
}

PlaybackPosition playback(MediaTime time) {
  return PlaybackPosition{time};
}

StreamTimestamp stream(MediaTime time) {
  return StreamTimestamp{time};
}

VideoSample sample(std::uint64_t ordinal, MediaTime pts,
                   std::optional<MediaTime> duration = std::nullopt) {
  return VideoSample{ordinal, stream(pts), duration};
}

VideoSample missing_timestamp_sample(std::uint64_t ordinal) {
  return VideoSample{ordinal, std::nullopt, std::nullopt};
}

class FakeVideoSource final : public VideoSource {
public:
  FakeVideoSource(VideoSourceDescriptor descriptor, std::vector<VideoSample> samples,
                  VideoSourceEndOfStream end)
      : descriptor_{descriptor}, samples_{std::move(samples)}, end_{end} {}

  [[nodiscard]] VideoSourceDescriptor describe() const override {
    return descriptor_;
  }

  [[nodiscard]] VideoReadOutcome read_next() override {
    if (read_index_ >= samples_.size()) {
      return end_;
    }
    return samples_[read_index_++];
  }

  [[nodiscard]] VideoSeekOutcome seek_for_presentation(StreamTimestamp target) override {
    if (!descriptor_.seekable) {
      return VideoSourceError{"source is not seekable"};
    }

    std::optional<VideoSample> previous;
    for (std::size_t index = 0; index < samples_.size(); ++index) {
      const VideoSample& current = samples_[index];
      if (!current.presentation_timestamp.has_value()) {
        return VideoSourceError{"video sample is missing presentation timestamp"};
      }

      const auto current_start = *current.presentation_timestamp;
      std::optional<StreamTimestamp> current_end;
      if (current.duration.has_value()) {
        current_end = StreamTimestamp{current_start.value() + *current.duration};
      } else if (index + 1 < samples_.size()) {
        if (!samples_[index + 1].presentation_timestamp.has_value()) {
          return VideoSourceError{"video sample is missing presentation timestamp"};
        }
        current_end = *samples_[index + 1].presentation_timestamp;
      } else if (end_.terminal_end_known && end_.known_presentation_end.has_value()) {
        current_end = *end_.known_presentation_end;
      }

      if (target.value() < current_start.value()) {
        if (previous.has_value()) {
          return VideoSeekGap{*previous, current};
        }
        return VideoSeekBeforeFirst{current};
      }

      if (!current_end.has_value() || target.value() < current_end->value()) {
        read_index_ = index + 1;
        return VideoSeekHit{current, index + 1 < samples_.size()
                                         ? std::optional<VideoSample>{samples_[index + 1]}
                                         : std::nullopt};
      }

      previous = current;
    }

    return VideoSeekEndOfStream{previous, end_.known_presentation_end, end_.terminal_end_known};
  }

private:
  VideoSourceDescriptor descriptor_;
  std::vector<VideoSample> samples_;
  VideoSourceEndOfStream end_;
  std::size_t read_index_{0};
};

PresentationScheduler scheduler_with_origin(MediaTime origin) {
  return PresentationScheduler{SingleStreamPlaybackClockMap{stream(origin)}};
}

bool is_present(const PresentationDecision& decision, std::uint64_t ordinal) {
  const auto* present = std::get_if<PresentSample>(&decision);
  return present != nullptr && present->sample.ordinal == ordinal;
}

bool is_hold(const PresentationDecision& decision, std::uint64_t ordinal) {
  const auto* hold = std::get_if<HoldPresentation>(&decision);
  return hold != nullptr && hold->sample.ordinal == ordinal;
}

bool is_present_or_hold(const PresentationDecision& decision, std::uint64_t ordinal) {
  return is_present(decision, ordinal) || is_hold(decision, ordinal);
}

bool is_gap(const PresentationDecision& decision) {
  return std::holds_alternative<PresentationGap>(decision);
}

bool is_starved(const PresentationDecision& decision) {
  return std::holds_alternative<PresentationStarved>(decision);
}

bool is_ended(const PresentationDecision& decision, bool exact) {
  const auto* ended = std::get_if<PresentationEnded>(&decision);
  return ended != nullptr && ended->exact_end_known == exact;
}

bool is_temporal_error(const PresentationDecision& decision) {
  return std::holds_alternative<PresentationTemporalError>(decision);
}

void ingest_next(PresentationScheduler& scheduler, VideoSource& source) {
  scheduler.ingest(source.read_next());
}

bool cfr_tests() {
  auto source = FakeVideoSource{
      VideoSourceDescriptor{stream(seconds(0)), Rational{1, 1000}, seconds(3), true},
      {sample(1, seconds(0)), sample(2, millis(40)), sample(3, millis(80))},
      VideoSourceEndOfStream{stream(millis(120)), true}};
  auto scheduler = scheduler_with_origin(seconds(0));

  ingest_next(scheduler, source);
  if (!is_present(scheduler.evaluate(playback(seconds(0))), 1)) {
    return false;
  }
  ingest_next(scheduler, source);
  if (!is_present_or_hold(scheduler.evaluate(playback(seconds(0))), 1)) {
    return false;
  }
  if (!is_hold(scheduler.evaluate(playback(millis(39))), 1)) {
    return false;
  }
  if (!is_present(scheduler.evaluate(playback(millis(40))), 2)) {
    return false;
  }
  ingest_next(scheduler, source);
  if (!is_present_or_hold(scheduler.evaluate(playback(millis(41))), 2)) {
    return false;
  }
  if (scheduler.buffered_sample_count() > 2) {
    return false;
  }

  return source.seek_for_presentation(stream(millis(41))).index() == 0;
}

bool vfr_and_rational_tests() {
  auto source = FakeVideoSource{
      VideoSourceDescriptor{stream(seconds(0)), Rational{1, 90000}, std::nullopt, true},
      {sample(1, seconds(0)), sample(2, fraction(1001, 24000)),
       sample(3, fraction(1001, 24000) + fraction(1001, 30000))},
      VideoSourceEndOfStream{std::nullopt, false}};
  auto scheduler = scheduler_with_origin(seconds(0));

  ingest_next(scheduler, source);
  ingest_next(scheduler, source);
  if (!is_present(scheduler.evaluate(playback(fraction(1, 50000))), 1)) {
    return false;
  }
  if (!is_present(scheduler.evaluate(playback(fraction(1001, 24000))), 2)) {
    return false;
  }
  ingest_next(scheduler, source);
  return is_present_or_hold(scheduler.evaluate(playback(fraction(1001, 24000) + fraction(1, 100000))), 2);
}

bool explicit_duration_gap_and_eof_tests() {
  auto source = FakeVideoSource{
      VideoSourceDescriptor{stream(seconds(0)), Rational{1, 1000}, seconds(1), true},
      {sample(1, seconds(0), millis(20)), sample(2, millis(40), millis(20))},
      VideoSourceEndOfStream{stream(millis(60)), true}};
  auto scheduler = scheduler_with_origin(seconds(0));

  ingest_next(scheduler, source);
  if (!is_present(scheduler.evaluate(playback(millis(19))), 1)) {
    return false;
  }
  ingest_next(scheduler, source);
  if (!is_gap(scheduler.evaluate(playback(millis(25))))) {
    return false;
  }
  if (!is_present(scheduler.evaluate(playback(millis(40))), 2)) {
    return false;
  }
  ingest_next(scheduler, source);
  return is_ended(scheduler.evaluate(playback(millis(60))), true);
}

bool unknown_eof_tests() {
  auto source = FakeVideoSource{
      VideoSourceDescriptor{stream(seconds(0)), Rational{1, 1000}, std::nullopt, true},
      {sample(1, seconds(0))}, VideoSourceEndOfStream{std::nullopt, false}};
  auto scheduler = scheduler_with_origin(seconds(0));

  ingest_next(scheduler, source);
  ingest_next(scheduler, source);
  const auto decision = scheduler.evaluate(playback(seconds(2)));
  const auto* present = std::get_if<PresentSample>(&decision);
  return present != nullptr && present->sample.ordinal == 1 && present->terminal_end_unknown;
}

bool seek_contract_tests() {
  auto source = FakeVideoSource{
      VideoSourceDescriptor{stream(seconds(5)), Rational{1, 1000}, std::nullopt, true},
      {sample(1, seconds(5)), sample(2, millis(5040)), sample(3, millis(5080))},
      VideoSourceEndOfStream{stream(millis(5120)), true}};

  auto exact = source.seek_for_presentation(stream(millis(5040)));
  auto* exact_hit = std::get_if<VideoSeekHit>(&exact);
  if (exact_hit == nullptr || exact_hit->sample.ordinal != 2 || !exact_hit->lookahead.has_value() ||
      exact_hit->lookahead->ordinal != 3) {
    return false;
  }

  auto between = source.seek_for_presentation(stream(millis(5041)));
  auto* between_hit = std::get_if<VideoSeekHit>(&between);
  if (between_hit == nullptr || between_hit->sample.ordinal != 2) {
    return false;
  }

  auto before = source.seek_for_presentation(stream(millis(4999)));
  if (!std::holds_alternative<VideoSeekBeforeFirst>(before)) {
    return false;
  }

  auto after = source.seek_for_presentation(stream(millis(5120)));
  return std::holds_alternative<VideoSeekEndOfStream>(after);
}

bool non_zero_and_negative_origin_tests() {
  auto non_zero = scheduler_with_origin(seconds(5));
  non_zero.ingest(sample(1, seconds(5)));
  non_zero.ingest(sample(2, millis(5040)));
  if (!is_present(non_zero.evaluate(playback(millis(1))), 1)) {
    return false;
  }

  auto negative = scheduler_with_origin(seconds(-2));
  negative.ingest(sample(1, seconds(-2)));
  negative.ingest(sample(2, MediaTime{-1960, Rational{1, 1000}}));
  return is_present(negative.evaluate(playback(millis(1))), 1);
}

bool duplicate_timestamp_tests() {
  auto scheduler = scheduler_with_origin(seconds(0));
  scheduler.ingest(sample(1, seconds(0)));
  scheduler.ingest(sample(2, seconds(0)));
  scheduler.ingest(sample(3, millis(40)));
  if (!is_present(scheduler.evaluate(playback(millis(1))), 2)) {
    return false;
  }

  auto late_duplicate = scheduler_with_origin(seconds(0));
  late_duplicate.ingest(sample(1, seconds(0), millis(40)));
  if (!is_present(late_duplicate.evaluate(playback(millis(1))), 1)) {
    return false;
  }
  late_duplicate.ingest(sample(2, seconds(0), millis(40)));
  return is_temporal_error(late_duplicate.evaluate(playback(millis(1))));
}

bool decreasing_and_missing_tests() {
  auto decreasing = scheduler_with_origin(seconds(0));
  decreasing.ingest(sample(1, millis(40)));
  decreasing.ingest(sample(2, seconds(0)));
  if (!is_temporal_error(decreasing.evaluate(playback(millis(1))))) {
    return false;
  }

  auto missing = scheduler_with_origin(seconds(0));
  missing.ingest(missing_timestamp_sample(1));
  return is_temporal_error(missing.evaluate(playback(seconds(0))));
}

bool late_frames_and_skipped_tests() {
  auto scheduler = scheduler_with_origin(seconds(0));
  scheduler.ingest(sample(1, seconds(0)));
  scheduler.ingest(sample(2, millis(40)));
  if (!is_present(scheduler.evaluate(playback(millis(40))), 2)) {
    return false;
  }
  if (scheduler.stats().skipped_presentation_count != 1) {
    return false;
  }
  scheduler.ingest(sample(3, millis(80)));
  if (!is_starved(scheduler.evaluate(playback(millis(81))))) {
    return false;
  }
  scheduler.ingest(sample(4, millis(120)));
  if (!is_present(scheduler.evaluate(playback(millis(81))), 3)) {
    return false;
  }
  return scheduler.stats().skipped_presentation_count == 1;
}

bool starvation_and_repeated_evaluation_tests() {
  auto scheduler = scheduler_with_origin(seconds(0));
  scheduler.ingest(sample(1, seconds(0)));
  if (!is_starved(scheduler.evaluate(playback(millis(1))))) {
    return false;
  }
  if (!is_starved(scheduler.evaluate(playback(millis(1))))) {
    return false;
  }
  return scheduler.stats().starvation_count == 2;
}

bool bounded_state_tests() {
  auto scheduler = scheduler_with_origin(seconds(0));
  scheduler.ingest(sample(1, seconds(0)));
  scheduler.ingest(sample(2, millis(40)));
  if (scheduler.buffered_sample_count() != 2) {
    return false;
  }
  scheduler.ingest(sample(3, millis(80)));
  return is_temporal_error(scheduler.evaluate(playback(millis(1))));
}

} // namespace

int main() {
  if (!cfr_tests()) {
    return fail("CFR tests");
  }
  if (!vfr_and_rational_tests()) {
    return fail("VFR and rational tests");
  }
  if (!explicit_duration_gap_and_eof_tests()) {
    return fail("explicit duration gap and EOF tests");
  }
  if (!unknown_eof_tests()) {
    return fail("unknown EOF tests");
  }
  if (!seek_contract_tests()) {
    return fail("seek contract tests");
  }
  if (!non_zero_and_negative_origin_tests()) {
    return fail("non-zero and negative origin tests");
  }
  if (!duplicate_timestamp_tests()) {
    return fail("duplicate timestamp tests");
  }
  if (!decreasing_and_missing_tests()) {
    return fail("decreasing and missing tests");
  }
  if (!late_frames_and_skipped_tests()) {
    return fail("late frames and skipped tests");
  }
  if (!starvation_and_repeated_evaluation_tests()) {
    return fail("starvation and repeated evaluation tests");
  }
  if (!bounded_state_tests()) {
    return fail("bounded state tests");
  }

  return 0;
}
