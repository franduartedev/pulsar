#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/playback/timeline_playback_coordinator.hpp>
#include <pulsar/application/media/media_probe.hpp>

#include <cassert>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace pulsar;
using application::PlaybackGeneration;
using application::PlaybackWorkerEvent;
using application::PlaybackWorkerSeekComplete;
using application::PlaybackWorkerFatalError;
using application::PlaybackWorkerSample;
using application::PlaybackWorkerFactory;
using application::ProjectDocument;
using application::ProjectSnapshot;
using application::TimelineEndedPresentation;
using application::TimelineGapPresentation;
using application::TimelineMissingMediaPresentation;
using application::TimelinePlaybackCoordinator;
using application::TimelinePresentSample;
using application::TimelineHoldPresentation;
using application::TimelineSourceEndedEarlyPresentation;
using application::TimelineSourceErrorPresentation;
using application::TimelineStarvedPresentation;
using application::VideoSample;
using domain::MediaId;
using domain::MediaTime;
using domain::Rational;
using domain::TimeRange;

MediaTime time(std::int64_t value, Rational base = Rational{1, 1}) {
  return MediaTime{value, base};
}

class FakeClock final : public application::MonotonicClock {
public:
  application::MonotonicInstant now() const override { return application::MonotonicInstant{value}; }
  void advance(MediaTime amount) { value = value + amount; }
  MediaTime value{MediaTime::zero()};
};

class FakeProbe final : public application::MediaProbe {
public:
  application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    application::MediaProbeResult result;
    result.path = path;
    result.video_streams.push_back(application::VideoStreamInfo{
        stream_index, "fake", 1920, 1080, Rational{1, 90000}, time(100000, Rational{1, 1000}),
        std::nullopt});
    return result;
  }
  int stream_index{2};
};

struct WorkerConfig {
  bool emit_seek{true};
  bool emit_end_on_seek{false};
  bool emit_error_on_seek{false};
  bool emit_fatal_on_seek{false};
};

class FakeWorker final : public application::PlaybackMediaWorker {
public:
  FakeWorker(std::shared_ptr<WorkerConfig> config, std::vector<MediaTime>* requested_targets)
      : config_{std::move(config)}, requested_targets_{requested_targets} {}

  application::VideoSourceDescriptor describe() const override {
    return application::VideoSourceDescriptor{
        application::StreamTimestamp{time(5, Rational{1, 1})}, Rational{1, 90000},
        time(100, Rational{1, 1}), true};
  }

  void request_read(PlaybackGeneration generation) override {
    events_.push_back(PlaybackWorkerSample{
        generation,
        VideoSample{next_ordinal_++, application::StreamTimestamp{last_target_ + time(1, Rational{1, 1000})},
                    time(1, Rational{1, 1000})}});
  }

  void request_seek_for_presentation(PlaybackGeneration generation,
                                     application::StreamTimestamp target) override {
    requested_targets_->push_back(target.value());
    last_target_ = target.value();
    if (!config_->emit_seek) return;
    if (config_->emit_fatal_on_seek) {
      events_.push_back(application::PlaybackWorkerFatalError{application::VideoSourceError{"fatal worker failure"}});
      return;
    }
    if (config_->emit_error_on_seek) {
      events_.push_back(application::PlaybackWorkerSeekComplete{
          generation, application::VideoSourceError{"fake source error"}});
      return;
    }
    if (config_->emit_end_on_seek) {
      events_.push_back(application::PlaybackWorkerSeekComplete{
          generation, application::VideoSeekEndOfStream{
              std::nullopt, std::optional<application::StreamTimestamp>{target}, true}});
      return;
    }
    events_.push_back(application::PlaybackWorkerSeekComplete{
        generation, application::VideoSeekHit{
            VideoSample{next_ordinal_++, target, time(10, Rational{1, 1})}, std::nullopt}});
  }

  void cancel_pending() override { cancelled_ = true; }

  std::optional<PlaybackWorkerEvent> poll_event() override {
    if (events_.empty()) return std::nullopt;
    auto event = std::move(events_.front());
    events_.pop_front();
    return event;
  }

  bool cancelled_{false};
private:
  std::shared_ptr<WorkerConfig> config_;
  std::vector<MediaTime>* requested_targets_;
  std::deque<PlaybackWorkerEvent> events_;
  MediaTime last_target_{MediaTime::zero()};
  std::uint64_t next_ordinal_{1};
};

struct Fixture {
  ProjectDocument document;
  application::CommandProcessor processor;
  FakeProbe probe;
  MediaId media_one{MediaId{1}};
  MediaId media_two{MediaId{2}};
  std::shared_ptr<WorkerConfig> config{std::make_shared<WorkerConfig>()};
  std::vector<MediaTime> requested_targets;
  int factory_calls{0};

  Fixture() : processor{document} {
    media_one = processor.import_asset("media/one.mp4", probe);
    media_two = processor.import_asset("media/two.mp4", probe);
  }

  PlaybackWorkerFactory factory() {
    return [this](const std::filesystem::path&, int) {
      ++factory_calls;
      return std::make_unique<FakeWorker>(config, &requested_targets);
    };
  }
};

ProjectSnapshot snapshot_with_clip(Fixture& fixture, MediaId media, TimeRange source, MediaTime start) {
  (void)fixture.processor.add_clip(media, source, start);
  return fixture.document.snapshot();
}

void one_clock_play_pause_resume_and_origin_mapping() {
  Fixture fixture;
  const auto snapshot = snapshot_with_clip(fixture, fixture.media_one, TimeRange{time(0), time(10)}, time(0));
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(snapshot);
  auto presentation = coordinator.update();
  assert(std::holds_alternative<TimelinePresentSample>(presentation));
  coordinator.play();
  clock.advance(time(2));
  presentation = coordinator.update();
  assert(std::holds_alternative<TimelinePresentSample>(presentation) ||
         std::holds_alternative<TimelineHoldPresentation>(presentation));
  assert(coordinator.timeline_position() == time(2));
  coordinator.pause();
  const auto paused = coordinator.timeline_position();
  clock.advance(time(3));
  assert(coordinator.timeline_position() == paused);
  coordinator.play();
  clock.advance(time(1));
  assert(coordinator.timeline_position() == time(3));
  assert(fixture.requested_targets.front() == time(5));
}

void gaps_and_missing_continue_clock() {
  Fixture fixture;
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(5));
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(15));
  auto snapshot = fixture.document.snapshot();
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(snapshot);
  coordinator.seek(time(12));
  auto presentation = coordinator.update();
  assert(std::holds_alternative<TimelineGapPresentation>(presentation));
  coordinator.play();
  clock.advance(time(2));
  assert(coordinator.timeline_position() == time(14));
  snapshot.assets.front() = application::MediaAsset{
      snapshot.assets.front().id(), snapshot.assets.front().source_locator(),
      snapshot.assets.front().video_stream_index(), snapshot.assets.front().playable_duration(),
      snapshot.assets.front().metadata(), application::MediaAvailability::Unavailable, "offline"};
  coordinator.set_snapshot(snapshot);
  coordinator.seek(time(6));
  presentation = coordinator.update();
  assert(std::holds_alternative<TimelineMissingMediaPresentation>(presentation));
  coordinator.play();
  clock.advance(time(1));
  assert(coordinator.timeline_position() == time(7));
}

void contiguous_same_and_different_media_switches() {
  Fixture fixture;
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(0));
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(5), time(10)}, time(5));
  const auto snapshot = fixture.document.snapshot();
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(snapshot);
  (void)coordinator.update();
  (void)coordinator.update();
  coordinator.play();
  clock.advance(time(5));
  auto presentation = coordinator.update();
  assert(std::holds_alternative<TimelinePresentSample>(presentation));
  assert(fixture.factory_calls == 1);
  assert(fixture.requested_targets.size() >= 2);

  Fixture different;
  (void)different.processor.add_clip(different.media_one, TimeRange{time(0), time(5)}, time(0));
  (void)different.processor.add_clip(different.media_two, TimeRange{time(0), time(5)}, time(5));
  FakeClock other_clock;
  TimelinePlaybackCoordinator other{other_clock, different.factory()};
  other.set_snapshot(different.document.snapshot());
  (void)other.update();
  (void)other.update();
  other.play();
  other_clock.advance(time(5));
  (void)other.update();
  assert(different.factory_calls == 2);
}

void seek_paths_and_ended_are_stable() {
  Fixture fixture;
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(0));
  const auto snapshot = fixture.document.snapshot();
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(snapshot);
  coordinator.seek(time(5));
  auto presentation = coordinator.update();
  assert(std::holds_alternative<TimelineEndedPresentation>(presentation));
  coordinator.play();
  clock.advance(time(1));
  assert(std::holds_alternative<TimelineEndedPresentation>(coordinator.update()));
  coordinator.seek(time(1));
  assert(std::holds_alternative<TimelinePresentSample>(coordinator.update()));
}

void preparing_starvation_and_runtime_errors() {
  Fixture fixture;
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(0));
  auto snapshot = fixture.document.snapshot();
  FakeClock clock;
  fixture.config->emit_seek = false;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(snapshot);
  coordinator.play();
  assert(std::holds_alternative<TimelineStarvedPresentation>(coordinator.update()));
  clock.advance(time(2));
  assert(coordinator.timeline_position() == time(0));

  Fixture early;
  (void)early.processor.add_clip(early.media_one, TimeRange{time(0), time(5)}, time(0));
  early.config->emit_end_on_seek = true;
  FakeClock early_clock;
  TimelinePlaybackCoordinator early_coordinator{early_clock, early.factory()};
  early_coordinator.set_snapshot(early.document.snapshot());
  assert(std::holds_alternative<TimelineSourceEndedEarlyPresentation>(early_coordinator.update()));

  Fixture error;
  (void)error.processor.add_clip(error.media_one, TimeRange{time(0), time(5)}, time(0));
  error.config->emit_error_on_seek = true;
  FakeClock error_clock;
  TimelinePlaybackCoordinator error_coordinator{error_clock, error.factory()};
  error_coordinator.set_snapshot(error.document.snapshot());
  assert(std::holds_alternative<TimelineSourceErrorPresentation>(error_coordinator.update()));
}

void exact_awkward_source_mapping() {
  Fixture fixture;
  const MediaTime source_start{1001, Rational{1, 30000}};
  const MediaTime source_end{2002, Rational{1, 30000}};
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{source_start, source_end}, time(0));
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(fixture.document.snapshot());
  (void)coordinator.update();
  assert(!fixture.requested_targets.empty());
  assert(fixture.requested_targets.front() == time(5) + source_start);
}

void snapshot_replacement_invalidates_old_session() {
  Fixture fixture;
  const auto first = snapshot_with_clip(fixture, fixture.media_one, TimeRange{time(0), time(5)}, time(0));
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(first);
  (void)coordinator.update();
  (void)coordinator.update();
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(10));
  const auto second = fixture.document.snapshot();
  coordinator.set_snapshot(second);
  const auto presentation = coordinator.update();
  assert(std::holds_alternative<TimelinePresentSample>(presentation) ||
         std::holds_alternative<TimelineStarvedPresentation>(presentation));
  assert(fixture.factory_calls == 2);
}

void canonical_end_position_and_snapshot_shrink() {
  Fixture fixture;
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(0));
  const auto long_snapshot = fixture.document.snapshot();
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(long_snapshot);
  coordinator.seek(time(20));
  auto presentation = coordinator.update();
  assert(coordinator.timeline_position() == time(5));
  assert(std::holds_alternative<TimelineEndedPresentation>(presentation));
  const auto ended = std::get<TimelineEndedPresentation>(presentation);
  assert(ended.position == time(5));
  assert(ended.duration == time(5));

  Fixture replacement;
  (void)replacement.processor.add_clip(replacement.media_one, TimeRange{time(0), time(20)}, time(0));
  const auto longer = replacement.document.snapshot();
  TimelinePlaybackCoordinator shrinking{clock, replacement.factory()};
  shrinking.set_snapshot(longer);
  shrinking.seek(time(8));
  Fixture short_fixture;
  (void)short_fixture.processor.add_clip(short_fixture.media_one, TimeRange{time(0), time(5)}, time(0));
  const auto shorter = short_fixture.document.snapshot();
  shrinking.set_snapshot(shorter);
  presentation = shrinking.update();
  assert(shrinking.timeline_position() == shorter.timeline.duration());
  assert(std::holds_alternative<TimelineEndedPresentation>(presentation));
  const auto shrunk_end = std::get<TimelineEndedPresentation>(presentation);
  assert(shrunk_end.position == shorter.timeline.duration());
  assert(shrunk_end.duration == shorter.timeline.duration());
}

void rapid_same_source_seek_uses_latest_completion() {
  Fixture fixture;
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(10)}, time(0));
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(fixture.document.snapshot());
  coordinator.seek(time(2));
  coordinator.seek(time(7));
  const auto presentation = coordinator.update();
  assert(std::holds_alternative<TimelinePresentSample>(presentation) ||
         std::holds_alternative<TimelineHoldPresentation>(presentation));
  assert(!fixture.requested_targets.empty());
  assert(fixture.requested_targets.back() == time(7) + time(5));
}

void snapshot_change_resets_active_clip_identity() {
  Fixture fixture;
  const domain::ClipId first_id =
      fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(10)}, time(0));
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(fixture.document.snapshot());
  auto presentation = coordinator.update();
  assert(std::holds_alternative<TimelinePresentSample>(presentation));
  assert(std::get<TimelinePresentSample>(presentation).clip_id == first_id);
  assert(fixture.factory_calls == 1);

  // Removing and re-adding consumes a fresh ClipId for the same media (no ID reuse).
  fixture.processor.remove_clip(first_id);
  const domain::ClipId second_id =
      fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(10)}, time(0));
  assert(second_id != first_id);
  coordinator.set_snapshot(fixture.document.snapshot());
  presentation = coordinator.update();
  // The new snapshot must establish the new clip identity with a fresh session:
  // the stale clip must neither be presented nor have its session reused.
  assert(std::holds_alternative<TimelinePresentSample>(presentation));
  assert(std::get<TimelinePresentSample>(presentation).clip_id == second_id);
  assert(fixture.factory_calls == 2);
}


void fatal_worker_error_propagates() {
  Fixture fixture;
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(0));
  fixture.config->emit_fatal_on_seek = true;
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(fixture.document.snapshot());
  const auto presentation = coordinator.update();
  assert(std::holds_alternative<TimelineSourceErrorPresentation>(presentation));
  assert(!std::holds_alternative<TimelineStarvedPresentation>(presentation));
}

} // namespace

int main() {
  one_clock_play_pause_resume_and_origin_mapping();
  gaps_and_missing_continue_clock();
  contiguous_same_and_different_media_switches();
  seek_paths_and_ended_are_stable();
  preparing_starvation_and_runtime_errors();
  exact_awkward_source_mapping();
  snapshot_replacement_invalidates_old_session();
  canonical_end_position_and_snapshot_shrink();
  rapid_same_source_seek_uses_latest_completion();
  snapshot_change_resets_active_clip_identity();
  fatal_worker_error_propagates();
}
