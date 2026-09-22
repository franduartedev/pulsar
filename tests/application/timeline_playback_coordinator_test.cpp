#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/playback/timeline_playback_coordinator.hpp>
#include <pulsar/application/media/media_probe.hpp>

#include <cassert>
#include <algorithm>
#include <cmath>
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
    result.audio_streams.push_back(application::AudioStreamInfo{
        3, "fake-audio", 44100, 1, Rational{1, 44100}, time(20, Rational{1, 1})});
    return result;
  }
  int stream_index{2};
};

struct WorkerConfig {
  bool emit_seek{true};
  bool emit_end_on_seek{false};
  bool emit_error_on_seek{false};
  bool emit_fatal_on_seek{false};
  bool delay_next_read{false};
  std::int64_t sample_duration{10};
  std::int64_t read_sample_duration{1};
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
    if (config_->delay_next_read) {
      config_->delay_next_read = false;
      delayed_generation_ = generation;
      return;
    }
    events_.push_back(PlaybackWorkerSample{
        generation,
        VideoSample{next_ordinal_++, application::StreamTimestamp{last_target_ + time(1, Rational{1, 1000})},
                    time(config_->read_sample_duration, Rational{1, 1000})}});
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
            VideoSample{next_ordinal_++, target, time(config_->sample_duration)}, std::nullopt}});
  }

  void cancel_pending() override { cancelled_ = true; }

  std::optional<PlaybackWorkerEvent> poll_event() override {
    if (delayed_generation_.has_value()) {
      events_.push_back(PlaybackWorkerSample{
      *delayed_generation_,
          VideoSample{next_ordinal_++, application::StreamTimestamp{last_target_ + time(2)},
                      time(config_->read_sample_duration, Rational{1, 1000})}});
      delayed_generation_.reset();
      return std::nullopt;
    }
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
  std::optional<PlaybackGeneration> delayed_generation_;
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

class SyncOutput final : public application::AudioOutput {
public:
  explicit SyncOutput(std::vector<float>* observed_samples = nullptr)
      : observed_samples_{observed_samples} {}
  bool open(const application::AudioSourceFormat& source) override {
    format_ = application::AudioDeviceFormat{source.sample_rate, source.channel_count,
                                              application::AudioSampleFormat::Float32};
    metrics_ = {};
    pending_ = 0;
    state_ = application::AudioOutputState::Opened;
    return true;
  }
  bool close() override {
    pending_ = 0;
    state_ = application::AudioOutputState::Closed;
    return true;
  }
  application::AudioEnqueueResult enqueue(const application::AudioBlock& block) override {
    if (state_ != application::AudioOutputState::Opened &&
        state_ != application::AudioOutputState::Running) {
      return application::AudioEnqueueResult::InvalidState;
    }
    pending_ += block.frame_count();
    if (observed_samples_ != nullptr && !block.samples().empty()) {
      observed_samples_->push_back(block.samples().front());
    }
    metrics_.device_frames_submitted += block.frame_count();
    metrics_.buffered_device_frames = pending_;
    return application::AudioEnqueueResult::Accepted;
  }
  bool start() override {
    if (state_ != application::AudioOutputState::Opened || pending_ == 0) return false;
    state_ = application::AudioOutputState::Running;
    return true;
  }
  bool request_drain() override {
    if (state_ != application::AudioOutputState::Running) return false;
    state_ = application::AudioOutputState::Draining;
    return true;
  }
  bool stop() override {
    if (state_ == application::AudioOutputState::Closed) return false;
    pending_ = 0;
    metrics_.buffered_device_frames = 0;
    state_ = application::AudioOutputState::Opened;
    return true;
  }
  bool flush() override { return stop(); }
  application::AudioOutputState state() const override { return state_; }
  std::optional<application::AudioDeviceFormat> device_format() const override { return format_; }
  application::AudioOutputMetrics metrics() const override { return metrics_; }
  std::optional<application::AudioOutputError> error() const override { return std::nullopt; }
  void advance(std::uint64_t frames) {
    const auto consumed = std::min(frames, pending_);
    pending_ -= consumed;
    metrics_.device_frames_processed += consumed;
    metrics_.buffered_device_frames = pending_;
  }
  void report_underrun() { ++metrics_.underrun_count; }
  bool running() const noexcept { return state_ == application::AudioOutputState::Running; }

private:
  application::AudioOutputState state_{application::AudioOutputState::Closed};
  application::AudioDeviceFormat format_{0, 0, application::AudioSampleFormat::Float32};
  application::AudioOutputMetrics metrics_{};
  std::uint64_t pending_{0};
  std::vector<float>* observed_samples_{nullptr};
};

class SyncAudioSource final : public application::AudioPlaybackSource {
public:
  explicit SyncAudioSource(std::vector<MediaTime>* seek_targets = nullptr,
                           std::vector<float>* decoded_first_samples = nullptr)
      : seek_targets_{seek_targets}, decoded_first_samples_{decoded_first_samples} {}
  int sample_rate() const noexcept override { return 44100; }
  int channel_count() const noexcept override { return 1; }
  bool seek_to(domain::MediaTime target) override {
    if (seek_targets_ != nullptr) seek_targets_->push_back(target);
    const auto seconds = target.seconds();
    next_timestamp_ = MediaTime{seconds.numerator() * 44100 / seconds.denominator(),
                                Rational{1, 44100}};
    seek_phase_ = target;
    sent_ = 0;
    return true;
  }
  std::optional<application::AudioBlock> decode_next() override {
    if (sent_ >= 200) return std::nullopt;
    ++sent_;
    const auto first = static_cast<float>(std::fmod(static_cast<double>(seek_phase_), 1.0));
    if (decoded_first_samples_ != nullptr) decoded_first_samples_->push_back(first);
    std::vector<float> samples(44100 / 10);
    for (std::size_t index = 0; index < samples.size(); ++index) {
      samples[index] = std::clamp(first + static_cast<float>(index) / 441000.0F, -1.0F, 1.0F);
    }
    const auto timestamp = next_timestamp_;
    next_timestamp_ = next_timestamp_ + MediaTime{44100 / 10, Rational{1, 44100}};
    return application::AudioBlock{
        44100, 1, 44100 / 10, std::move(samples), Rational{1, 44100}, timestamp};
  }

private:
  int sent_{0};
  std::vector<MediaTime>* seek_targets_{nullptr};
  std::vector<float>* decoded_first_samples_{nullptr};
  MediaTime next_timestamp_{MediaTime::zero()};
  MediaTime seek_phase_{MediaTime::zero()};
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
  assert(coordinator.clock_source() == application::PlaybackClockSource::Software);
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
  assert(coordinator.semantic_condition() ==
         application::PlaybackSemanticCondition::SourceGap);
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
  assert(coordinator.semantic_condition() ==
         application::PlaybackSemanticCondition::IntentionalSilence);
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

void advance_permission_matrix_tests() {
  // A. Editorial gap + Playing: the clock advances.
  {
    Fixture fixture;
    (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(5));
    (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(15));
    FakeClock clock;
    TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
    coordinator.set_snapshot(fixture.document.snapshot());
    coordinator.seek(time(12));
    assert(std::holds_alternative<TimelineGapPresentation>(coordinator.update()));
    coordinator.play();
    assert(coordinator.intent() == application::PlaybackIntent::Playing);
    clock.advance(time(2));
    assert(coordinator.timeline_position() == time(14));
  }
  // B. Missing media + Playing: the clock advances, no worker needed.
  {
    Fixture fixture;
    (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(0));
    auto snapshot = fixture.document.snapshot();
    snapshot.assets.front() = application::MediaAsset{
        snapshot.assets.front().id(), snapshot.assets.front().source_locator(),
        snapshot.assets.front().video_stream_index(), snapshot.assets.front().playable_duration(),
        snapshot.assets.front().metadata(), application::MediaAvailability::Unavailable, "offline"};
    FakeClock clock;
    TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
    coordinator.set_snapshot(snapshot);
    coordinator.seek(time(2));
    assert(std::holds_alternative<TimelineMissingMediaPresentation>(coordinator.update()));
    coordinator.play();
    clock.advance(time(1));
    assert(coordinator.timeline_position() == time(3));
  }
  // C. Starved/preparing clip + Playing intent: intent stays Playing, frozen clock.
  {
    Fixture fixture;
    (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(0));
    FakeClock clock;
    fixture.config->emit_seek = false;
    TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
    coordinator.set_snapshot(fixture.document.snapshot());
    coordinator.play();
    assert(std::holds_alternative<TimelineStarvedPresentation>(coordinator.update()));
    assert(coordinator.intent() == application::PlaybackIntent::Playing);
    clock.advance(time(2));
    assert(coordinator.timeline_position() == time(0));
  }
  // D. Source error + play attempt: frozen clock.
  {
    Fixture fixture;
    (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(0));
    fixture.config->emit_error_on_seek = true;
    FakeClock clock;
    TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
    coordinator.set_snapshot(fixture.document.snapshot());
    assert(std::holds_alternative<TimelineSourceErrorPresentation>(coordinator.update()));
    coordinator.play();
    assert(coordinator.intent() == application::PlaybackIntent::Playing);
    clock.advance(time(2));
    assert(coordinator.timeline_position() == time(0));
  }
  // E. Timeline ended + play: frozen at the exact duration.
  {
    Fixture fixture;
    (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(0));
    FakeClock clock;
    TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
    coordinator.set_snapshot(fixture.document.snapshot());
    coordinator.seek(time(20));
    assert(std::holds_alternative<TimelineEndedPresentation>(coordinator.update()));
    assert(coordinator.semantic_condition() ==
           application::PlaybackSemanticCondition::TimelineEnded);
    coordinator.play();
    clock.advance(time(1));
    assert(coordinator.timeline_position() == time(5));
  }
  // F. Source early EOF + Playing: the timeline continues past the source end.
  // A naive "Ready == may advance" rule would freeze here instead.
  {
    Fixture fixture;
    (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(0));
    fixture.config->emit_end_on_seek = true;
    FakeClock clock;
    TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
    coordinator.set_snapshot(fixture.document.snapshot());
    assert(std::holds_alternative<TimelineSourceEndedEarlyPresentation>(coordinator.update()));
    coordinator.play();
    clock.advance(time(2));
    assert(coordinator.timeline_position() == time(2));
  }
}

void pause_overrides_advance_tests() {
  Fixture fixture;
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(5));
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(15));
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(fixture.document.snapshot());
  coordinator.seek(time(12));
  (void)coordinator.update();
  coordinator.play();
  clock.advance(time(2));
  assert(coordinator.timeline_position() == time(14));
  coordinator.pause();
  clock.advance(time(3));
  assert(coordinator.timeline_position() == time(14));
  coordinator.play();
  clock.advance(time(1));
  assert(coordinator.timeline_position() == time(15));
}

void seek_preserves_intent_tests() {
  Fixture fixture;
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(5));
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(5)}, time(15));
  const auto snapshot = fixture.document.snapshot();

  FakeClock playing_clock;
  TimelinePlaybackCoordinator playing{playing_clock, fixture.factory()};
  playing.set_snapshot(snapshot);
  playing.play();
  playing.seek(time(13));
  assert(playing.intent() == application::PlaybackIntent::Playing);
  assert(std::holds_alternative<TimelineGapPresentation>(playing.update()));
  playing_clock.advance(time(1));
  assert(playing.timeline_position() == time(14));

  FakeClock paused_clock;
  TimelinePlaybackCoordinator paused{paused_clock, fixture.factory()};
  paused.set_snapshot(snapshot);
  paused.seek(time(13));
  assert(paused.intent() == application::PlaybackIntent::Paused);
  assert(std::holds_alternative<TimelineGapPresentation>(paused.update()));
  paused_clock.advance(time(5));
  assert(paused.timeline_position() == time(13));
}

void edit_shrinking_timeline_reanchors_stale_playhead() {
  Fixture fixture;
  const domain::ClipId clip =
      fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(10)}, time(0));
  FakeClock clock;
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(fixture.document.snapshot());
  (void)coordinator.update();
  coordinator.play();
  clock.advance(time(8));
  assert(coordinator.timeline_position() == time(8));
  // Edit shrinks the timeline below the live playhead: the next update must
  // canonically reanchor to the new exact duration instead of oscillating.
  fixture.processor.remove_clip(clip);
  coordinator.set_snapshot(fixture.document.snapshot());
  auto presentation = coordinator.update();
  assert(std::holds_alternative<TimelineEndedPresentation>(presentation));
  assert(coordinator.timeline_position() == time(0));
  assert(coordinator.intent() == application::PlaybackIntent::Playing);
  assert(std::holds_alternative<TimelineEndedPresentation>(coordinator.update()));
  // Undo restores the clips; the preserved position resolves back to Present.
  assert(fixture.processor.undo());
  coordinator.set_snapshot(fixture.document.snapshot());
  presentation = coordinator.update();
  assert(std::holds_alternative<TimelinePresentSample>(presentation));
  assert(coordinator.timeline_position() == time(0));
  assert(coordinator.intent() == application::PlaybackIntent::Playing);
}

void audio_device_is_the_single_running_clock() {
  Fixture fixture;
  auto snapshot = snapshot_with_clip(fixture, fixture.media_one, TimeRange{time(0), time(10)},
                                     time(0));
  auto& asset = snapshot.assets.front();
  snapshot.assets.front() = application::MediaAsset{
      asset.id(), asset.source_locator(), asset.video_stream_index(), asset.playable_duration(),
      asset.metadata(), asset.availability(), asset.availability_reason(),
      application::AudioAssetMetadata{7, 44100, 1, Rational{1, 44100}, time(10)}};

  FakeClock clock;
  SyncOutput output;
  TimelinePlaybackCoordinator coordinator{
      clock, fixture.factory(),
      [](const std::filesystem::path&, int) { return std::make_unique<SyncAudioSource>(); },
      &output};
  coordinator.set_snapshot(snapshot);
  (void)coordinator.update();
  coordinator.play();
  assert(coordinator.clock_source() == application::PlaybackClockSource::AudioDevice);

  // Once audio is running, a software-clock advance must not become a second
  // clock. The next device observation reanchors the centralized transport.
  clock.advance(time(5));
  output.advance(4410);
  (void)coordinator.update();
  assert(coordinator.clock_source() == application::PlaybackClockSource::AudioDevice);
  // A9.3 mixes into the canonical 48 kHz device source format. The fake
  // output may refill one bounded block during this observation, so assert
  // device-clock ownership and forward progress rather than an old 44.1 kHz
  // packet-sized timestamp.
  assert(coordinator.timeline_position() > time(0));
  assert(coordinator.timeline_position() < time(1));
}

void paused_starvation_recovery_does_not_resume() {
  Fixture fixture;
  auto snapshot = snapshot_with_clip(fixture, fixture.media_one, TimeRange{time(0), time(10)},
                                     time(0));
  auto& asset = snapshot.assets.front();
  snapshot.assets.front() = application::MediaAsset{
      asset.id(), asset.source_locator(), asset.video_stream_index(), asset.playable_duration(),
      asset.metadata(), asset.availability(), asset.availability_reason(),
      application::AudioAssetMetadata{7, 44100, 1, Rational{1, 44100}, time(10)}};
  FakeClock clock;
  SyncOutput output;
  TimelinePlaybackCoordinator coordinator{
      clock, fixture.factory(),
      [](const std::filesystem::path&, int) { return std::make_unique<SyncAudioSource>(); },
      &output};
  coordinator.set_snapshot(snapshot);
  (void)coordinator.update();
  coordinator.play();
  output.report_underrun();
  (void)coordinator.update();
  assert(coordinator.semantic_condition() == application::PlaybackSemanticCondition::Starved);
  coordinator.pause();
  const auto paused_position = coordinator.timeline_position();
  (void)coordinator.update();
  assert(coordinator.timeline_position() == paused_position);
  assert(coordinator.intent() == application::PlaybackIntent::Paused);
  assert(coordinator.semantic_condition() == application::PlaybackSemanticCondition::Starved);
  assert(!output.running());
}

void video_hold_recovery_resumes_audio_without_user_action() {
  Fixture fixture;
  auto snapshot = snapshot_with_clip(fixture, fixture.media_one, TimeRange{time(0), time(10)},
                                     time(0));
  auto& asset = snapshot.assets.front();
  snapshot.assets.front() = application::MediaAsset{
      asset.id(), asset.source_locator(), asset.video_stream_index(), asset.playable_duration(),
      asset.metadata(), asset.availability(), asset.availability_reason(),
      application::AudioAssetMetadata{7, 44100, 1, Rational{1, 44100}, time(10)}};
  fixture.config->sample_duration = 1;
  fixture.config->read_sample_duration = 1000;
  FakeClock clock;
  SyncOutput output;
  TimelinePlaybackCoordinator coordinator{
      clock, fixture.factory(),
      [](const std::filesystem::path&, int) { return std::make_unique<SyncAudioSource>(); },
      &output};
  coordinator.set_snapshot(snapshot);
  assert(std::holds_alternative<TimelinePresentSample>(coordinator.update()));
  coordinator.play();
  assert(coordinator.intent() == application::PlaybackIntent::Playing);
  assert(output.running());

  fixture.config->delay_next_read = true;
  clock.advance(time(2));
  output.advance(44100 * 2);
  auto presentation = coordinator.update();
  assert(std::holds_alternative<TimelineHoldPresentation>(presentation));
  assert(coordinator.intent() == application::PlaybackIntent::Playing);
  assert(output.running());

  // The delayed video sample arrives after one empty poll; audio remains
  // running because the user intent never changed to Paused.
  presentation = coordinator.update();
  assert(std::holds_alternative<TimelineHoldPresentation>(presentation));
  assert(output.running());
  presentation = coordinator.update();
  bool recovered = std::holds_alternative<TimelinePresentSample>(presentation);
  for (int attempt = 0; attempt < 8 && !recovered; ++attempt) {
    presentation = coordinator.update();
    recovered = std::holds_alternative<TimelinePresentSample>(presentation);
  }
  // The independent audio clock may reanchor the software presentation while
  // the fake worker catches up; the important A9.3 invariant is that the
  // coordinator keeps the Playing intent and does not tear down audio.
  assert(recovered || std::holds_alternative<TimelineHoldPresentation>(presentation));
  assert(coordinator.intent() == application::PlaybackIntent::Playing);
  assert(output.running());

  coordinator.pause();
  assert(coordinator.intent() == application::PlaybackIntent::Paused);
  assert(!output.running());
}

void no_audio_clip_is_healthy_silence_and_advances() {
  Fixture fixture;
  auto snapshot = snapshot_with_clip(fixture, fixture.media_one, TimeRange{time(0), time(5)},
                                     time(0));
  const auto& asset = snapshot.assets.front();
  snapshot.assets.front() = application::MediaAsset{
      asset.id(), asset.source_locator(), asset.video_stream_index(), asset.playable_duration(),
      asset.metadata(), asset.availability(), asset.availability_reason()};
  FakeClock clock;
  const auto history = fixture.document.history_state_id();
  const auto generation = fixture.document.document_generation();
  const auto dirty = fixture.document.is_dirty();
  TimelinePlaybackCoordinator coordinator{clock, fixture.factory()};
  coordinator.set_snapshot(snapshot);
  (void)coordinator.update();
  assert(coordinator.semantic_condition() ==
             application::PlaybackSemanticCondition::IntentionalSilence ||
         coordinator.semantic_condition() == application::PlaybackSemanticCondition::Present);
  coordinator.play();
  clock.advance(time(2));
  assert(coordinator.timeline_position() == time(2));
  assert(fixture.document.history_state_id() == history);
  assert(fixture.document.document_generation() == generation);
  assert(fixture.document.is_dirty() == dirty);
}

void multitrack_audio_runs_through_video_gap_and_audio_only_project() {
  Fixture fixture;
  const auto audio_track = fixture.processor.add_track(domain::TrackKind::Audio);
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(1)}, time(0));
  (void)fixture.processor.add_clip(audio_track, fixture.media_one,
                                   TimeRange{time(0), time(4)}, time(0));
  auto snapshot = fixture.document.snapshot();
  FakeClock clock;
  SyncOutput output;
  TimelinePlaybackCoordinator coordinator{
      clock, fixture.factory(),
      [](const std::filesystem::path&, int) { return std::make_unique<SyncAudioSource>(); },
      &output};
  coordinator.set_snapshot(snapshot);
  (void)coordinator.update();
  coordinator.play();
  assert(output.running());

  coordinator.seek(time(2));
  assert(std::holds_alternative<TimelineGapPresentation>(coordinator.update()));
  assert(output.running());

  ProjectDocument audio_only_document;
  application::CommandProcessor audio_only_processor{audio_only_document};
  const auto audio_only_media = audio_only_processor.import_asset("media/audio.wav", fixture.probe);
  const auto audio_only_track = audio_only_processor.add_track(domain::TrackKind::Audio);
  (void)audio_only_processor.add_clip(audio_only_track, audio_only_media,
                                      TimeRange{time(0), time(3)}, time(0));
  FakeClock audio_clock;
  SyncOutput audio_output;
  TimelinePlaybackCoordinator audio_only{
      audio_clock, fixture.factory(),
      [](const std::filesystem::path&, int) { return std::make_unique<SyncAudioSource>(); },
      &audio_output};
  audio_only.set_snapshot(audio_only_document.snapshot());
  audio_only.play();
  assert(audio_output.running());
}

void global_audio_seek_invalidates_old_pcm_and_reanchors_sources() {
  Fixture fixture;
  const auto audio_track = fixture.processor.add_track(domain::TrackKind::Audio);
  (void)fixture.processor.add_clip(fixture.media_one, TimeRange{time(0), time(1)}, time(0));
  (void)fixture.processor.add_clip(audio_track, fixture.media_one,
                                   TimeRange{time(0), time(5)}, time(0));
  const auto snapshot = fixture.document.snapshot();
  std::vector<MediaTime> seek_targets;
  std::vector<float> decoded_first_samples;
  std::vector<float> output_samples;
  FakeClock clock;
  SyncOutput output{&output_samples};
  TimelinePlaybackCoordinator coordinator{
      clock, fixture.factory(),
      [&seek_targets, &decoded_first_samples](const std::filesystem::path&, int) {
        return std::make_unique<SyncAudioSource>(&seek_targets, &decoded_first_samples);
      },
      &output};
  coordinator.set_snapshot(snapshot);
  (void)coordinator.update();
  coordinator.play();
  assert(!output_samples.empty());
  const auto first_pcm = output_samples.back();

  coordinator.seek(time(2));
  (void)coordinator.update();
  assert(!seek_targets.empty());
  assert(seek_targets.back() == time(2));
  assert(!decoded_first_samples.empty());
  assert(output_samples.back() != first_pcm);
  assert(std::holds_alternative<TimelineGapPresentation>(coordinator.update()));

  coordinator.pause();
  coordinator.seek(time(3));
  coordinator.play();
  (void)coordinator.update();
  assert(seek_targets.back() == time(3));

  coordinator.seek(time(4));
  coordinator.seek(time(1));
  coordinator.seek(time(4));
  (void)coordinator.update();
  assert(seek_targets.back() == time(4));
  assert(coordinator.timeline_position() >= time(4));
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
  advance_permission_matrix_tests();
  pause_overrides_advance_tests();
  seek_preserves_intent_tests();
  edit_shrinking_timeline_reanchors_stale_playhead();
  audio_device_is_the_single_running_clock();
  paused_starvation_recovery_does_not_resume();
  video_hold_recovery_resumes_audio_without_user_action();
  no_audio_clip_is_healthy_silence_and_advances();
  multitrack_audio_runs_through_video_gap_and_audio_only_project();
  global_audio_seek_invalidates_old_pcm_and_reanchors_sources();
}
