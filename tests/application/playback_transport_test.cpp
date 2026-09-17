#include <pulsar/application/playback/monotonic_clock.hpp>
#include <pulsar/application/playback/playback_time.hpp>
#include <pulsar/application/playback/playback_transport.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

using pulsar::application::MonotonicClock;
using pulsar::application::MonotonicInstant;
using pulsar::application::PlaybackPosition;
using pulsar::application::PlaybackTransport;
using pulsar::application::PlaybackTransportState;
using pulsar::application::StreamTimestamp;
using pulsar::domain::MediaTime;
using pulsar::domain::Rational;

class FakeMonotonicClock final : public MonotonicClock {
public:
  [[nodiscard]] MonotonicInstant now() const override {
    ++now_calls_;
    return now_;
  }

  void advance(MediaTime delta) {
    now_ = MonotonicInstant{now_.value() + delta};
  }


  [[nodiscard]] int now_calls() const noexcept {
    return now_calls_;
  }

private:
  MonotonicInstant now_{MonotonicInstant::zero()};
  mutable int now_calls_{0};
};

int fail(std::string_view message) {
  std::cerr << "playback transport test failed: " << message << '\n';
  return 1;
}

template <typename Exception, typename Callable>
bool throws(Callable callable) {
  try {
    callable();
  } catch (const Exception&) {
    return true;
  } catch (...) {
    return false;
  }

  return false;
}

MediaTime seconds(std::int64_t value) {
  return MediaTime{value, Rational{1, 1}};
}

MediaTime fraction(std::int64_t numerator, std::int64_t denominator) {
  return MediaTime{numerator, Rational{1, denominator}};
}

PlaybackPosition position(MediaTime time) {
  return PlaybackPosition{time};
}

bool expect_position(const PlaybackTransport& transport, MediaTime expected) {
  return transport.position() == position(expected);
}

bool initial_state_tests() {
  FakeMonotonicClock clock;
  PlaybackTransport transport{clock};

  if (transport.state() != PlaybackTransportState::Paused || transport.is_playing()) {
    return false;
  }
  if (!expect_position(transport, MediaTime::zero())) {
    return false;
  }

  clock.advance(seconds(10));
  return expect_position(transport, MediaTime::zero());
}

bool play_advances_exactly_tests() {
  FakeMonotonicClock clock;
  PlaybackTransport transport{clock};

  transport.play();
  clock.advance(seconds(2));
  if (!expect_position(transport, seconds(2))) {
    return false;
  }

  const int calls_before_repeated_play = clock.now_calls();
  transport.play();
  if (clock.now_calls() != calls_before_repeated_play) {
    return false;
  }
  clock.advance(seconds(3));
  return expect_position(transport, seconds(5));
}

bool clock_capture_tests() {
  FakeMonotonicClock clock;
  PlaybackTransport transport{clock};

  int calls = clock.now_calls();
  transport.play();
  if (clock.now_calls() != calls + 1) {
    return false;
  }

  calls = clock.now_calls();
  transport.pause();
  if (clock.now_calls() != calls + 1) {
    return false;
  }

  calls = clock.now_calls();
  transport.pause();
  if (clock.now_calls() != calls) {
    return false;
  }

  calls = clock.now_calls();
  transport.seek(position(seconds(8)));
  if (clock.now_calls() != calls) {
    return false;
  }

  transport.play();
  calls = clock.now_calls();
  transport.seek(position(seconds(3)));
  if (clock.now_calls() != calls + 1) {
    return false;
  }

  calls = clock.now_calls();
  transport.play();
  return clock.now_calls() == calls;
}

bool pause_freezes_position_tests() {
  FakeMonotonicClock clock;
  PlaybackTransport transport{clock};

  transport.play();
  clock.advance(seconds(3));
  transport.pause();
  if (transport.state() != PlaybackTransportState::Paused || !expect_position(transport, seconds(3))) {
    return false;
  }

  clock.advance(seconds(7));
  if (!expect_position(transport, seconds(3))) {
    return false;
  }

  const int calls_before_repeated_pause = clock.now_calls();
  transport.pause();
  return clock.now_calls() == calls_before_repeated_pause && expect_position(transport, seconds(3));
}

bool seek_tests() {
  FakeMonotonicClock clock;
  PlaybackTransport transport{clock};

  transport.seek(position(seconds(12)));
  if (transport.state() != PlaybackTransportState::Paused || !expect_position(transport, seconds(12))) {
    return false;
  }
  clock.advance(seconds(5));
  if (!expect_position(transport, seconds(12))) {
    return false;
  }

  transport.play();
  clock.advance(seconds(10));
  transport.seek(position(seconds(4)));
  if (transport.state() != PlaybackTransportState::Playing || !expect_position(transport, seconds(4))) {
    return false;
  }
  clock.advance(seconds(2));
  return expect_position(transport, seconds(6));
}

bool pause_resume_continuity_tests() {
  FakeMonotonicClock clock;
  PlaybackTransport transport{clock};

  transport.play();
  clock.advance(seconds(2));
  transport.pause();
  clock.advance(seconds(20));
  transport.play();
  clock.advance(seconds(3));

  return expect_position(transport, seconds(5));
}

bool multiple_cycles_do_not_drift_tests() {
  FakeMonotonicClock clock;
  PlaybackTransport transport{clock};
  MediaTime expected = MediaTime::zero();
  const MediaTime step = fraction(1, 7);

  for (int index = 0; index < 12; ++index) {
    transport.play();
    clock.advance(step);
    transport.pause();
    expected = expected + step;
    clock.advance(fraction(1, 11));
  }

  return expect_position(transport, expected);
}

bool non_frame_aligned_time_tests() {
  FakeMonotonicClock clock;
  PlaybackTransport transport{clock};

  transport.play();
  clock.advance(fraction(1001, 24000));
  if (!expect_position(transport, fraction(1001, 24000))) {
    return false;
  }
  clock.advance(fraction(1001, 30000));
  if (!expect_position(transport, fraction(1001, 24000) + fraction(1001, 30000))) {
    return false;
  }
  clock.advance(fraction(1, 123456));
  return expect_position(transport,
                         fraction(1001, 24000) + fraction(1001, 30000) + fraction(1, 123456));
}

bool small_and_large_advances_tests() {
  FakeMonotonicClock clock;
  PlaybackTransport transport{clock};

  transport.play();
  clock.advance(MediaTime{1, Rational{1, 1'000'000'000}});
  if (!expect_position(transport, MediaTime{1, Rational{1, 1'000'000'000}})) {
    return false;
  }
  clock.advance(seconds(86'400));
  return expect_position(transport, MediaTime{1, Rational{1, 1'000'000'000}} + seconds(86'400));
}

bool nominal_types_tests() {
  const PlaybackPosition playback{seconds(1)};
  const StreamTimestamp stream{MediaTime{-5, Rational{1, 1000}}};
  const MonotonicInstant instant{seconds(2)};

  if (playback.value() != seconds(1) || stream.value() != MediaTime{-5, Rational{1, 1000}} ||
      instant.value() != seconds(2)) {
    return false;
  }
  if (!throws<std::invalid_argument>([] { (void)PlaybackPosition{seconds(-1)}; })) {
    return false;
  }
  if (!throws<std::invalid_argument>([] { (void)MonotonicInstant{seconds(-1)}; })) {
    return false;
  }
  return throws<std::invalid_argument>([] {
    (void)MonotonicInstant{seconds(1)}.elapsed_since(MonotonicInstant{seconds(2)});
  });
}

bool overflow_tests() {
  FakeMonotonicClock clock;
  PlaybackTransport transport{clock};
  transport.seek(PlaybackPosition{
      MediaTime{std::numeric_limits<std::int64_t>::max(), Rational{1, 1}}});
  transport.play();
  clock.advance(seconds(1));

  return throws<std::overflow_error>([&] { (void)transport.position(); });
}

} // namespace

int main() {
  if (!initial_state_tests()) {
    return fail("initial state tests");
  }
  if (!play_advances_exactly_tests()) {
    return fail("play advances exactly tests");
  }
  if (!clock_capture_tests()) {
    return fail("clock capture tests");
  }
  if (!pause_freezes_position_tests()) {
    return fail("pause freezes position tests");
  }
  if (!seek_tests()) {
    return fail("seek tests");
  }
  if (!pause_resume_continuity_tests()) {
    return fail("pause resume continuity tests");
  }
  if (!multiple_cycles_do_not_drift_tests()) {
    return fail("multiple cycles do not drift tests");
  }
  if (!non_frame_aligned_time_tests()) {
    return fail("non-frame-aligned time tests");
  }
  if (!small_and_large_advances_tests()) {
    return fail("small and large advances tests");
  }
  if (!nominal_types_tests()) {
    return fail("nominal types tests");
  }
  if (!overflow_tests()) {
    return fail("overflow tests");
  }

  return 0;
}
