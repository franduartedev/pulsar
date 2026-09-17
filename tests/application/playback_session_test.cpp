#include <pulsar/application/playback/monotonic_clock.hpp>
#include <pulsar/application/playback/playback_session.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace {

using pulsar::application::HoldPresentation;
using pulsar::application::MonotonicClock;
using pulsar::application::MonotonicInstant;
using pulsar::application::PlaybackAvailability;
using pulsar::application::PlaybackGeneration;
using pulsar::application::PlaybackIntent;
using pulsar::application::PlaybackMediaWorker;
using pulsar::application::PlaybackPosition;
using pulsar::application::PlaybackSession;
using pulsar::application::PlaybackWorkerCancelled;
using pulsar::application::PlaybackWorkerEndOfStream;
using pulsar::application::PlaybackWorkerEvent;
using pulsar::application::PlaybackWorkerSample;
using pulsar::application::PlaybackWorkerSeekComplete;
using pulsar::application::PresentSample;
using pulsar::application::PresentationDecision;
using pulsar::application::PresentationEnded;
using pulsar::application::PresentationStarved;
using pulsar::application::StreamTimestamp;
using pulsar::application::VideoReadOutcome;
using pulsar::application::VideoSample;
using pulsar::application::VideoSeekHit;
using pulsar::application::VideoSeekOutcome;
using pulsar::application::VideoSourceDescriptor;
using pulsar::application::VideoSourceEndOfStream;
using pulsar::domain::MediaTime;
using pulsar::domain::Rational;

class FakeMonotonicClock final : public MonotonicClock {
public:
  [[nodiscard]] MonotonicInstant now() const override {
    return now_;
  }

  void advance(MediaTime delta) {
    now_ = MonotonicInstant{now_.value() + delta};
  }

private:
  MonotonicInstant now_{MonotonicInstant::zero()};
};

class FakePlaybackWorker final : public PlaybackMediaWorker {
public:
  explicit FakePlaybackWorker(VideoSourceDescriptor descriptor) : descriptor_{descriptor} {}

  [[nodiscard]] VideoSourceDescriptor describe() const override {
    return descriptor_;
  }

  void request_read(PlaybackGeneration generation) override {
    last_read_generation = generation;
    ++read_request_count;
  }

  void request_seek_for_presentation(PlaybackGeneration generation, StreamTimestamp target) override {
    last_seek_generation = generation;
    last_seek_target = target;
    ++seek_request_count;
  }

  void cancel_pending() override {
    ++cancel_count;
  }

  [[nodiscard]] std::optional<PlaybackWorkerEvent> poll_event() override {
    if (events.empty()) {
      return std::nullopt;
    }
    auto event = std::move(events.front());
    events.pop_front();
    return event;
  }

  void push(PlaybackWorkerEvent event) {
    events.push_back(std::move(event));
  }

  VideoSourceDescriptor descriptor_;
  std::deque<PlaybackWorkerEvent> events;
  std::optional<PlaybackGeneration> last_read_generation;
  std::optional<PlaybackGeneration> last_seek_generation;
  std::optional<StreamTimestamp> last_seek_target;
  int read_request_count{0};
  int seek_request_count{0};
  int cancel_count{0};
};

int fail(std::string_view message) {
  std::cerr << "playback session test failed: " << message << '\n';
  return 1;
}

MediaTime seconds(std::int64_t value) {
  return MediaTime{value, Rational{1, 1}};
}

MediaTime millis(std::int64_t value) {
  return MediaTime{value, Rational{1, 1000}};
}

PlaybackPosition playback(MediaTime time) {
  return PlaybackPosition{time};
}

StreamTimestamp stream(MediaTime time) {
  return StreamTimestamp{time};
}

VideoSample sample(std::uint64_t ordinal, MediaTime pts) {
  return VideoSample{ordinal, stream(pts), std::nullopt};
}

bool is_present(const PresentationDecision& decision, std::uint64_t ordinal) {
  const auto* present = std::get_if<PresentSample>(&decision);
  return present != nullptr && present->sample.ordinal == ordinal;
}

bool is_hold(const PresentationDecision& decision, std::uint64_t ordinal) {
  const auto* hold = std::get_if<HoldPresentation>(&decision);
  return hold != nullptr && hold->sample.ordinal == ordinal;
}

bool is_starved(const PresentationDecision& decision) {
  return std::holds_alternative<PresentationStarved>(decision);
}

bool is_ended(const PresentationDecision& decision) {
  return std::holds_alternative<PresentationEnded>(decision);
}

std::pair<std::unique_ptr<PlaybackSession>, FakePlaybackWorker*> make_session(
    FakeMonotonicClock& clock, MediaTime origin = seconds(5)) {
  auto worker = std::make_unique<FakePlaybackWorker>(
      VideoSourceDescriptor{stream(origin), Rational{1, 1000}, std::nullopt, true});
  auto* raw = worker.get();
  return {std::make_unique<PlaybackSession>(clock, std::move(worker)), raw};
}

bool open_and_play_tests() {
  FakeMonotonicClock clock;
  auto [session, worker] = make_session(clock);

  session->open();
  if (session->availability() != PlaybackAvailability::Preparing || worker->seek_request_count != 1 ||
      !worker->last_seek_target.has_value() || worker->last_seek_target->value() != seconds(5)) {
    return false;
  }

  worker->push(PlaybackWorkerSeekComplete{
      *worker->last_seek_generation,
      VideoSeekHit{sample(1, seconds(5)), sample(2, MediaTime{5040, Rational{1, 1000}})}});
  if (!is_present(session->update(), 1) || session->availability() != PlaybackAvailability::Ready ||
      session->intent() != PlaybackIntent::Paused) {
    return false;
  }

  session->play();
  clock.advance(millis(20));
  if (!is_hold(session->update(), 1)) {
    return false;
  }
  session->pause();
  clock.advance(seconds(10));
  return session->position().value() == millis(20);
}

bool seek_while_playing_tests() {
  FakeMonotonicClock clock;
  auto [session, worker] = make_session(clock);
  session->open();
  worker->push(PlaybackWorkerSeekComplete{
      *worker->last_seek_generation,
      VideoSeekHit{sample(1, seconds(5)), sample(2, MediaTime{5040, Rational{1, 1000}})}});
  (void)session->update();

  session->play();
  clock.advance(seconds(1));
  session->seek(playback(millis(40)));
  if (session->availability() != PlaybackAvailability::Seeking ||
      !worker->last_seek_target.has_value() || worker->last_seek_target->value() != millis(5040)) {
    return false;
  }
  clock.advance(seconds(10));
  worker->push(PlaybackWorkerSeekComplete{
      *worker->last_seek_generation,
      VideoSeekHit{sample(3, millis(5040)), sample(4, millis(5080))}});
  if (!is_present(session->update(), 3) || session->availability() != PlaybackAvailability::Ready ||
      session->intent() != PlaybackIntent::Playing) {
    return false;
  }
  clock.advance(millis(10));
  return session->position().value() == millis(50);
}

bool pause_and_play_during_seek_tests() {
  FakeMonotonicClock clock;
  auto [session, worker] = make_session(clock);
  session->open();
  worker->push(PlaybackWorkerSeekComplete{
      *worker->last_seek_generation,
      VideoSeekHit{sample(1, seconds(5)), sample(2, millis(5040))}});
  (void)session->update();

  session->play();
  session->seek(playback(millis(80)));
  session->pause();
  worker->push(PlaybackWorkerSeekComplete{
      *worker->last_seek_generation,
      VideoSeekHit{sample(3, millis(5080)), sample(4, millis(5120))}});
  if (!is_present(session->update(), 3) || session->intent() != PlaybackIntent::Paused) {
    return false;
  }
  clock.advance(seconds(5));
  if (session->position().value() != millis(80)) {
    return false;
  }

  session->seek(playback(millis(120)));
  session->play();
  worker->push(PlaybackWorkerSeekComplete{
      *worker->last_seek_generation,
      VideoSeekHit{sample(5, millis(5120)), sample(6, millis(5160))}});
  if (!is_present(session->update(), 5) || session->intent() != PlaybackIntent::Playing) {
    return false;
  }
  clock.advance(millis(5));
  return session->position().value() == millis(125);
}

bool stale_and_eof_tests() {
  FakeMonotonicClock clock;
  auto [session, worker] = make_session(clock);
  session->open();
  const auto stale_generation = *worker->last_seek_generation;
  session->seek(playback(millis(40)));
  worker->push(PlaybackWorkerSeekComplete{
      stale_generation, VideoSeekHit{sample(1, seconds(5)), sample(2, millis(5040))}});
  if (!is_starved(session->update()) || session->stats().stale_event_count != 1) {
    return false;
  }
  worker->push(PlaybackWorkerSeekComplete{
      *worker->last_seek_generation,
      VideoSeekHit{sample(3, millis(5040)), sample(4, millis(5080))}});
  if (!is_present(session->update(), 3)) {
    return false;
  }

  session->seek(playback(millis(120)));
  worker->push(PlaybackWorkerSeekComplete{
      *worker->last_seek_generation,
      pulsar::application::VideoSeekEndOfStream{sample(5, millis(5120)), stream(millis(5160)), true}});
  if (!is_present(session->update(), 5)) {
    return false;
  }
  session->play();
  clock.advance(millis(40));
  const auto decision = session->update();
  return is_ended(decision) && session->availability() == PlaybackAvailability::Ended;
}

bool matching_cancelled_releases_read_tests() {
  FakeMonotonicClock clock;
  auto [session, worker] = make_session(clock);
  session->open();
  worker->push(PlaybackWorkerSeekComplete{
      *worker->last_seek_generation,
      VideoSeekHit{sample(1, seconds(5)), sample(2, millis(5040))}});
  if (!is_present(session->update(), 1) || session->availability() != PlaybackAvailability::Ready) {
    return false;
  }
  const auto ready_generation = *worker->last_seek_generation;

  // Drive the scheduler past the buffered samples so a read is outstanding.
  session->play();
  clock.advance(seconds(60));
  if (!is_starved(session->update()) || worker->read_request_count != 1 ||
      worker->last_read_generation != ready_generation) {
    return false;
  }

  // A current-generation cancellation releases the read pump without error.
  worker->push(PlaybackWorkerCancelled{ready_generation});
  if (!is_starved(session->update()) || worker->read_request_count != 2 ||
      session->stats().stale_event_count != 0 ||
      session->availability() != PlaybackAvailability::Ready) {
    return false;
  }
  return true;
}

bool stale_cancelled_keeps_read_tests() {
  FakeMonotonicClock clock;
  auto [session, worker] = make_session(clock);
  session->open();
  const auto old_generation = *worker->last_seek_generation;
  session->seek(playback(millis(40)));
  worker->push(PlaybackWorkerSeekComplete{
      *worker->last_seek_generation,
      VideoSeekHit{sample(1, seconds(5)), sample(2, millis(5040))}});
  if (!is_present(session->update(), 2)) {
    return false;
  }
  const auto current_generation = *worker->last_seek_generation;
  if (current_generation == old_generation) {
    return false;
  }

  session->play();
  clock.advance(seconds(60));
  if (!is_starved(session->update()) || worker->read_request_count != 1 ||
      worker->last_read_generation != current_generation) {
    return false;
  }
  const auto intent_before = session->intent();

  worker->push(PlaybackWorkerCancelled{old_generation});
  if (!is_starved(session->update()) || worker->read_request_count != 1 ||
      session->stats().stale_event_count != 1 ||
      session->availability() != PlaybackAvailability::Ready ||
      session->intent() != intent_before) {
    return false;
  }
  return true;
}

} // namespace

int main() {
  if (!open_and_play_tests()) {
    return fail("open and play tests");
  }
  if (!seek_while_playing_tests()) {
    return fail("seek while playing tests");
  }
  if (!pause_and_play_during_seek_tests()) {
    return fail("pause and play during seek tests");
  }
  if (!stale_and_eof_tests()) {
    return fail("stale and EOF tests");
  }
  if (!matching_cancelled_releases_read_tests()) {
    return fail("matching cancelled releases read tests");
  }
  if (!stale_cancelled_keeps_read_tests()) {
    return fail("stale cancelled keeps read tests");
  }

  return 0;
}
