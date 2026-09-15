#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>
#include <pulsar/domain/time/time_range.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/timeline.hpp>
#include <pulsar/domain/timeline/track.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace {

using Clock = std::chrono::steady_clock;
using Duration = std::chrono::duration<double, std::milli>;

volatile std::uint64_t benchmark_sink = 0;

using pulsar::domain::Clip;
using pulsar::domain::ClipId;
using pulsar::domain::MediaId;
using pulsar::domain::MediaTime;
using pulsar::domain::Rational;
using pulsar::domain::TimeRange;
using pulsar::domain::Timeline;
using pulsar::domain::TimelineId;
using pulsar::domain::Track;
using pulsar::domain::TrackId;
using pulsar::domain::TrackKind;

void consume(std::uint64_t value) {
  const std::uint64_t current = benchmark_sink;
  benchmark_sink = current ^ (value + 0x9e3779b97f4a7c15ULL + (current << 6U) + (current >> 2U));
}

MediaTime seconds(std::int64_t value) {
  return MediaTime{value, Rational{1, 1}};
}

Clip make_clip(std::uint64_t id, std::int64_t start_seconds) {
  return Clip{ClipId{id}, MediaId{id}, TimeRange{MediaTime::zero(), seconds(1)},
              seconds(start_seconds)};
}

template <typename Callable> Duration measure(Callable callable) {
  const auto start = Clock::now();
  callable();
  return std::chrono::duration_cast<Duration>(Clock::now() - start);
}

void print_result(std::string_view label, Duration elapsed) {
  std::cout << label << ": " << elapsed.count() << " ms\n";
}

void benchmark_media_time() {
  constexpr int iterations = 1'000'000;
  const MediaTime one_second = seconds(1);
  const MediaTime one_second_ms = MediaTime{1000, Rational{1, 1000}};
  MediaTime accumulator = MediaTime::zero();
  std::uint64_t matches = 0;

  const Duration elapsed = measure([&] {
    for (int index = 0; index < iterations; ++index) {
      if (one_second == one_second_ms) {
        ++matches;
      }
      accumulator = accumulator + one_second_ms - one_second;
    }
  });

  consume(matches);
  consume(static_cast<std::uint64_t>(accumulator.seconds().denominator()));

  std::cout << "MediaTime comparison and arithmetic\n";
  std::cout << "iterations: " << iterations << '\n';
  print_result("elapsed", elapsed);
}

void benchmark_track_insert(std::uint64_t clip_count) {
  const Duration elapsed = measure([&] {
    Track track{TrackId{1}, TrackKind::Video};
    for (std::uint64_t index = 0; index < clip_count; ++index) {
      track.insert_clip(make_clip(index + 1, static_cast<std::int64_t>(index)));
    }
    consume(track.clips().size());
    consume(static_cast<std::uint64_t>(track.duration().seconds().numerator()));
  });

  print_result(std::to_string(clip_count) + " clips", elapsed);
}

Timeline make_timeline(std::uint64_t clip_count, std::uint64_t track_count) {
  Timeline timeline{TimelineId{1}};

  for (std::uint64_t track_index = 0; track_index < track_count; ++track_index) {
    timeline.add_track(Track{TrackId{track_index + 1}, TrackKind::Video});
  }

  for (std::uint64_t index = 0; index < clip_count; ++index) {
    const TrackId track_id{(index % track_count) + 1};
    const auto local_start = static_cast<std::int64_t>(index / track_count);
    timeline.insert_clip(track_id, make_clip(index + 1, local_start));
  }

  return timeline;
}

void benchmark_timeline_lookup(std::uint64_t clip_count) {
  const std::uint64_t track_count = clip_count >= 10'000 ? 10 : 1;
  Timeline timeline = make_timeline(clip_count, track_count);
  const ClipId first{1};
  const ClipId middle{(clip_count / 2U) + 1U};
  const ClipId last{clip_count};
  const int iterations = clip_count >= 10'000 ? 1'000 : 10'000;

  const Duration elapsed = measure([&] {
    for (int index = 0; index < iterations; ++index) {
      consume(timeline.find_clip(first) != nullptr ? 1U : 0U);
      consume(timeline.find_clip(middle) != nullptr ? 1U : 0U);
      consume(timeline.find_clip(last) != nullptr ? 1U : 0U);
    }
  });

  print_result(std::to_string(clip_count) + " clips", elapsed);
}

} // namespace

int main() {
  std::cout << "PULSAR Benchmarks\n\n";
  std::cout << "These local benchmarks use std::chrono::steady_clock.\n";
  std::cout << "They are trend indicators, not scientific or CI thresholds.\n\n";

  benchmark_media_time();

  std::cout << "\nTrack insert\n";
  benchmark_track_insert(100);
  benchmark_track_insert(1'000);
  std::cout << "10000 clips: skipped (single-track insertion is O(n^2) and not reasonable for "
               "this local harness yet)\n";

  std::cout << "\nTimeline find_clip\n";
  benchmark_timeline_lookup(100);
  benchmark_timeline_lookup(1'000);
  benchmark_timeline_lookup(10'000);

  consume(benchmark_sink);
  return 0;
}
