#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/application/export/offline_audio_renderer.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_audio_source.hpp>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <utility>
#include <vector>

namespace {

void renders_real_embedded_audio_sequentially() {
  pulsar::application::ProjectDocument document;
  pulsar::application::CommandProcessor processor{document};
  const std::filesystem::path fixture =
      std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/audio_decode_fixture.mp4";
  pulsar::media::ffmpeg::FFmpegMediaProbe probe;
  const auto media = processor.import_asset(fixture, probe);
  const auto asset = document.find_asset(media);
  assert(asset.has_value() && asset->audio().has_value());
  (void)processor.add_clip(media,
                           pulsar::domain::TimeRange{pulsar::domain::MediaTime::zero(),
                                                     asset->playable_duration()},
                           pulsar::domain::MediaTime::zero());

  const auto plan = pulsar::application::exporting::ExportPlan::build(document.snapshot());
  pulsar::media::ffmpeg::FFmpegOfflineAudioSourceFactory factory;
  pulsar::application::exporting::OfflineAudioRenderer renderer{plan, factory, 997};
  std::uint64_t non_zero = 0;
  std::int64_t samples = 0;
  while (const auto block = renderer.render_next_block()) {
    assert(block->sample_rate == 48'000);
    assert(block->channel_count == 2);
    samples += static_cast<std::int64_t>(block->frame_count);
    for (const auto sample : block->samples) {
      assert(std::isfinite(sample));
      if (std::abs(sample) > 1.0e-5F) ++non_zero;
    }
  }
  assert(samples == plan.audio_sample_count());
  assert(non_zero > 0);
  assert(renderer.metrics().source_opens == 1);
  assert(renderer.metrics().seeks == 1);
  assert(renderer.metrics().peak_buffered_source_frames <= 3 * 4096);
}

// audio_decode_fixture.mp4 carries 44.1 kHz audio. A source start of 0.127 s is
// 5600.7 samples: not on the physical sample grid. This used to abort the export
// with "audio sample request escaped source range".
void renders_real_audio_with_unaligned_source_range() {
  using pulsar::domain::MediaTime;
  using pulsar::domain::TimeRange;
  pulsar::application::ProjectDocument document;
  pulsar::application::CommandProcessor processor{document};
  const std::filesystem::path fixture =
      std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/audio_decode_fixture.mp4";
  pulsar::media::ffmpeg::FFmpegMediaProbe probe;
  const auto media = processor.import_asset(fixture, probe);
  const auto asset = document.find_asset(media);
  assert(asset.has_value() && asset->audio().has_value());
  assert(asset->audio()->sample_rate == 44'100);
  (void)processor.add_clip(media, TimeRange{MediaTime{127, {1, 1000}}, MediaTime{377, {1, 1000}}},
                           MediaTime::zero());

  const auto plan = pulsar::application::exporting::ExportPlan::build(document.snapshot());
  pulsar::media::ffmpeg::FFmpegOfflineAudioSourceFactory factory;
  std::vector<float> reference;
  for (const std::size_t block_size : {std::size_t{4096}, std::size_t{331}}) {
    pulsar::application::exporting::OfflineAudioRenderer renderer{plan, factory, block_size};
    std::vector<float> pcm;
    while (const auto block = renderer.render_next_block()) {
      pcm.insert(pcm.end(), block->samples.begin(), block->samples.end());
    }
    assert(pcm.size() == static_cast<std::size_t>(plan.audio_sample_count()) * 2);
    std::uint64_t non_zero = 0;
    for (const auto sample : pcm) {
      assert(std::isfinite(sample));
      if (std::abs(sample) > 1.0e-5F) ++non_zero;
    }
    assert(non_zero > 0);
    assert(renderer.metrics().inserted_source_gap_silence == 0);
    assert(renderer.metrics().source_opens == 1);
    if (reference.empty()) reference = std::move(pcm);
    else assert(reference == pcm);
  }
}

} // namespace

int main() {
  renders_real_embedded_audio_sequentially();
  renders_real_audio_with_unaligned_source_range();
}
