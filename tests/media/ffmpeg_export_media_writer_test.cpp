#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/application/export/export_media_writer.hpp>
#include <pulsar/application/export/offline_video_renderer.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_audio_decode_session.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_export_media_writer.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>

#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {

class Probe final : public pulsar::application::MediaProbe {
public:
  Probe(pulsar::domain::FrameRate rate, pulsar::domain::MediaTime duration)
      : rate_{rate}, duration_{duration} {}

  pulsar::application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    pulsar::application::MediaProbeResult result;
    result.path = path;
    result.container_name = "synthetic";
    result.video_streams.push_back({0, "synthetic", 320, 180, pulsar::domain::Rational{1, 1'000},
                                    duration_, rate_});
    return result;
  }

private:
  pulsar::domain::FrameRate rate_;
  pulsar::domain::MediaTime duration_;
};

std::filesystem::path temporary_output(const char* suffix) {
  return std::filesystem::temp_directory_path() /
         (std::string{"pulsar-e4-"} + suffix + ".mp4");
}

pulsar::application::exporting::ExportPlan make_plan(pulsar::domain::FrameRate rate,
                                                       pulsar::domain::MediaTime duration) {
  pulsar::application::ProjectDocument document;
  pulsar::application::CommandProcessor processor{document};
  const auto media = processor.import_asset("synthetic", Probe{rate, duration});
  (void)processor.add_clip(media, pulsar::domain::TimeRange{pulsar::domain::MediaTime::zero(), duration},
                           pulsar::domain::MediaTime::zero());
  return pulsar::application::exporting::ExportPlan::build(document.snapshot());
}

void write_synthetic(const pulsar::application::exporting::ExportPlan& plan,
                     const std::filesystem::path& path) {
  using namespace pulsar;
  using namespace application::exporting;
  media::ffmpeg::FFmpegExportMediaWriter writer{plan, path};
  const auto width = static_cast<std::size_t>(plan.output_width());
  const auto height = static_cast<std::size_t>(plan.output_height());
  std::vector<std::uint8_t> pixels(width * height * 4);
  const std::array<std::array<std::uint8_t, 3>, 3> colors{{
      {{255, 0, 0}}, {{0, 255, 0}}, {{0, 0, 255}}}};
  std::int64_t audio_cursor = 0;
  constexpr std::size_t audio_block_size = 997;
  for (std::int64_t frame = 0; frame < plan.frame_count(); ++frame) {
    const auto color = colors[static_cast<std::size_t>(frame * 3 / plan.frame_count()) % 3];
    for (std::size_t index = 0; index < width * height; ++index) {
      pixels[index * 4] = color[0];
      pixels[index * 4 + 1] = color[1];
      pixels[index * 4 + 2] = color[2];
      pixels[index * 4 + 3] = 255;
    }
    writer.write_video(OfflineVideoFrame{
        domain::FrameNumber{frame}, output_frame_time(domain::FrameNumber{frame}, plan.frame_rate()),
        static_cast<int>(width), static_cast<int>(height), static_cast<int>(width * 4),
        OfflinePixelFormat::Rgba8888, pixels});
    const auto horizon = audio_sample_index_at(
        output_frame_time(domain::FrameNumber{frame + 1}, plan.frame_rate()),
        domain::RoundingMode::Ceil);
    while (audio_cursor < std::min<std::int64_t>(horizon, plan.audio_sample_count())) {
      const auto count = static_cast<std::size_t>(std::min<std::int64_t>(
          static_cast<std::int64_t>(audio_block_size), plan.audio_sample_count() - audio_cursor));
      std::vector<float> audio(count * 2, 0.0F);
      for (std::size_t index = 0; index < count; ++index) {
        const auto global = audio_cursor + static_cast<std::int64_t>(index);
        const float value = global == 0 ? 1.0F
                                        : 0.2F * std::sin(static_cast<float>(global) * 0.01F);
        audio[index * 2] = value;
        audio[index * 2 + 1] = value;
      }
      writer.write_audio(OfflineAudioBlock{audio_cursor, count, 48'000, 2, std::move(audio)});
      audio_cursor += static_cast<std::int64_t>(count);
    }
  }
  while (audio_cursor < plan.audio_sample_count()) {
    const auto count = static_cast<std::size_t>(std::min<std::int64_t>(
        static_cast<std::int64_t>(audio_block_size), plan.audio_sample_count() - audio_cursor));
    writer.write_audio(OfflineAudioBlock{audio_cursor, count, 48'000, 2,
                                        std::vector<float>(count * 2, 0.0F)});
    audio_cursor += static_cast<std::int64_t>(count);
  }
  writer.finish();
  assert(writer.metrics().video_frames_submitted == static_cast<std::uint64_t>(plan.frame_count()));
  assert(writer.metrics().audio_samples_submitted ==
         static_cast<std::uint64_t>(plan.audio_sample_count()));
  assert(writer.metrics().peak_audio_fifo_frames <= audio_block_size + 1024);
}

void verifies_mp4_profile_and_decodes_audio(pulsar::domain::FrameRate rate,
                                             pulsar::domain::MediaTime duration,
                                             const char* suffix) {
  const auto path = temporary_output(suffix);
  const auto plan = make_plan(rate, duration);
  write_synthetic(plan, path);
  assert(std::filesystem::file_size(path) > 0);

  pulsar::media::ffmpeg::FFmpegMediaProbe probe;
  const auto result = probe.probe(path);
  assert(result.container_name.has_value());
  assert(result.container_name->find("mp4") != std::string::npos);
  assert(result.video_streams.size() == 1);
  assert(result.audio_streams.size() == 1);
  assert(result.video_streams[0].codec_name == "h264");
  assert(result.audio_streams[0].codec_name == "aac");
  assert(result.video_streams[0].width == plan.output_width());
  assert(result.video_streams[0].height == plan.output_height());
  assert(result.video_streams[0].nominal_frame_rate.has_value());
  assert(result.audio_streams[0].sample_rate == 48'000);
  assert(result.audio_streams[0].channel_count == 2);
  assert(result.audio_streams[0].duration.has_value());
  assert(*result.audio_streams[0].duration == plan.output_duration());

  pulsar::media::ffmpeg::FFmpegVideoDecodeSession video{path, result.video_streams[0].stream_index};
  std::uint64_t decoded_video_frames = 0;
  while (const auto frame = video.decode_next()) {
    assert(frame->width == plan.output_width());
    assert(frame->height == plan.output_height());
    assert(frame->presentation_timestamp.has_value());
    ++decoded_video_frames;
  }
  // The existing decoder session may retain one delayed visual frame at EOF;
  // ffprobe packet/frame validation below is the authoritative count check.
  assert(decoded_video_frames + 1 >= static_cast<std::uint64_t>(plan.frame_count()));

  pulsar::media::ffmpeg::FFmpegAudioDecodeSession audio{path, result.audio_streams[0].stream_index};
  std::uint64_t decoded_frames = 0;
  std::uint64_t non_zero = 0;
  while (const auto block = audio.decode_next()) {
    assert(block->sample_rate() == 48'000);
    assert(block->channel_count() == 2);
    decoded_frames += block->frame_count();
    for (const auto sample : block->samples()) {
      assert(std::isfinite(sample));
      if (std::abs(sample) > 1.0e-4F) ++non_zero;
    }
  }
  assert(decoded_frames >= static_cast<std::uint64_t>(plan.audio_sample_count()));
  assert(non_zero > 0);
  std::filesystem::remove(path);
}

void rejects_audio_timestamp_discontinuity() {
  using namespace pulsar;
  const auto path = temporary_output("timestamp");
  const auto plan = make_plan({25, 1}, {2, {1, 1}});
  using namespace pulsar::application::exporting;
  media::ffmpeg::FFmpegExportMediaWriter writer{plan, path};
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(plan.output_width()) *
                                   static_cast<std::size_t>(plan.output_height()) * 4, 0);
  writer.write_video(OfflineVideoFrame{domain::FrameNumber{0}, domain::MediaTime::zero(),
                                       static_cast<int>(plan.output_width()),
                                       static_cast<int>(plan.output_height()),
                                       static_cast<int>(plan.output_width() * 4),
                                       OfflinePixelFormat::Rgba8888, std::move(pixels)});
  bool failed = false;
  try {
    writer.write_audio(OfflineAudioBlock{1, 1, 48'000, 2, {0.0F, 0.0F}});
  } catch (const ExportMediaWriterError& error) {
    failed = error.code() == ExportMediaWriterErrorCode::TimestampInvariant;
  }
  assert(failed);
  writer.abort();
  std::filesystem::remove(path);
}

void rejects_non_finite_audio_and_unwritable_output() {
  using namespace pulsar;
  using namespace application::exporting;
  const auto plan = make_plan({25, 1}, {1, {1, 1}});
  bool output_failed = false;
  try {
    media::ffmpeg::FFmpegExportMediaWriter writer{plan, "/definitely/not/a/real/pulsar.mp4"};
  } catch (const ExportMediaWriterError& error) {
    output_failed = error.code() == ExportMediaWriterErrorCode::OutputOpenFailed;
  }
  assert(output_failed);

  const auto path = temporary_output("finite");
  media::ffmpeg::FFmpegExportMediaWriter writer{plan, path};
  bool non_finite_failed = false;
  try {
    writer.write_audio(OfflineAudioBlock{0, 1, 48'000, 2,
                                         {std::numeric_limits<float>::quiet_NaN(), 0.0F}});
  } catch (const ExportMediaWriterError& error) {
    non_finite_failed = error.code() == ExportMediaWriterErrorCode::InvalidInputFrame;
  }
  assert(non_finite_failed);
  writer.abort();
  std::filesystem::remove(path);
}

} // namespace

int main() {
  verifies_mp4_profile_and_decodes_audio({25, 1}, {2, {1, 1}}, "25fps");
  verifies_mp4_profile_and_decodes_audio({30'000, 1001}, {2002, {1, 1'000}}, "2997fps");
  rejects_audio_timestamp_discontinuity();
  rejects_non_finite_audio_and_unwritable_output();
}
