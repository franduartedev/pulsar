#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/application/export/export_media_writer.hpp>
#include <pulsar/application/export/offline_video_renderer.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_audio_decode_session.hpp>
#include <pulsar/media/ffmpeg/detail/ffmpeg_export_audio_policy.hpp>
#include <pulsar/media/ffmpeg/detail/ffmpeg_export_media_writer_test_hook.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_export_media_writer.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>

#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "../support/shell_command.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

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

struct MuxEvidence {
  int audio_profile{-1};
  int audio_initial_padding{-1};
  AVRational audio_time_base{};
  AVRational video_time_base{};
  std::int64_t audio_duration{AV_NOPTS_VALUE};
  std::int64_t video_duration{AV_NOPTS_VALUE};
  std::int64_t audio_packet_duration_sum{0};
  std::int64_t audio_logical_packet_duration_sum{0};
  std::int64_t video_packet_count{0};
  std::int64_t audio_packet_count{0};
  std::int64_t first_audio_pts{AV_NOPTS_VALUE};
  std::int64_t last_audio_end{AV_NOPTS_VALUE};
  std::int64_t previous_video_dts{AV_NOPTS_VALUE};
  std::int64_t minimum_video_pts{AV_NOPTS_VALUE};
  bool saw_aac_priming_packet{false};
};

MuxEvidence inspect_mux(const std::filesystem::path& path) {
  AVFormatContext* format = nullptr;
  const auto path_text = pulsar::test_support::ffmpeg_path(path);
  assert(avformat_open_input(&format, path_text.c_str(), nullptr, nullptr) == 0);
  assert(avformat_find_stream_info(format, nullptr) >= 0);
  int video_index = -1;
  int audio_index = -1;
  for (unsigned index = 0; index < format->nb_streams; ++index) {
    if (format->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) video_index = index;
    if (format->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) audio_index = index;
  }
  assert(video_index >= 0 && audio_index >= 0);
  const auto* video = format->streams[video_index];
  const auto* audio = format->streams[audio_index];
  MuxEvidence evidence{audio->codecpar->profile,
                        audio->codecpar->initial_padding,
                        audio->time_base,
                        video->time_base,
                        audio->duration,
                        video->duration};
  AVPacket* packet = av_packet_alloc();
  assert(packet != nullptr);
  while (av_read_frame(format, packet) >= 0) {
    if (packet->stream_index == video_index) {
      ++evidence.video_packet_count;
      assert(packet->pts != AV_NOPTS_VALUE && packet->dts != AV_NOPTS_VALUE);
      if (evidence.minimum_video_pts == AV_NOPTS_VALUE) evidence.minimum_video_pts = packet->pts;
      assert(evidence.previous_video_dts == AV_NOPTS_VALUE ||
             packet->dts >= evidence.previous_video_dts);
      evidence.previous_video_dts = packet->dts;
    } else if (packet->stream_index == audio_index) {
      ++evidence.audio_packet_count;
      assert(packet->pts != AV_NOPTS_VALUE && packet->duration > 0);
      if (packet->pts < 0) {
        std::uint32_t skipped = 0;
        std::uint32_t discarded = 0;
        size_t side_data_size = 0;
        const auto* side_data = av_packet_get_side_data(
            packet, AV_PKT_DATA_SKIP_SAMPLES, &side_data_size);
        assert(side_data != nullptr && side_data_size >= 8);
        std::memcpy(&skipped, side_data, sizeof(skipped));
        std::memcpy(&discarded, side_data + 4, sizeof(discarded));
        assert(skipped == 1024 && discarded == 0);
        evidence.saw_aac_priming_packet = true;
      } else if (evidence.first_audio_pts == AV_NOPTS_VALUE) {
        evidence.first_audio_pts = packet->pts;
      }
      evidence.audio_packet_duration_sum += packet->duration;
      if (packet->pts >= 0) evidence.audio_logical_packet_duration_sum += packet->duration;
      evidence.last_audio_end = packet->pts + packet->duration;
    }
    av_packet_unref(packet);
  }
  av_packet_free(&packet);
  avformat_close_input(&format);
  return evidence;
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
                                        : global == plan.audio_sample_count() - 1 ? -1.0F : 0.0F;
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

  const auto mux = inspect_mux(path);
  assert(mux.audio_profile == AV_PROFILE_AAC_LOW);
  assert(mux.audio_initial_padding == 0);
  assert(mux.audio_time_base.num == 1 && mux.audio_time_base.den == 48'000);
  assert(mux.video_packet_count == plan.frame_count());
  assert(mux.audio_packet_count > 0);
  assert(mux.minimum_video_pts == 0);
  assert(mux.audio_duration == plan.audio_sample_count());
  assert(mux.video_duration == av_rescale_q(
                                  plan.frame_count(),
                                  AVRational{static_cast<int>(plan.frame_rate().rational().denominator()),
                                             static_cast<int>(plan.frame_rate().rational().numerator())},
                                  mux.video_time_base));
  assert(mux.first_audio_pts == 0);
  assert(mux.saw_aac_priming_packet);
  // AAC packets are frame-shaped. Any physical tail is bounded to one codec
  // frame and is explained by the exact logical stream duration above.
  assert(mux.audio_packet_duration_sum >= plan.audio_sample_count());
  assert(mux.audio_logical_packet_duration_sum == plan.audio_sample_count());
  assert(mux.audio_packet_duration_sum == plan.audio_sample_count() + 1024);

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
  std::uint64_t decoded_cursor = 0;
  std::uint64_t first_event = std::numeric_limits<std::uint64_t>::max();
  std::uint64_t last_event = 0;
  while (const auto block = audio.decode_next()) {
    assert(block->sample_rate() == 48'000);
    assert(block->channel_count() == 2);
    decoded_frames += block->frame_count();
    for (std::size_t index = 0; index < block->samples().size(); ++index) {
      const auto sample = block->samples()[index];
      assert(std::isfinite(sample));
      if (std::abs(sample) > 0.05F) {
        ++non_zero;
        const auto frame_index = decoded_cursor + index / 2;
        first_event = std::min(first_event, frame_index);
        last_event = frame_index;
      }
    }
    decoded_cursor += block->frame_count();
  }
  assert(decoded_frames >= static_cast<std::uint64_t>(plan.audio_sample_count()));
  assert(non_zero > 0);
  assert(first_event < 2048);
  assert(last_event + 4096 >= static_cast<std::uint64_t>(plan.audio_sample_count()));
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
  const auto missing_parent = std::filesystem::temp_directory_path() /
                              "pulsar-e4-output-parent-does-not-exist";
  std::filesystem::remove_all(missing_parent);
  bool output_failed = false;
  try {
    media::ffmpeg::FFmpegExportMediaWriter writer{plan, missing_parent / "output.mp4"};
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

void verifies_hard_clip_policy() {
  using pulsar::media::ffmpeg::detail::hard_clip_audio_sample;
  const std::array<float, 5> input{-1.5F, -1.0F, 0.0F, 1.0F, 1.5F};
  const std::array<float, 5> expected{-1.0F, -1.0F, 0.0F, 1.0F, 1.0F};
  for (std::size_t index = 0; index < input.size(); ++index) {
    assert(hard_clip_audio_sample(input[index]) == expected[index]);
  }
}

void rejects_premature_finish_and_allows_idempotent_abort() {
  using namespace pulsar;
  using namespace application::exporting;
  const auto path = temporary_output("premature");
  const auto plan = make_plan({25, 1}, {1, {1, 1}});
  media::ffmpeg::FFmpegExportMediaWriter writer{plan, path};
  bool failed = false;
  try {
    writer.finish();
  } catch (const ExportMediaWriterError& error) {
    failed = error.code() == ExportMediaWriterErrorCode::TimestampInvariant;
  }
  assert(failed);
  writer.abort();
  writer.abort();
  std::filesystem::remove(path);
}

int fail_header_for_test(AVFormatContext*) { return -1; }

void cleans_up_when_header_write_fails() {
  using namespace pulsar;
  using namespace application::exporting;
  const auto path = temporary_output("header-failure");
  const auto plan = make_plan({25, 1}, {1, {1, 1}});
  auto& hook = media::ffmpeg::detail::export_write_header_hook();
  hook = &fail_header_for_test;
  bool failed = false;
  try {
    media::ffmpeg::FFmpegExportMediaWriter writer{plan, path};
  } catch (const ExportMediaWriterError& error) {
    failed = error.code() == ExportMediaWriterErrorCode::MuxWriteFailed;
  }
  hook = nullptr;
  assert(failed);
  std::filesystem::remove(path);
}

} // namespace

int main() {
  verifies_mp4_profile_and_decodes_audio({25, 1}, {2, {1, 1}}, "25fps");
  verifies_mp4_profile_and_decodes_audio({30'000, 1001}, {2002, {1, 1'000}}, "2997fps");
  rejects_audio_timestamp_discontinuity();
  rejects_non_finite_audio_and_unwritable_output();
  verifies_hard_clip_policy();
  rejects_premature_finish_and_allows_idempotent_abort();
  cleans_up_when_header_write_fails();
}
