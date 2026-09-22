#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/audio/clip_gain.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/playback/timeline_playback_coordinator.hpp>
#include <pulsar/application/project/project_codec.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/application/timeline/timeline_clip_resolver.hpp>

#include <cassert>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace pulsar;
using application::AudioAssetMetadata;
using application::CommandProcessor;
using application::MediaProbeResult;
using application::ProjectCodec;
using application::ProjectCodecError;
using application::ProjectDocument;
using application::ResolvedClip;
using domain::MediaTime;
using domain::Rational;
using domain::TimeRange;
using domain::TrackKind;

MediaTime time(std::int64_t value, Rational base = Rational{1, 1}) {
  return MediaTime{value, base};
}

class FakeProbe final : public application::MediaProbe {
public:
  FakeProbe(std::vector<application::AudioStreamInfo> audio_streams, int video_stream = 2)
      : audio_streams_{std::move(audio_streams)}, video_stream_{video_stream} {}

  [[nodiscard]] MediaProbeResult probe(const std::filesystem::path& path) const override {
    MediaProbeResult result;
    result.path = path;
    result.container_name = "test";
    result.video_streams.push_back(application::VideoStreamInfo{
        video_stream_, "h264", 1920, 1080, Rational{1, 90000},
        time(100000, Rational{1, 1000}), std::nullopt});
    result.audio_streams = audio_streams_;
    return result;
  }

private:
  std::vector<application::AudioStreamInfo> audio_streams_;
  int video_stream_;
};

application::AudioStreamInfo audio_stream(int index, int sample_rate, int channels);

class AudioOnlyProbe final : public application::MediaProbe {
public:
  [[nodiscard]] MediaProbeResult probe(const std::filesystem::path& path) const override {
    MediaProbeResult result;
    result.path = path;
    result.container_name = "wav";
    result.duration = time(4, Rational{1, 1});
    result.audio_streams.push_back(audio_stream(0, 48000, 1));
    result.audio_streams.back().duration = result.duration;
    return result;
  }
};

application::AudioStreamInfo audio_stream(int index, int sample_rate = 48000,
                                          int channels = 2) {
  return application::AudioStreamInfo{index, "aac", sample_rate, channels, Rational{1, 48000},
                                      time(96000, Rational{1, 48000})};
}

// T1: first audio stream wins deterministically, by real FFmpeg stream index.
void audio_selected_first_stream_deterministically() {
  ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe probe{{audio_stream(5), audio_stream(7, 44100, 1)}};
  const auto media = processor.import_asset("media/av.mp4", probe);
  const auto asset = document.find_asset(media);
  assert(asset.has_value());
  assert(asset->audio().has_value());
  assert(asset->audio()->stream_index == 5);
  assert(asset->audio()->sample_rate == 48000);
  assert(asset->audio()->channel_count == 2);
  assert((asset->audio()->time_base == Rational{1, 48000}));
  assert(asset->audio()->duration.has_value());
  assert(*asset->audio()->duration == time(96000, Rational{1, 48000}));
  // M9 duration semantics frozen: playable duration still comes from video.
  assert(asset->playable_duration() == time(100000, Rational{1, 1000}));
  assert(asset->video_stream_index() == 2);
}

// T2: video-only media imports normally with no audio binding.
void video_only_has_no_audio() {
  ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe probe{{}};
  const auto media = processor.import_asset("media/silent.mp4", probe);
  const auto asset = document.find_asset(media);
  assert(asset.has_value());
  assert(!asset->audio().has_value());
  const auto clip =
      processor.add_clip(media, TimeRange{time(0), time(10)}, time(0));
  assert(document.timeline().find_clip(clip) != nullptr);
}

void audio_only_ingest_has_no_video_metadata() {
  ProjectDocument document;
  CommandProcessor processor{document};
  const auto media = processor.import_asset("media/tone.wav", AudioOnlyProbe{});
  const auto asset = document.find_asset(media);
  assert(asset.has_value() && !asset->has_video());
  assert(asset->video_stream_index() < 0);
  assert(asset->audio().has_value() && asset->audio()->channel_count == 1);
  bool video_metadata_rejected = false;
  try {
    (void)asset->metadata();
  } catch (const std::logic_error&) {
    video_metadata_rejected = true;
  }
  assert(video_metadata_rejected);
  const auto audio_track = processor.add_track(TrackKind::Audio);
  const auto clip = processor.add_clip(audio_track, media, TimeRange{time(0), time(2)}, time(0));
  const auto bytes = ProjectCodec::encode(document.snapshot());
  assert(bytes.find("AUDIO_ONLY") != std::string::npos);
  const auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  assert(!restored.find_asset(media)->has_video());
  assert(restored.timeline().find_clip(clip) != nullptr);
  assert(ProjectCodec::encode(restored.snapshot()) == bytes);
}

void independent_audio_track_edits_and_round_trip() {
  ProjectDocument document;
  CommandProcessor processor{document};
  const auto av = processor.import_asset("media/av.mp4", FakeProbe{{audio_stream(4)}});
  const auto silent = processor.import_asset("media/silent.mp4", FakeProbe{{}});
  const auto audio_track = processor.add_track(TrackKind::Audio);
  assert(document.timeline().find_track(audio_track)->kind() == TrackKind::Audio);

  const auto clip = processor.add_clip(audio_track, av, TimeRange{time(0), time(2)}, time(0));
  processor.set_clip_audio_gain(clip, -6.0);
  processor.set_clip_muted(clip, true);
  processor.move_clip(clip, audio_track, time(2));
  processor.trim_clip_start(clip, time(2, Rational{1, 1}) + time(1, Rational{1, 2}));
  const auto right = processor.split_clip(clip, time(3, Rational{1, 1}) + time(1, Rational{1, 2}));
  assert(document.timeline().find_clip(right)->source_range().start() ==
         document.timeline().find_clip(clip)->source_range().end());
  assert(document.timeline().find_clip(right)->audio_gain_db() == -6.0);
  assert(document.timeline().find_clip(right)->muted());
  const auto resolved = application::resolve_clip_at(document.snapshot(), audio_track,
                                                      time(15, Rational{1, 4}));
  assert(std::holds_alternative<ResolvedClip>(resolved));
  assert(std::get<ResolvedClip>(resolved).source_time == time(7, Rational{1, 4}));

  bool rejected = false;
  try {
    (void)processor.add_clip(audio_track, silent, TimeRange{time(0), time(1)}, time(5));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  const auto bytes = ProjectCodec::encode(document.snapshot());
  assert(bytes.find("TRACK 2 AUDIO") != std::string::npos);
  auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  assert(restored.timeline().find_track(audio_track)->kind() == TrackKind::Audio);
  assert(restored.timeline().find_clip(right)->audio_gain_db() == -6.0);
  assert(restored.timeline().find_clip(right)->muted());
  assert(ProjectCodec::encode(restored.snapshot()) == bytes);

  processor.remove_clip(right);
  assert(document.timeline().find_clip(right) == nullptr);
  assert(processor.undo());
  assert(document.timeline().find_clip(right) != nullptr);
  assert(processor.redo());
  assert(document.timeline().find_clip(right) == nullptr);
  assert(processor.undo());
  assert(processor.undo());
  assert(document.timeline().find_clip(right) == nullptr);
  assert(processor.redo());
  assert(document.timeline().find_clip(right) != nullptr);
}

// T3: audio metadata survives encode/decode/restore exactly; absence stays absent.
void audio_survives_codec_round_trip() {
  ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe with_audio{{audio_stream(4, 44100, 6)}};
  FakeProbe silent{{}};
  const auto av = processor.import_asset("media/av.mp4", with_audio);
  const auto muted = processor.import_asset("media/muted.mp4", silent);
  (void)processor.add_clip(av, TimeRange{time(0), time(10)}, time(0));
  (void)processor.add_clip(muted, TimeRange{time(0), time(10)}, time(10));
  const auto bytes = ProjectCodec::encode(document.snapshot());
  assert(bytes.find("AUDIO") != std::string::npos);
  auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  assert(ProjectCodec::encode(restored.snapshot()) == bytes);
  const auto restored_av = restored.find_asset(av);
  assert(restored_av.has_value() && restored_av->audio().has_value());
  assert(restored_av->audio()->stream_index == 4);
  assert(restored_av->audio()->sample_rate == 44100);
  assert(restored_av->audio()->channel_count == 6);
  assert((restored_av->audio()->time_base == Rational{1, 48000}));
  assert(restored_av->audio()->duration.has_value());
  assert(*restored_av->audio()->duration == time(96000, Rational{1, 48000}));
  const auto restored_muted = restored.find_asset(muted);
  assert(restored_muted.has_value() && !restored_muted->audio().has_value());
}

// T3b: missing audio duration round-trips as missing.
void audio_without_duration_round_trips() {
  ProjectDocument document;
  CommandProcessor processor{document};
  application::AudioStreamInfo stream = audio_stream(3);
  stream.duration.reset();
  FakeProbe probe{{stream}};
  const auto media = processor.import_asset("media/av.mp4", probe);
  assert(document.find_asset(media)->audio()->duration == std::nullopt);
  const auto bytes = ProjectCodec::encode(document.snapshot());
  auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  assert(ProjectCodec::encode(restored.snapshot()) == bytes);
  const auto asset = restored.find_asset(media);
  assert(asset.has_value() && asset->audio().has_value());
  assert(asset->audio()->stream_index == 3);
  assert(asset->audio()->duration == std::nullopt);
}

// T4: Outcome A proof — golden v1 decodes unchanged, absence means nullopt.
void golden_and_absence_decode_as_no_audio() {
  std::ifstream input{std::filesystem::path{PULSAR_SOURCE_DIR} / "tests" / "fixtures" /
                      "project_v1_golden.pulsar"};
  const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
  assert(!bytes.empty());
  assert(bytes.find("AUDIO") == std::string::npos);
  auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  for (const auto& asset : restored.assets()) {
    assert(!asset.audio().has_value());
  }
  // Legacy v1 opens losslessly; re-encoding emits the explicit v2 schema.
  const auto canonical = ProjectCodec::encode(restored.snapshot());
  assert(canonical.find("PULSAR_PROJECT 2\n") == 0);
  assert(ProjectDocument::restore(ProjectCodec::decode(canonical)).assets().size() ==
         restored.assets().size());
}

// Malformed audio suffixes are rejected; strictness for old shapes is kept.
void malformed_audio_suffix_rejected() {
  ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe probe{{audio_stream(1)}};
  (void)processor.import_asset("media/av.mp4", probe);
  const auto good = ProjectCodec::encode(document.snapshot());
  const auto asset_line = good.find("ASSET ");
  assert(asset_line != std::string::npos);
  const auto asset_end = good.find('\n', asset_line);
  assert(asset_end != std::string::npos);
  const std::string prefix = good.substr(0, asset_end);
  const std::string suffix = good.substr(asset_end);
  auto expect_reject = [](const std::string& text) {
    bool rejected = false;
    try {
      (void)ProjectCodec::decode(text);
    } catch (const ProjectCodecError&) {
      rejected = true;
    }
    assert(rejected);
  };
  expect_reject(prefix + " JUNK" + suffix);
  expect_reject(prefix + " AUDIO" + suffix);
  expect_reject(prefix + " AUDIO 1 48000" + suffix);
  expect_reject(prefix + " AUDIO x 48000 2 1 48000 96000 1 48000" + suffix);
  expect_reject(prefix + " AUDIO -1 48000 2 1 48000 96000 1 48000" + suffix);
  expect_reject(prefix + " AUDIO 1 0 2 1 48000 96000 1 48000" + suffix);
  expect_reject(prefix + " AUDIO 1 48000 0 1 48000 96000 1 48000" + suffix);
  expect_reject(prefix + " AUDIO 1 48000 2 1 0 96000 1 48000" + suffix);
  // Trailing garbage after a valid binding is still rejected.
  const auto audio_at = good.find(" AUDIO ");
  assert(audio_at != std::string::npos);
  expect_reject(good.substr(0, asset_end) + " EXTRA" + suffix);
  (void)audio_at;
}

// T5: resolver exposes audio identity (locator + metadata), no timestamp mapping.
void resolver_exposes_audio_identity() {
  ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe with_audio{{audio_stream(6)}};
  FakeProbe silent{{}};
  const auto av = processor.import_asset("media/av.mp4", with_audio);
  const auto muted = processor.import_asset("media/muted.mp4", silent);
  const auto av_clip = processor.add_clip(av, TimeRange{time(0), time(10)}, time(0));
  (void)processor.add_clip(muted, TimeRange{time(0), time(10)}, time(10));
  const auto snapshot = document.snapshot();
  const auto av_outcome =
      application::resolve_clip_at(snapshot, snapshot.video_track_id, time(2));
  assert(std::holds_alternative<ResolvedClip>(av_outcome));
  const auto resolved = std::get<ResolvedClip>(av_outcome);
  assert(resolved.clip_id == av_clip);
  assert(resolved.audio.has_value());
  assert(resolved.audio->source_locator == std::filesystem::path{"media/av.mp4"});
  assert(resolved.audio->metadata.stream_index == 6);
  assert(resolved.audio->metadata.sample_rate == 48000);
  assert(resolved.audio->metadata.channel_count == 2);
  assert((resolved.audio->metadata.time_base == Rational{1, 48000}));
  const auto muted_outcome =
      application::resolve_clip_at(snapshot, snapshot.video_track_id, time(12));
  assert(std::holds_alternative<ResolvedClip>(muted_outcome));
  assert(!std::get<ResolvedClip>(muted_outcome).audio.has_value());
}

class FakeClock final : public application::MonotonicClock {
public:
  application::MonotonicInstant now() const override {
    return application::MonotonicInstant{value};
  }
  void advance(MediaTime amount) { value = value + amount; }
  MediaTime value{MediaTime::zero()};
};

class FakeWorker final : public application::PlaybackMediaWorker {
public:
  explicit FakeWorker(int* seek_stream_index) : seek_stream_index_{seek_stream_index} {}

  application::VideoSourceDescriptor describe() const override {
    return application::VideoSourceDescriptor{
        application::StreamTimestamp{time(0)}, Rational{1, 90000},
        time(100, Rational{1, 1}), true};
  }

  void request_read(application::PlaybackGeneration generation) override {
    using namespace pulsar::application;
    events_.push_back(PlaybackWorkerSample{
        generation,
        VideoSample{next_ordinal_++, StreamTimestamp{last_target_ + time(1)}, time(10)}});
  }

  void request_seek_for_presentation(application::PlaybackGeneration generation,
                                     application::StreamTimestamp target) override {
    last_target_ = target.value();
    events_.push_back(application::PlaybackWorkerSeekComplete{
        generation, application::VideoSeekHit{
                        application::VideoSample{next_ordinal_++, target, time(10)},
                        std::nullopt}});
  }

  void cancel_pending() override {}

  std::optional<application::PlaybackWorkerEvent> poll_event() override {
    if (events_.empty()) {
      return std::nullopt;
    }
    auto event = std::move(events_.front());
    events_.pop_front();
    return event;
  }

  int* seek_stream_index() const noexcept { return seek_stream_index_; }

private:
  int* seek_stream_index_;
  std::deque<application::PlaybackWorkerEvent> events_;
  MediaTime last_target_{MediaTime::zero()};
  std::uint64_t next_ordinal_{1};
};

// T6: identical M9 presentation/clock behavior with and without audio metadata.
void audio_metadata_does_not_change_playback() {
  using application::TimelinePlaybackCoordinator;
  auto run_script = [](const application::ProjectSnapshot& snapshot,
                       std::vector<std::size_t>& kinds, std::vector<MediaTime>& positions,
                       std::vector<int>& streams) {
    FakeClock clock;
    int streams_seen = 0;
    application::PlaybackWorkerFactory factory =
        [&streams, &streams_seen](const std::filesystem::path&, int stream_index) {
          streams.push_back(stream_index);
          ++streams_seen;
          return std::make_unique<FakeWorker>(&streams_seen);
        };
    TimelinePlaybackCoordinator coordinator{clock, std::move(factory)};
    coordinator.set_snapshot(snapshot);
    kinds.push_back(coordinator.update().index());
    positions.push_back(coordinator.timeline_position());
    coordinator.play();
    clock.advance(time(3));
    kinds.push_back(coordinator.update().index());
    positions.push_back(coordinator.timeline_position());
    coordinator.seek(time(6));
    kinds.push_back(coordinator.update().index());
    positions.push_back(coordinator.timeline_position());
    coordinator.pause();
    clock.advance(time(5));
    kinds.push_back(coordinator.update().index());
    positions.push_back(coordinator.timeline_position());
    assert(coordinator.intent() == application::PlaybackIntent::Paused);
  };

  ProjectDocument silent_document;
  CommandProcessor silent_processor{silent_document};
  FakeProbe silent_probe{{}};
  const auto silent_media = silent_processor.import_asset("media/clip.mp4", silent_probe);
  (void)silent_processor.add_clip(silent_media, TimeRange{time(0), time(10)}, time(0));

  ProjectDocument av_document;
  CommandProcessor av_processor{av_document};
  FakeProbe av_probe{{audio_stream(9)}};
  const auto av_media = av_processor.import_asset("media/clip.mp4", av_probe);
  (void)av_processor.add_clip(av_media, TimeRange{time(0), time(10)}, time(0));

  std::vector<std::size_t> silent_kinds;
  std::vector<MediaTime> silent_positions;
  std::vector<int> silent_streams;
  run_script(silent_document.snapshot(), silent_kinds, silent_positions, silent_streams);

  std::vector<std::size_t> av_kinds;
  std::vector<MediaTime> av_positions;
  std::vector<int> av_streams;
  run_script(av_document.snapshot(), av_kinds, av_positions, av_streams);

  assert(silent_kinds == av_kinds);
  assert(silent_positions.size() == av_positions.size());
  for (std::size_t i = 0; i < silent_positions.size(); ++i) {
    assert(silent_positions[i] == av_positions[i]);
  }
  assert(silent_streams == av_streams);
  assert(!silent_streams.empty());
  assert(silent_streams.front() == 2);
}

void clip_gain_dsp_preserves_shape_and_timing() {
  application::AudioBlock block{48000, 2, 2, {0.5F, -0.5F, 1.0F, -1.0F},
                                Rational{1, 48000}, time(12, Rational{1, 48000})};
  const auto original_pts = block.presentation_timestamp();
  application::apply_clip_gain(block, application::ClipGain::from_decibels(-6.0, false));
  assert(block.frame_count() == 2 && block.channel_count() == 2);
  assert(block.samples()[0] > 0.24F && block.samples()[0] < 0.26F);
  assert(block.samples()[1] < -0.24F && block.samples()[1] > -0.26F);
  assert(block.presentation_timestamp() == original_pts);

  application::apply_clip_gain(block, application::ClipGain::from_decibels(0.0, true));
  for (const auto sample : block.samples()) assert(sample == 0.0F);
}

void clip_properties_follow_edits_history_and_persistence() {
  ProjectDocument document;
  CommandProcessor processor{document};
  const auto media = processor.import_asset("media/av.mp4", FakeProbe{{audio_stream(4)} });
  const auto clip = processor.add_clip(media, TimeRange{time(0), time(10)}, time(0));
  const auto* defaults = document.timeline().find_clip(clip);
  assert(defaults->audio_gain_db() == 0.0 && !defaults->muted());

  processor.set_clip_audio_gain(clip, -6.0);
  processor.set_clip_muted(clip, true);
  processor.set_clip_muted(clip, false);
  assert(document.timeline().find_clip(clip)->audio_gain_db() == -6.0);
  processor.set_clip_muted(clip, true);
  const auto right = processor.split_clip(clip, time(4));
  for (const auto id : {clip, right}) {
    const auto* child = document.timeline().find_clip(id);
    assert(child->audio_gain_db() == -6.0 && child->muted());
  }
  processor.trim_clip_start(right, time(5));
  processor.move_clip(right, document.video_track_id(), time(20));
  const auto* moved = document.timeline().find_clip(right);
  assert(moved->audio_gain_db() == -6.0 && moved->muted());

  assert(processor.undo());
  assert(document.timeline().find_clip(right)->timeline_start() == time(5));
  assert(processor.undo());
  assert(document.timeline().find_clip(right)->audio_gain_db() == -6.0);
  assert(processor.undo());
  assert(document.timeline().find_clip(clip)->audio_gain_db() == -6.0);
  assert(document.timeline().find_clip(clip)->muted());
  assert(processor.undo());
  assert(!document.timeline().find_clip(clip)->muted());
  assert(processor.redo());
  assert(document.timeline().find_clip(clip)->muted());

  const auto encoded = ProjectCodec::encode(document.snapshot());
  auto restored = ProjectDocument::restore(ProjectCodec::decode(encoded));
  const auto* restored_clip = restored.timeline().find_clip(clip);
  assert(restored_clip->audio_gain_db() == -6.0 && restored_clip->muted());
  assert(ProjectCodec::encode(restored.snapshot()) == encoded);
}

void invalid_clip_gain_is_rejected() {
  ProjectDocument document;
  CommandProcessor processor{document};
  const auto media = processor.import_asset("media/av.mp4", FakeProbe{{audio_stream(4)} });
  const auto clip = processor.add_clip(media, TimeRange{time(0), time(2)}, time(0));
  for (const auto value : {-61.0, 13.0, std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity()}) {
    bool rejected = false;
    try {
      processor.set_clip_audio_gain(clip, value);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
    assert(document.timeline().find_clip(clip)->audio_gain_db() == 0.0);
  }
}

}  // namespace

int main() {
  audio_selected_first_stream_deterministically();
  video_only_has_no_audio();
  audio_only_ingest_has_no_video_metadata();
  independent_audio_track_edits_and_round_trip();
  audio_survives_codec_round_trip();
  audio_without_duration_round_trips();
  golden_and_absence_decode_as_no_audio();
  malformed_audio_suffix_rejected();
  resolver_exposes_audio_identity();
  audio_metadata_does_not_change_playback();
  clip_gain_dsp_preserves_shape_and_timing();
  clip_properties_follow_edits_history_and_persistence();
  invalid_clip_gain_is_rejected();
}
