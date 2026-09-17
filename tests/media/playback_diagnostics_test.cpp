// Deterministic diagnostics tests: mailbox stats and read-latency ring.
// No wall-clock threshold assertions; synthetic durations only.
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>
#include <pulsar/media/ffmpeg/decoded_visual_video_frame.hpp>
#include <pulsar/media/ffmpeg/detail/latency_sample_ring.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_playback_visual_mailbox.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

using pulsar::domain::MediaTime;
using pulsar::domain::Rational;
using pulsar::media::ffmpeg::DecodedVisualVideoFrame;
using pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox;
using pulsar::media::ffmpeg::detail::LatencySampleRing;

int fail(std::string_view message) {
  std::cerr << "playback diagnostics test failed: " << message << '\n';
  return 1;
}

DecodedVisualVideoFrame frame(std::int64_t value) {
  return DecodedVisualVideoFrame{0,
                                 {1, 1, 1, 1, std::nullopt, std::nullopt, "", "", "", ""},
                                 MediaTime{value, Rational{1, 1}},
                                 {}};
}

bool mailbox_stats_tests() {
  FFmpegPlaybackVisualMailbox mailbox{2};
  const auto epoch = mailbox.activate();
  auto stats = mailbox.stats();
  if (stats.published != 0 || stats.evicted != 0 || stats.high_water_mark != 0 ||
      stats.current_size != 0) {
    return false;
  }

  // Below capacity: published tracks, nothing evicted.
  mailbox.publish(epoch, 1, frame(1));
  stats = mailbox.stats();
  if (stats.published != 1 || stats.evicted != 0 || stats.high_water_mark != 1 ||
      stats.current_size != 1) {
    return false;
  }

  // Fill to capacity exactly.
  mailbox.publish(epoch, 2, frame(2));
  stats = mailbox.stats();
  if (stats.published != 2 || stats.evicted != 0 || stats.high_water_mark != 2 ||
      stats.current_size != 2) {
    return false;
  }

  // One more forces exactly one eviction; size stays at capacity.
  mailbox.publish(epoch, 3, frame(3));
  stats = mailbox.stats();
  if (stats.published != 3 || stats.evicted != 1 || stats.high_water_mark != 2 ||
      stats.current_size != 2) {
    return false;
  }

  // take() shrinks current size without touching lifetime counters.
  if (!mailbox.take(epoch, 2).has_value()) {
    return false;
  }
  stats = mailbox.stats();
  if (stats.published != 3 || stats.evicted != 1 || stats.high_water_mark != 2 ||
      stats.current_size != 1) {
    return false;
  }

  // Stale-epoch publish is ignored entirely.
  mailbox.publish(epoch + 100, 4, frame(4));
  stats = mailbox.stats();
  if (stats.published != 3 || stats.evicted != 1 || stats.current_size != 1) {
    return false;
  }

  // clear() drops entries but preserves lifetime counters.
  mailbox.clear();
  stats = mailbox.stats();
  if (stats.current_size != 0 || stats.published != 3 || stats.evicted != 1 ||
      stats.high_water_mark != 2) {
    return false;
  }
  return true;
}

bool latency_ring_tests() {
  LatencySampleRing ring;
  if (ring.size() != 0 || !ring.snapshot().empty()) {
    return false;
  }

  using namespace std::chrono_literals;
  ring.push(30ns);
  ring.push(10ns);
  if (ring.size() != 2) {
    return false;
  }
  auto partial = ring.snapshot();
  if (partial.size() != 2 || partial[0] != 30ns || partial[1] != 10ns) {
    return false;
  }

  // Overfill: oldest samples fall off, newest capacity samples remain in order.
  for (std::uint64_t i = 0; i < LatencySampleRing::capacity + 5; ++i) {
    ring.push(std::chrono::nanoseconds{static_cast<long long>(100 + i)});
  }
  if (ring.size() != LatencySampleRing::capacity) {
    return false;
  }
  auto full = ring.snapshot();
  if (full.size() != LatencySampleRing::capacity || full.front() != std::chrono::nanoseconds{105} ||
      full.back() != std::chrono::nanoseconds{100 + LatencySampleRing::capacity + 5 - 1}) {
    return false;
  }

  // Ordering/max over synthetic durations (sorted copy, as reporters do).
  std::sort(full.begin(), full.end());
  if (full.front() != std::chrono::nanoseconds{105} ||
      full.back() != std::chrono::nanoseconds{100 + LatencySampleRing::capacity + 5 - 1}) {
    return false;
  }
  return true;
}

}  // namespace

int main() {
  if (!mailbox_stats_tests()) {
    return fail("mailbox stats tests");
  }
  if (!latency_ring_tests()) {
    return fail("latency ring tests");
  }
  return 0;
}
