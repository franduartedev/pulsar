#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/timeline/timeline_clip_resolver.hpp>
#include <pulsar/application/composition/visual_composition_resolver.hpp>

#include <cassert>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
using namespace pulsar;
using application::CommandProcessor;
using application::ProjectDocument;
using application::ProjectSnapshot;
using application::ResolvedClip;
using application::TimelineEnded;
using application::TimelineGap;
using application::TimelineResolveOutcome;
using domain::MediaId;
using domain::MediaTime;
using domain::Rational;
using domain::TimeRange;

MediaTime time(std::int64_t value, Rational base = Rational{1, 1}) {
  return MediaTime{value, base};
}

bool same_range(TimeRange lhs, TimeRange rhs) {
  return lhs.start() == rhs.start() && lhs.end() == rhs.end();
}

class FakeProbe final : public application::MediaProbe {
public:
  [[nodiscard]] application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    application::MediaProbeResult result;
    result.path = path;
    result.container_name = "test";
    result.video_streams.push_back(application::VideoStreamInfo{
        2, "h264", 1920, 1080, Rational{1, 90000}, time(100000, Rational{1, 1000}),
        std::nullopt});
    return result;
  }
};

struct Fixture {
  ProjectDocument document;
  CommandProcessor processor;
  FakeProbe probe;
  MediaId media{MediaId{1}};

  Fixture() : processor{document} { media = processor.import_asset("media/test.mp4", probe); }
};

ResolvedClip resolve_clip(const ProjectSnapshot& snapshot, MediaTime position) {
  const TimelineResolveOutcome outcome =
      application::resolve_clip_at(snapshot, snapshot.video_track_id, position);
  assert(std::holds_alternative<ResolvedClip>(outcome));
  return std::get<ResolvedClip>(outcome);
}

void boundaries_and_gaps() {
  Fixture fixture;
  const auto first = fixture.processor.add_clip(fixture.media, TimeRange{time(0), time(5)}, time(5));
  const auto second = fixture.processor.add_clip(fixture.media, TimeRange{time(5), time(10)}, time(10));
  const auto third = fixture.processor.add_clip(fixture.media, TimeRange{time(10), time(15)}, time(20));
  const auto snapshot = fixture.document.snapshot();

  assert(resolve_clip(snapshot, time(5)).clip_id == first);
  assert(resolve_clip(snapshot, time(9)).clip_id == first);
  assert(resolve_clip(snapshot, time(10)).clip_id == second);
  assert(resolve_clip(snapshot, time(14)).clip_id == second);
  assert(resolve_clip(snapshot, time(20)).clip_id == third);
  assert(std::holds_alternative<TimelineGap>(application::resolve_clip_at(
      snapshot, snapshot.video_track_id, time(0))));
  assert(std::holds_alternative<TimelineGap>(application::resolve_clip_at(
      snapshot, snapshot.video_track_id, time(15))));
  assert(std::holds_alternative<TimelineGap>(application::resolve_clip_at(
      snapshot, snapshot.video_track_id, time(17))));
  assert(std::holds_alternative<TimelineEnded>(application::resolve_clip_at(
      snapshot, snapshot.video_track_id, time(25))));
  const auto ended = std::get<TimelineEnded>(application::resolve_clip_at(
      snapshot, snapshot.video_track_id, time(25)));
  assert(ended.duration == time(25));
}

void visual_composition_order_gaps_and_boundaries() {
  Fixture fixture;
  const auto first = fixture.processor.add_clip(fixture.media, TimeRange{time(0), time(2)}, time(0));
  auto snapshot = fixture.document.snapshot();
  auto single = application::resolve_visual_composition(snapshot, time(0));
  assert(single.layers.size() == 1);
  assert(std::get<ResolvedClip>(single.layers[0].source).clip_id == first);

  domain::Track upper{domain::TrackId{9}, domain::TrackKind::Video};
  upper.insert_clip(domain::Clip{domain::ClipId{20}, fixture.media,
      TimeRange{time(10), time(11)}, time(1001, Rational{1, 30000})});
  snapshot.timeline.add_track(std::move(upper));
  snapshot.timeline.add_track(domain::Track{domain::TrackId{3}, domain::TrackKind::Video});
  snapshot.visual_track_ids = {domain::TrackId{3}, snapshot.video_track_id, domain::TrackId{9}};
  const auto original = snapshot.assets.front();
  snapshot.assets.front() = application::MediaAsset{
      original.id(), original.source_locator(), original.video_stream_index(),
      original.playable_duration(), original.metadata(), application::MediaAvailability::Available,
      std::nullopt, application::AudioAssetMetadata{1, 48000, 2, Rational{1, 48000}, time(100)}};
  auto before = application::resolve_visual_composition(snapshot, time(1000, Rational{1, 30000}));
  assert(std::holds_alternative<TimelineGap>(before.layers[0].source));
  assert(std::holds_alternative<TimelineGap>(before.layers[2].source));
  const auto boundary = time(1001, Rational{1, 30000});
  for (int repeat = 0; repeat < 3; ++repeat) {
    const auto active = application::resolve_visual_composition(snapshot, boundary);
    assert(active.timeline_position == boundary);
    for (std::size_t i = 0; i < active.layers.size(); ++i)
      assert(active.layers[i].track_id == snapshot.visual_track_ids[i]);
    assert(std::get<ResolvedClip>(active.layers[1].source).source_time == boundary);
    assert(std::get<ResolvedClip>(active.layers[1].source).audio.has_value());
    const auto& top = std::get<ResolvedClip>(active.layers[2].source);
    assert(top.source_time == time(10));
    assert(top.timeline_position == boundary);
    assert(!top.audio.has_value());
  }
  const auto end = application::resolve_visual_composition(snapshot, boundary + time(1));
  assert(std::holds_alternative<TimelineGap>(end.layers[2].source));
  assert(std::holds_alternative<ResolvedClip>(end.layers[1].source));
  const auto ended = application::resolve_visual_composition(snapshot, time(2));
  for (const auto& layer : ended.layers) assert(std::holds_alternative<TimelineEnded>(layer.source));
  snapshot.assets.clear();
  const auto missing = application::resolve_visual_composition(snapshot, boundary);
  assert(std::holds_alternative<application::MissingClipMedia>(missing.layers[1].source));
  assert(std::holds_alternative<application::MissingClipMedia>(missing.layers[2].source));
  bool rejected = false;
  try { (void)application::resolve_visual_composition(snapshot, time(-1)); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
  snapshot.visual_track_ids.pop_back();
  rejected = false;
  try { (void)application::resolve_visual_composition(snapshot, time(0)); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void empty_end_and_invalid_inputs() {
  ProjectDocument document;
  const auto snapshot = document.snapshot();
  assert(std::holds_alternative<TimelineEnded>(application::resolve_clip_at(
      snapshot, snapshot.video_track_id, time(0))));
  assert(std::holds_alternative<TimelineEnded>(application::resolve_clip_at(
      snapshot, snapshot.video_track_id, time(1))));

  bool negative_threw = false;
  try {
    (void)application::resolve_clip_at(snapshot, snapshot.video_track_id, time(-1));
  } catch (const std::invalid_argument&) {
    negative_threw = true;
  }
  assert(negative_threw);

  bool missing_track_threw = false;
  try {
    (void)application::resolve_clip_at(snapshot, domain::TrackId{99}, time(0));
  } catch (const std::invalid_argument&) {
    missing_track_threw = true;
  }
  assert(missing_track_threw);
}

void exact_mapping_and_boundaries_are_preserved() {
  Fixture fixture;
  const auto clip = fixture.processor.add_clip(
      fixture.media, TimeRange{time(7), time(20)}, time(10));
  fixture.processor.trim_clip_start(clip, time(13));
  const auto snapshot = fixture.document.snapshot();
  const auto resolved = resolve_clip(snapshot, time(14));
  assert(resolved.clip_id == clip);
  assert(resolved.media_id == fixture.media);
  assert(resolved.timeline_position == time(14));
  assert(same_range(resolved.timeline_range, TimeRange{time(13), time(23)}));
  assert(same_range(resolved.source_range, TimeRange{time(10), time(20)}));
  assert(resolved.clip_offset == time(1));
  assert(resolved.source_time == time(11));
  assert(resolved.video_stream_index == 2);
  assert(resolved.source_locator == std::filesystem::path{"media/test.mp4"});
}

void split_left_and_right_mapping() {
  Fixture fixture;
  const auto original = fixture.processor.add_clip(
      fixture.media, TimeRange{time(5), time(15)}, time(0));
  const auto right = fixture.processor.split_clip(original, time(4));
  const auto snapshot = fixture.document.snapshot();
  const auto left = resolve_clip(snapshot, time(3));
  const auto right_result = resolve_clip(snapshot, time(4));
  assert(left.clip_id == original && left.source_time == time(8));
  assert(right_result.clip_id == right && right_result.source_time == time(9));
  assert(same_range(left.timeline_range, TimeRange{time(0), time(4)}));
  assert(same_range(right_result.timeline_range, TimeRange{time(4), time(10)}));

  assert(application::clip_relative_position(left, time(3)) == time(3));
  assert(application::clip_source_position(left, time(3)) == time(8));
  assert(application::clip_relative_position(right_result, time(4)) == time(0));
  assert(application::clip_source_position(right_result, time(4)) == time(9));
  assert(application::audio_source_position(right_result, time(4), time(477, Rational{1, 1000})) ==
         time(9477, Rational{1, 1000}));

  bool boundary_rejected = false;
  try {
    (void)application::clip_source_position(right_result, time(10));
  } catch (const std::invalid_argument&) {
    boundary_rejected = true;
  }
  assert(boundary_rejected);
}

void mixed_time_bases_are_exact() {
  Fixture fixture;
  const auto clip = fixture.processor.add_clip(
      fixture.media,
      TimeRange{time(9000, Rational{1, 90000}), time(27000, Rational{1, 90000})},
      time(1000, Rational{1, 1000}));
  const auto snapshot = fixture.document.snapshot();
  const auto resolved = resolve_clip(snapshot, time(1050, Rational{1, 1000}));
  assert(resolved.clip_id == clip);
  assert(resolved.clip_offset == time(50, Rational{1, 1000}));
  assert(resolved.source_time == time(13500, Rational{1, 90000}));
  assert(application::clip_source_position(resolved, time(1050, Rational{1, 1000})) ==
         time(13500, Rational{1, 90000}));
}

void unavailable_and_absent_assets_are_distinct_from_gaps() {
  Fixture fixture;
  const auto clip = fixture.processor.add_clip(fixture.media, TimeRange{time(0), time(5)}, time(0));
  auto unavailable = fixture.document.snapshot();
  const auto& original = unavailable.assets.front();
  unavailable.assets.front() = application::MediaAsset{
      original.id(), original.source_locator(), original.video_stream_index(),
      original.playable_duration(), original.metadata(), application::MediaAvailability::Unavailable,
      "offline"};
  const auto missing = application::resolve_clip_at(unavailable, unavailable.video_track_id, time(1));
  assert(std::holds_alternative<application::MissingClipMedia>(missing));
  const auto missing_value = std::get<application::MissingClipMedia>(missing);
  assert(missing_value.clip_id == clip && missing_value.reason.has_value() &&
         *missing_value.reason == "offline");
  auto absent = fixture.document.snapshot();
  absent.assets.clear();
  const auto absent_value = application::resolve_clip_at(absent, absent.video_track_id, time(1));
  assert(std::holds_alternative<application::MissingClipMedia>(absent_value));
  assert(!std::get<application::MissingClipMedia>(absent_value).reason.has_value());
}

void ordering_same_asset_and_snapshot_purity() {
  Fixture fixture;
  const auto late = fixture.processor.add_clip(fixture.media, TimeRange{time(0), time(2)}, time(10));
  const auto early = fixture.processor.add_clip(fixture.media, TimeRange{time(2), time(4)}, time(0));
  const auto old_snapshot = fixture.document.snapshot();
  assert(resolve_clip(old_snapshot, time(1)).clip_id == early);
  assert(resolve_clip(old_snapshot, time(11)).clip_id == late);
  (void)fixture.processor.add_clip(fixture.media, TimeRange{time(4), time(6)}, time(20));
  assert(std::holds_alternative<TimelineEnded>(application::resolve_clip_at(
      old_snapshot, old_snapshot.video_track_id, time(20))));
  assert(old_snapshot.assets.size() == 1);
}

} // namespace

int main() {
  visual_composition_order_gaps_and_boundaries();
  boundaries_and_gaps();
  empty_end_and_invalid_inputs();
  exact_mapping_and_boundaries_are_preserved();
  split_left_and_right_mapping();
  mixed_time_bases_are_exact();
  unavailable_and_absent_assets_are_distinct_from_gaps();
  ordering_same_asset_and_snapshot_purity();
}
