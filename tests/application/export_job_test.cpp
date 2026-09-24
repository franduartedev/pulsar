#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/export/export_job.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace pulsar;
namespace exporting = application::exporting;

class Probe final : public application::MediaProbe {
public:
  explicit Probe(domain::MediaTime duration = domain::MediaTime{1, {1, 1}})
      : duration_{duration} {}

  application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    application::MediaProbeResult result;
    result.path = path;
    result.video_streams.push_back({0, "rawvideo", 2, 2, {1, 1},
                                    duration_, domain::FrameRate{1, 1}});
    result.audio_streams.push_back({0, "pcm", 48'000, 2, {1, 48'000},
                                    duration_});
    return result;
  }

private:
  domain::MediaTime duration_;
};

application::ProjectSnapshot snapshot_a() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto media = processor.import_asset("snapshot-A", Probe{});
  (void)processor.add_clip(media, domain::TimeRange{{0, {1, 1}}, {1, {1, 1}}},
                           domain::MediaTime::zero());
  return document.snapshot();
}

application::ProjectSnapshot snapshot_two_seconds() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto media = processor.import_asset("snapshot-A", Probe{domain::MediaTime{2, {1, 1}}});
  (void)processor.add_clip(media, domain::TimeRange{{0, {1, 1}}, {2, {1, 1}}},
                           domain::MediaTime::zero());
  return document.snapshot();
}

class VideoSource final : public exporting::OfflineVideoSource {
public:
  explicit VideoSource(bool fail) : fail_{fail} {}

  exporting::OfflineVideoSeekResult seek_to_presentation_timestamp(domain::MediaTime) override {
    if (fail_) throw exporting::OfflineVideoRenderError{
        exporting::OfflineVideoRenderErrorCode::DecodeFailed, "video fake failure"};
    auto image = std::make_shared<const exporting::OfflineVideoPixelBuffer>(
        exporting::OfflineVideoPixelBuffer{2, 2, 8, std::vector<std::uint8_t>(16, 255)});
    return exporting::OfflineVideoSeekResult{
        exporting::OfflineVideoSourceFrame{domain::MediaTime::zero(), domain::MediaTime{1, {1, 1}},
                                           std::move(image)},
        std::nullopt, 1};
  }

  std::optional<exporting::OfflineVideoSourceFrame> decode_next() override { return std::nullopt; }

private:
  bool fail_;
};

class VideoFactory final : public exporting::OfflineVideoSourceFactory {
public:
  bool fail{false};
  std::filesystem::path last_locator;

  std::unique_ptr<exporting::OfflineVideoSource> open(const std::filesystem::path& locator,
                                                      int) override {
    last_locator = locator;
    return std::make_unique<VideoSource>(fail);
  }
};

class AudioSource final : public exporting::OfflineAudioSource {
public:
  explicit AudioSource(bool fail) : fail_{fail} {}

  int sample_rate() const noexcept override { return 48'000; }
  int channel_count() const noexcept override { return 2; }
  std::optional<domain::MediaTime> stream_origin() const override {
    return domain::MediaTime::zero();
  }
  void seek_to(domain::MediaTime) override { cursor_ = 0; }

  std::optional<exporting::OfflineAudioSourceBlock> decode_next() override {
    if (fail_ && cursor_ > 0) {
      throw exporting::OfflineAudioRenderError{
          exporting::OfflineAudioRenderErrorCode::DecodeFailed, "audio fake failure"};
    }
    if (cursor_ >= 96'000) return std::nullopt;
    const auto count = std::min<std::int64_t>(4096, 96'000 - cursor_);
    const auto first = cursor_;
    cursor_ += count;
    return exporting::OfflineAudioSourceBlock{
        48'000, 2, static_cast<std::size_t>(count),
        std::vector<float>(static_cast<std::size_t>(count) * 2, 0.0F),
        domain::MediaTime{first, {1, 48'000}}};
  }

private:
  bool fail_;
  std::int64_t cursor_{0};
};

class AudioFactory final : public exporting::OfflineAudioSourceFactory {
public:
  bool fail{false};

  std::unique_ptr<exporting::OfflineAudioSource> open(const std::filesystem::path&, int) override {
    return std::make_unique<AudioSource>(fail);
  }
};

class FakePublisher final : public exporting::ExportOutputPublisher {
public:
  int create_count{0};
  int cleanup_count{0};
  bool publish_fail{false};
  bool temp_live{false};
  std::string destination_content{"old"};
  std::string published_content;

  std::filesystem::path create_temporary_sibling(const std::filesystem::path&) override {
    temp_live = true;
    return std::filesystem::path{"fake-temp-" + std::to_string(++create_count)};
  }
  bool temporary_exists(const std::filesystem::path&) const override { return temp_live; }
  void publish_replace_existing_safely(const std::filesystem::path&,
                                       const std::filesystem::path&) override {
    if (publish_fail) throw std::runtime_error{"fake publish failure"};
    published_content = "new";
    destination_content = published_content;
    temp_live = false;
  }
  void cleanup_temporary(const std::filesystem::path&) noexcept override {
    ++cleanup_count;
    temp_live = false;
  }
};

class FakeWriter final : public exporting::ExportMediaWriter {
public:
  FakeWriter(int& video_writes, int& audio_writes, bool fail_video, bool fail_audio,
             bool fail_finish)
      : video_writes_{video_writes}, audio_writes_{audio_writes}, fail_video_{fail_video},
        fail_audio_{fail_audio}, fail_finish_{fail_finish} {}

  void write_video(const exporting::OfflineVideoFrame&) override {
    ++video_writes_;
    if (fail_video_) throw exporting::ExportMediaWriterError{
        exporting::ExportMediaWriterErrorCode::MuxWriteFailed, "fake video write failure"};
  }
  void write_audio(const exporting::OfflineAudioBlock&) override {
    ++audio_writes_;
    if (fail_audio_) throw exporting::ExportMediaWriterError{
        exporting::ExportMediaWriterErrorCode::MuxWriteFailed, "fake audio write failure"};
  }
  void finish() override {
    if (fail_finish_) throw exporting::ExportMediaWriterError{
        exporting::ExportMediaWriterErrorCode::FinalizeFailed, "fake finish failure"};
  }
  void abort() noexcept override { ++aborts; }
  const exporting::ExportMediaWriterMetrics& metrics() const noexcept override { return metrics_; }

  int aborts{0};

private:
  int& video_writes_;
  int& audio_writes_;
  bool fail_video_;
  bool fail_audio_;
  bool fail_finish_;
  exporting::ExportMediaWriterMetrics metrics_;
};

class WriterFactory final : public exporting::ExportMediaWriterFactory {
public:
  bool fail_open{false};
  bool fail_video{false};
  bool fail_audio{false};
  bool fail_finish{false};
  int video_writes{0};
  int audio_writes{0};

  std::unique_ptr<exporting::ExportMediaWriter> open(const exporting::ExportPlan&,
                                                     const std::filesystem::path&) override {
    if (fail_open) throw exporting::ExportMediaWriterError{
        exporting::ExportMediaWriterErrorCode::OutputOpenFailed, "fake open failure"};
    return std::make_unique<FakeWriter>(video_writes, audio_writes, fail_video, fail_audio,
                                        fail_finish);
  }
};

struct Fixture {
  VideoFactory video;
  AudioFactory audio;
  FakePublisher publisher;
  WriterFactory writers;
  exporting::ExportJob job;

  Fixture() : job{video, audio, writers, publisher} {}
};

exporting::ExportRequest request() {
  return exporting::ExportRequest{snapshot_a(), "final.mp4", exporting::ExportProfile::beta_v01()};
}

void assert_error(Fixture& fixture, exporting::ExportRequest export_request,
                  exporting::ExportErrorCode expected) {
  try {
    (void)fixture.job.run(export_request);
    assert(false);
  } catch (const exporting::ExportError& error) {
    assert(error.code() == expected);
  }
  assert(fixture.publisher.destination_content == "old");
  assert(!fixture.publisher.temp_live);
  assert(fixture.publisher.cleanup_count == 1);
}

void success_is_incremental_and_progress_is_monotonic() {
  Fixture fixture;
  std::vector<double> progress;
  const auto result = fixture.job.run(request(), {}, [&](const exporting::ExportProgress& value) {
    progress.push_back(value.fraction);
  });
  assert(result.progress.fraction == 1.0);
  assert(fixture.publisher.destination_content == "new");
  assert(!fixture.publisher.temp_live);
  assert(fixture.writers.video_writes == 1);
  assert(fixture.writers.audio_writes > 1);
  for (std::size_t index = 1; index < progress.size(); ++index) {
    assert(progress[index] >= progress[index - 1]);
    assert(progress[index] >= 0.0 && progress[index] <= 1.0);
  }
}

void failure_matrix_preserves_destination() {
  {
    Fixture fixture;
    fixture.video.fail = true;
    assert_error(fixture, request(), exporting::ExportErrorCode::DecodeFailed);
  }
  {
    Fixture fixture;
    fixture.audio.fail = true;
    assert_error(fixture, request(), exporting::ExportErrorCode::DecodeFailed);
  }
  {
    Fixture fixture;
    fixture.writers.fail_video = true;
    assert_error(fixture, request(), exporting::ExportErrorCode::MuxFailed);
  }
  {
    Fixture fixture;
    fixture.writers.fail_audio = true;
    assert_error(fixture, request(), exporting::ExportErrorCode::MuxFailed);
  }
  {
    Fixture fixture;
    fixture.writers.fail_finish = true;
    assert_error(fixture, request(), exporting::ExportErrorCode::FinalizeFailed);
  }
  {
    Fixture fixture;
    fixture.publisher.publish_fail = true;
    assert_error(fixture, request(), exporting::ExportErrorCode::PublishFailed);
  }
}

void cancellation_and_snapshot_are_safe() {
  Fixture fixture;
  bool saw_subunit_progress = false;
  auto export_request = request();
  const auto generation = export_request.snapshot.document_generation;
  bool cancelled = false;
  try {
    (void)fixture.job.run(export_request, [&] { return fixture.writers.audio_writes >= 1; },
                    [&](const exporting::ExportProgress& value) {
                      saw_subunit_progress |= value.fraction < 1.0;
                    });
  } catch (const exporting::ExportError& error) {
    cancelled = error.code() == exporting::ExportErrorCode::Cancelled;
  }
  assert(cancelled && saw_subunit_progress);
  assert(fixture.writers.audio_writes == 1);
  assert(fixture.publisher.destination_content == "old");
  assert(!fixture.publisher.temp_live);
  assert(fixture.publisher.cleanup_count == 1);
  assert(export_request.snapshot.document_generation == generation);
}

void cancellation_is_observed_between_video_frames() {
  Fixture fixture;
  auto export_request = exporting::ExportRequest{snapshot_two_seconds(), "mid-video.mp4",
                                                  exporting::ExportProfile::beta_v01()};
  bool cancelled = false;
  try {
    (void)fixture.job.run(export_request, [&] { return fixture.writers.video_writes >= 1; });
  } catch (const exporting::ExportError& error) {
    cancelled = error.code() == exporting::ExportErrorCode::Cancelled;
  }
  assert(cancelled);
  assert(fixture.writers.video_writes == 1);
  assert(fixture.publisher.destination_content == "old");
  assert(!fixture.publisher.temp_live);
}

void cancellation_before_start_and_unique_temps() {
  Fixture fixture;
  bool cancelled = false;
  try {
    (void)fixture.job.run(request(), [] { return true; });
  } catch (const exporting::ExportError& error) {
    cancelled = error.code() == exporting::ExportErrorCode::Cancelled;
  }
  assert(cancelled);
  assert(fixture.publisher.create_count == 0);

  auto first = request();
  auto second = request();
  const auto first_result = fixture.job.run(first);
  const auto second_result = fixture.job.run(second);
  assert(first_result.destination == second_result.destination);
  assert(fixture.publisher.create_count == 2);
}

void snapshot_is_frozen_before_document_mutation() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto media = processor.import_asset("snapshot-A", Probe{});
  (void)processor.add_clip(media, domain::TimeRange{{0, {1, 1}}, {1, {1, 1}}},
                           domain::MediaTime::zero());
  const auto frozen = document.snapshot();
  const auto generation = frozen.document_generation;
  (void)processor.add_track(domain::TrackKind::Audio);
  assert(document.document_generation() != generation);

  Fixture fixture;
  const auto result = fixture.job.run(
      exporting::ExportRequest{frozen, "frozen.mp4", exporting::ExportProfile::beta_v01()});
  assert(result.document_generation == generation);
  assert(fixture.video.last_locator == "snapshot-A");
}

void rejects_reentrant_run_on_same_job() {
  Fixture fixture;
  bool rejected = false;
  bool attempted = false;
  (void)fixture.job.run(request(), {}, [&](const exporting::ExportProgress&) {
    if (attempted) return;
    attempted = true;
    try {
      (void)fixture.job.run(request());
    } catch (const exporting::ExportError& error) {
      rejected = error.code() == exporting::ExportErrorCode::InternalInvariant;
    }
  });
  assert(rejected);
}

} // namespace

int main() {
  success_is_incremental_and_progress_is_monotonic();
  failure_matrix_preserves_destination();
  cancellation_and_snapshot_are_safe();
  cancellation_is_observed_between_video_frames();
  cancellation_before_start_and_unique_temps();
  snapshot_is_frozen_before_document_mutation();
  rejects_reentrant_run_on_same_job();
}
