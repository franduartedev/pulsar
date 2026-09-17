#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>
#include <pulsar/domain/time/time_range.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/timeline.hpp>
#include <pulsar/domain/timeline/track.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

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

int fail(std::string_view message) {
  std::cerr << "timeline model test failed: " << message << '\n';
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

MediaTime millis(std::int64_t value) {
  return MediaTime{value, Rational{1, 1000}};
}

Clip clip(ClipId id, MediaId media_id, MediaTime timeline_start, MediaTime duration) {
  return Clip{id, media_id, TimeRange{MediaTime::zero(), duration}, timeline_start};
}

bool id_tests() {
  static_assert(!std::is_same_v<ClipId, TrackId>);
  static_assert(!std::is_same_v<TimelineId, TrackId>);
  static_assert(!std::is_convertible_v<ClipId, TrackId>);
  static_assert(std::is_same_v<decltype(std::declval<const Timeline&>().tracks()),
                               const std::vector<Track>&>);
  static_assert(std::is_same_v<decltype(std::declval<const Track&>().clips()),
                               const std::vector<Clip>&>);
  static_assert(std::is_same_v<decltype(std::declval<const Timeline&>().find_track(TrackId{1})),
                               const Track*>);
  static_assert(std::is_same_v<decltype(std::declval<const Timeline&>().find_clip(ClipId{1})),
                               const Clip*>);

  if (ClipId{1} != ClipId{1}) {
    return false;
  }
  if (ClipId{1} == ClipId{2}) {
    return false;
  }
  if (!throws<std::invalid_argument>([] { (void)ClipId{0}; })) {
    return false;
  }
  if (!throws<std::invalid_argument>([] { (void)TrackId{0}; })) {
    return false;
  }
  if (!throws<std::invalid_argument>([] { (void)TimelineId{0}; })) {
    return false;
  }
  if (!throws<std::invalid_argument>([] { (void)MediaId{0}; })) {
    return false;
  }

  return true;
}

bool time_range_tests() {
  const TimeRange range{seconds(0), seconds(5)};
  if (range.duration() != seconds(5)) {
    return false;
  }
  if (!range.contains(seconds(0))) {
    return false;
  }
  if (range.contains(seconds(5))) {
    return false;
  }
  if (range.overlaps(TimeRange{seconds(5), seconds(10)})) {
    return false;
  }
  if (!range.overlaps(TimeRange{seconds(4), seconds(10)})) {
    return false;
  }
  if (!range.overlaps(TimeRange{seconds(1), seconds(2)})) {
    return false;
  }
  if (range.duration() != TimeRange{millis(0), millis(5000)}.duration()) {
    return false;
  }
  if (!throws<std::invalid_argument>([] { (void)TimeRange{seconds(5), seconds(4)}; })) {
    return false;
  }
  const TimeRange empty{seconds(5), seconds(5)};
  if (empty.duration() != MediaTime::zero()) {
    return false;
  }
  if (empty.contains(seconds(5))) {
    return false;
  }
  if (empty.overlaps(empty)) {
    return false;
  }
  if (empty.overlaps(TimeRange{seconds(0), seconds(10)})) {
    return false;
  }
  if (TimeRange{seconds(0), seconds(10)}.overlaps(empty)) {
    return false;
  }
  if (empty.overlaps(TimeRange{seconds(5), seconds(10)})) {
    return false;
  }
  if (empty.overlaps(TimeRange{seconds(0), seconds(5)})) {
    return false;
  }
  if (TimeRange{seconds(0), seconds(5)}.overlaps(empty)) {
    return false;
  }
  if (TimeRange{seconds(5), seconds(5)}.overlaps(TimeRange{seconds(5), seconds(5)})) {
    return false;
  }
  if (!TimeRange{seconds(0), seconds(10)}.overlaps(TimeRange{seconds(4), seconds(6)})) {
    return false;
  }
  if (TimeRange{seconds(0), seconds(5)}.overlaps(TimeRange{seconds(5), seconds(10)})) {
    return false;
  }

  return true;
}

bool media_time_arithmetic_tests() {
  if (seconds(1) + millis(500) != millis(1500)) {
    return false;
  }
  if (seconds(5) - millis(2500) != millis(2500)) {
    return false;
  }

  return true;
}

bool clip_tests() {
  const Clip valid{ClipId{1}, MediaId{1}, TimeRange{seconds(1), seconds(6)}, seconds(2)};
  if (valid.duration() != seconds(5)) {
    return false;
  }
  const TimeRange timeline_range = valid.timeline_range();
  if (timeline_range.start() != seconds(2) || timeline_range.end() != seconds(7)) {
    return false;
  }
  if (!throws<std::invalid_argument>(
          [] { (void)Clip{ClipId{2}, MediaId{1}, TimeRange{seconds(0), seconds(0)}, seconds(0)}; })) {
    return false;
  }
  if (!throws<std::invalid_argument>(
          [] { (void)Clip{ClipId{3}, MediaId{1}, TimeRange{seconds(0), seconds(1)}, seconds(-1)}; })) {
    return false;
  }
  if (!throws<std::invalid_argument>(
          [] { (void)Clip{ClipId{4}, MediaId{1}, TimeRange{seconds(-1), seconds(1)}, seconds(0)}; })) {
    return false;
  }

  return true;
}

bool track_tests() {
  Track track{TrackId{1}, TrackKind::Video};
  track.insert_clip(clip(ClipId{2}, MediaId{2}, seconds(5), seconds(5)));
  track.insert_clip(clip(ClipId{1}, MediaId{1}, seconds(0), seconds(5)));

  if (track.clips().size() != 2) {
    return false;
  }
  if (track.clips()[0].id() != ClipId{1} || track.clips()[1].id() != ClipId{2}) {
    return false;
  }
  if (track.duration() != seconds(10)) {
    return false;
  }
  if (!throws<std::invalid_argument>(
          [&track] { track.insert_clip(clip(ClipId{3}, MediaId{3}, seconds(4), seconds(2))); })) {
    return false;
  }
  if (!throws<std::invalid_argument>(
          [&track] { track.insert_clip(clip(ClipId{1}, MediaId{4}, seconds(10), seconds(1))); })) {
    return false;
  }
  if (track.clips().size() != 2 || track.clips()[0].id() != ClipId{1} ||
      track.clips()[1].id() != ClipId{2}) {
    return false;
  }

  Track atomic_track{TrackId{2}, TrackKind::Audio};
  atomic_track.insert_clip(clip(ClipId{10}, MediaId{10}, seconds(0), seconds(5)));
  atomic_track.insert_clip(clip(ClipId{11}, MediaId{11}, seconds(10), seconds(5)));
  if (!throws<std::invalid_argument>([&atomic_track] {
        atomic_track.insert_clip(clip(ClipId{12}, MediaId{12}, seconds(4), seconds(7)));
      })) {
    return false;
  }
  if (atomic_track.clips().size() != 2 || atomic_track.clips()[0].id() != ClipId{10} ||
      atomic_track.clips()[1].id() != ClipId{11}) {
    return false;
  }

  Track exact_order{TrackId{3}, TrackKind::Video};
  exact_order.insert_clip(clip(ClipId{21}, MediaId{21}, millis(1500), millis(250)));
  exact_order.insert_clip(clip(ClipId{20}, MediaId{20}, seconds(1), millis(250)));
  if (exact_order.clips()[0].id() != ClipId{20} || exact_order.clips()[1].id() != ClipId{21}) {
    return false;
  }

  Track consecutive{TrackId{4}, TrackKind::Video};
  consecutive.insert_clip(clip(ClipId{30}, MediaId{30}, seconds(0), seconds(1)));
  consecutive.insert_clip(clip(ClipId{31}, MediaId{31}, millis(1000), seconds(1)));
  if (consecutive.clips().size() != 2) {
    return false;
  }

  Track overflow_track{TrackId{5}, TrackKind::Video};
  overflow_track.insert_clip(clip(ClipId{40}, MediaId{40}, seconds(0), seconds(1)));
  if (!throws<std::overflow_error>([&overflow_track] {
        overflow_track.insert_clip(
            clip(ClipId{41}, MediaId{41}, MediaTime{std::numeric_limits<std::int64_t>::max(),
                                                   Rational{1, 1}},
                 seconds(1)));
      })) {
    return false;
  }
  if (overflow_track.clips().size() != 1 || overflow_track.clips()[0].id() != ClipId{40}) {
    return false;
  }

  return true;
}

bool timeline_tests() {
  Timeline timeline{TimelineId{1}};
  if (timeline.duration() != MediaTime::zero()) {
    return false;
  }

  timeline.add_track(Track{TrackId{1}, TrackKind::Video});
  timeline.add_track(Track{TrackId{2}, TrackKind::Audio});

  if (timeline.tracks().size() != 2 || timeline.find_track(TrackId{1}) == nullptr) {
    return false;
  }
  if (!throws<std::invalid_argument>([&timeline] {
        timeline.add_track(Track{TrackId{1}, TrackKind::Video});
      })) {
    return false;
  }

  timeline.insert_clip(TrackId{1}, clip(ClipId{1}, MediaId{1}, seconds(0), seconds(5)));
  timeline.insert_clip(TrackId{2}, clip(ClipId{2}, MediaId{2}, millis(2500), millis(7500)));

  if (timeline.find_clip(ClipId{2}) == nullptr) {
    return false;
  }
  if (timeline.duration() != seconds(10)) {
    return false;
  }
  if (!throws<std::invalid_argument>(
          [&timeline] { timeline.insert_clip(TrackId{2}, clip(ClipId{1}, MediaId{3}, seconds(10), seconds(1))); })) {
    return false;
  }
  if (!throws<std::invalid_argument>(
          [&timeline] { timeline.insert_clip(TrackId{99}, clip(ClipId{3}, MediaId{3}, seconds(10), seconds(1))); })) {
    return false;
  }
  if (timeline.tracks().size() != 2 || timeline.find_clip(ClipId{3}) != nullptr) {
    return false;
  }

  Track incoming{TrackId{3}, TrackKind::Video};
  incoming.insert_clip(clip(ClipId{2}, MediaId{7}, seconds(10), seconds(1)));
  if (!throws<std::invalid_argument>([&timeline, &incoming] { timeline.add_track(incoming); })) {
    return false;
  }
  if (timeline.tracks().size() != 2 || timeline.find_track(TrackId{3}) != nullptr) {
    return false;
  }

  Track reusable_media_track{TrackId{4}, TrackKind::Video};
  reusable_media_track.insert_clip(clip(ClipId{4}, MediaId{7}, seconds(0), seconds(1)));
  reusable_media_track.insert_clip(clip(ClipId{5}, MediaId{7}, seconds(1), seconds(1)));
  timeline.add_track(reusable_media_track);
  if (timeline.find_clip(ClipId{4}) == nullptr || timeline.find_clip(ClipId{5}) == nullptr) {
    return false;
  }

  return true;
}

bool timeline_remove_tests() {
  Timeline timeline{TimelineId{10}};
  timeline.add_track(Track{TrackId{10}, TrackKind::Video});
  timeline.insert_clip(TrackId{10}, clip(ClipId{10}, MediaId{10}, seconds(0), seconds(5)));
  timeline.insert_clip(TrackId{10}, clip(ClipId{11}, MediaId{11}, seconds(5), seconds(5)));

  timeline.remove_clip(ClipId{10});
  if (timeline.find_clip(ClipId{10}) != nullptr || timeline.find_clip(ClipId{11}) == nullptr) {
    return false;
  }
  if (timeline.find_track(TrackId{10})->clips()[0].id() != ClipId{11}) {
    return false;
  }
  if (!throws<std::invalid_argument>([&timeline] { timeline.remove_clip(ClipId{99}); })) {
    return false;
  }
  if (timeline.find_track(TrackId{10})->clips().size() != 1) {
    return false;
  }

  timeline.remove_clip(ClipId{11});
  if (timeline.duration() != MediaTime::zero()) {
    return false;
  }

  return true;
}

bool timeline_move_tests() {
  Timeline timeline{TimelineId{20}};
  timeline.add_track(Track{TrackId{20}, TrackKind::Video});
  timeline.add_track(Track{TrackId{21}, TrackKind::Audio});
  timeline.insert_clip(TrackId{20}, clip(ClipId{20}, MediaId{20}, seconds(0), seconds(5)));
  timeline.insert_clip(TrackId{20}, clip(ClipId{21}, MediaId{21}, seconds(10), seconds(5)));
  timeline.insert_clip(TrackId{21}, clip(ClipId{22}, MediaId{22}, seconds(0), seconds(5)));

  const TimeRange original_source = timeline.find_clip(ClipId{20})->source_range();
  timeline.move_clip(ClipId{20}, TrackId{20}, millis(5000));
  if (timeline.find_clip(ClipId{20})->timeline_start() != seconds(5) ||
      timeline.find_clip(ClipId{20})->source_range().start() != original_source.start() ||
      timeline.find_clip(ClipId{20})->source_range().end() != original_source.end() ||
      timeline.find_clip(ClipId{21})->timeline_start() != seconds(10)) {
    return false;
  }
  if (timeline.find_track(TrackId{20})->clips()[0].id() != ClipId{20}) {
    return false;
  }

  if (!throws<std::invalid_argument>(
          [&timeline] { timeline.move_clip(ClipId{20}, TrackId{20}, seconds(9)); })) {
    return false;
  }
  if (timeline.find_clip(ClipId{20})->timeline_start() != seconds(5)) {
    return false;
  }

  if (!throws<std::invalid_argument>(
          [&timeline] { timeline.move_clip(ClipId{20}, TrackId{20}, seconds(-1)); })) {
    return false;
  }
  if (timeline.find_clip(ClipId{20})->timeline_start() != seconds(5)) {
    return false;
  }

  timeline.move_clip(ClipId{20}, TrackId{21}, seconds(5));
  if (timeline.find_track(TrackId{20})->find_clip(ClipId{20}) != nullptr ||
      timeline.find_clip(ClipId{20})->source_range().start() != original_source.start() ||
      timeline.find_clip(ClipId{20})->source_range().end() != original_source.end()) {
    return false;
  }
  if (timeline.find_track(TrackId{21})->find_clip(ClipId{20}) == nullptr) {
    return false;
  }

  if (!throws<std::invalid_argument>(
          [&timeline] { timeline.move_clip(ClipId{21}, TrackId{99}, seconds(0)); })) {
    return false;
  }
  if (timeline.find_track(TrackId{20})->find_clip(ClipId{21}) == nullptr) {
    return false;
  }

  if (!throws<std::invalid_argument>(
          [&timeline] { timeline.move_clip(ClipId{21}, TrackId{21}, seconds(2)); })) {
    return false;
  }
  if (timeline.find_track(TrackId{20})->find_clip(ClipId{21}) == nullptr ||
      timeline.find_track(TrackId{21})->find_clip(ClipId{21}) != nullptr) {
    return false;
  }

  timeline.move_clip(ClipId{21}, TrackId{21}, millis(10'000));
  if (timeline.find_track(TrackId{21})->find_clip(ClipId{21}) == nullptr) {
    return false;
  }

  return true;
}

bool lookup_tests() {
  Timeline timeline{TimelineId{30}};
  timeline.add_track(Track{TrackId{30}, TrackKind::Video});
  timeline.insert_clip(TrackId{30}, clip(ClipId{30}, MediaId{30}, seconds(0), seconds(1)));
  timeline.insert_clip(TrackId{30}, clip(ClipId{31}, MediaId{31}, seconds(1), seconds(1)));
  timeline.insert_clip(TrackId{30}, clip(ClipId{32}, MediaId{32}, seconds(3), seconds(1)));
  if (timeline.find_clip_at(TrackId{30}, millis(-1)).has_value()) return false;
  if (!timeline.find_clip_at(TrackId{30}, seconds(0)).has_value() ||
      timeline.find_clip_at(TrackId{30}, seconds(0))->id() != ClipId{30}) return false;
  if (timeline.find_clip_at(TrackId{30}, millis(500))->id() != ClipId{30}) return false;
  if (timeline.find_clip_at(TrackId{30}, seconds(1))->id() != ClipId{31}) return false;
  if (timeline.find_clip_at(TrackId{30}, seconds(2)).has_value()) return false;
  if (timeline.find_clip_at(TrackId{30}, seconds(3))->id() != ClipId{32}) return false;
  if (timeline.find_clip_at(TrackId{30}, seconds(4)).has_value()) return false;
  return true;
}

MediaTime eighths(std::int64_t value) { return MediaTime{value, Rational{1, 8}}; }

bool same_range(TimeRange lhs, TimeRange rhs) {
  return lhs.start() == rhs.start() && lhs.end() == rhs.end();
}

bool trim_tests() {
  Timeline start_timeline{TimelineId{31}};
  start_timeline.add_track(Track{TrackId{31}, TrackKind::Video});
  start_timeline.insert_clip(TrackId{31}, Clip{ClipId{31}, MediaId{31},
                                TimeRange{eighths(16), eighths(80)}, eighths(40)});
  start_timeline.trim_clip_start(ClipId{31}, eighths(49));
  const Clip* start_clip = start_timeline.find_clip(ClipId{31});
  if (start_clip == nullptr || !same_range(start_clip->timeline_range(),
          TimeRange{eighths(49), eighths(104)}) ||
      !same_range(start_clip->source_range(), TimeRange{eighths(25), eighths(80)}) ||
      start_clip->duration() != eighths(55)) return false;
  if (!throws<std::invalid_argument>([&start_timeline] {
        start_timeline.trim_clip_start(ClipId{31}, eighths(49));
      }) ||
      !throws<std::invalid_argument>([&start_timeline] {
        start_timeline.trim_clip_start(ClipId{31}, eighths(104));
      }) ||
      !throws<std::invalid_argument>([&start_timeline] {
        start_timeline.trim_clip_start(ClipId{31}, eighths(105));
      })) return false;

  Timeline end_timeline{TimelineId{32}};
  end_timeline.add_track(Track{TrackId{32}, TrackKind::Video});
  end_timeline.insert_clip(TrackId{32}, Clip{ClipId{32}, MediaId{32},
                              TimeRange{eighths(16), eighths(80)}, eighths(40)});
  end_timeline.trim_clip_end(ClipId{32}, eighths(71));
  const Clip* end_clip = end_timeline.find_clip(ClipId{32});
  if (end_clip == nullptr || !same_range(end_clip->timeline_range(),
          TimeRange{eighths(40), eighths(71)}) ||
      !same_range(end_clip->source_range(), TimeRange{eighths(16), eighths(47)}) ||
      end_clip->duration() != eighths(31)) return false;
  if (!throws<std::invalid_argument>([&end_timeline] {
        end_timeline.trim_clip_end(ClipId{32}, eighths(40));
      }) ||
      !throws<std::invalid_argument>([&end_timeline] {
        end_timeline.trim_clip_end(ClipId{32}, eighths(71));
      }) ||
      !throws<std::invalid_argument>([&end_timeline] {
        end_timeline.trim_clip_end(ClipId{32}, eighths(39));
      })) return false;
  return true;
}

bool split_tests() {
  Timeline timeline{TimelineId{33}};
  timeline.add_track(Track{TrackId{33}, TrackKind::Video});
  timeline.insert_clip(TrackId{33}, Clip{ClipId{33}, MediaId{33},
                           TimeRange{eighths(8), eighths(88)}, eighths(16)});
  timeline.insert_clip(TrackId{33}, clip(ClipId{34}, MediaId{34}, eighths(96), eighths(8)));
  timeline.split_clip(ClipId{33}, eighths(56), ClipId{35});
  const Clip* left = timeline.find_clip(ClipId{33});
  const Clip* right = timeline.find_clip(ClipId{35});
  if (left == nullptr || right == nullptr || !same_range(left->timeline_range(),
          TimeRange{eighths(16), eighths(56)}) ||
      !same_range(right->timeline_range(), TimeRange{eighths(56), eighths(96)}) ||
      !same_range(left->source_range(), TimeRange{eighths(8), eighths(48)}) ||
      !same_range(right->source_range(), TimeRange{eighths(48), eighths(88)}) ||
      left->duration() + right->duration() != eighths(80) ||
      left->media_id() != right->media_id()) return false;
  if (timeline.find_clip_at(TrackId{33}, eighths(56))->id() != ClipId{35}) return false;
  const auto before_left = *left;
  const auto before_right = *right;
  if (!throws<std::invalid_argument>([&timeline] {
        timeline.split_clip(ClipId{33}, eighths(16), ClipId{36});
      }) ||
      !throws<std::invalid_argument>([&timeline] {
        timeline.split_clip(ClipId{33}, eighths(56), ClipId{34});
      }) ||
      !throws<std::invalid_argument>([&timeline] {
        timeline.split_clip(ClipId{33}, eighths(96), ClipId{36});
      })) return false;
  const Clip* unchanged_left = timeline.find_clip(ClipId{33});
  const Clip* unchanged_right = timeline.find_clip(ClipId{35});
  return unchanged_left != nullptr && unchanged_right != nullptr &&
         same_range(unchanged_left->source_range(), before_left.source_range()) &&
         same_range(unchanged_right->source_range(), before_right.source_range());
}

} // namespace

int main() {
  if (!id_tests()) {
    return fail("id tests");
  }
  if (!time_range_tests()) {
    return fail("time-range tests");
  }
  if (!media_time_arithmetic_tests()) {
    return fail("media-time arithmetic tests");
  }
  if (!clip_tests()) {
    return fail("clip tests");
  }
  if (!track_tests()) {
    return fail("track tests");
  }
  if (!timeline_tests()) {
    return fail("timeline tests");
  }
  if (!timeline_remove_tests()) {
    return fail("timeline remove tests");
  }
  if (!timeline_move_tests()) {
    return fail("timeline move tests");
  }
  if (!lookup_tests()) {
    return fail("timeline lookup tests");
  }
  if (!trim_tests()) {
    return fail("timeline trim tests");
  }
  if (!split_tests()) {
    return fail("timeline split tests");
  }

  return 0;
}
