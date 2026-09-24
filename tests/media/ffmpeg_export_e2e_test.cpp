#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/export/export_job.hpp>
#include <pulsar/application/media/audio_block.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_audio_decode_session.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_export_media_writer.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_audio_source.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_video_source.hpp>
#include <pulsar/project_file/file_project_repository.hpp>
#include <pulsar/project_file/filesystem_export_output_publisher.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
using namespace pulsar;
namespace exporting = application::exporting;

domain::MediaTime seconds(std::int64_t value) {
  return domain::MediaTime{value, domain::Rational{1, 1}};
}

class WriterFactory final : public exporting::ExportMediaWriterFactory {
public:
  std::unique_ptr<exporting::ExportMediaWriter> open(
      const exporting::ExportPlan& plan, const std::filesystem::path& path) override {
    return std::make_unique<media::ffmpeg::FFmpegExportMediaWriter>(plan, path);
  }
};

class BuiltinSoundProbe final : public application::MediaProbe {
public:
  application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    return {path, std::string{"WAV"}, domain::MediaTime{4800, domain::Rational{1, 4800}}, {},
            {{0, "pcm_s16le", 48000, 1, domain::Rational{1, 48000},
              domain::MediaTime{48000, domain::Rational{1, 48000}}}}};
  }
};

void assert_monotonic_progress(const std::vector<double>& values) {
  assert(!values.empty());
  assert(values.back() == 1.0);
  for (std::size_t i = 0; i < values.size(); ++i) {
    assert(values[i] >= 0.0 && values[i] <= 1.0);
    if (i != 0) assert(values[i] >= values[i - 1]);
    if (i + 1 != values.size()) assert(values[i] < 1.0);
  }
}

struct VideoSample {
  double timestamp;
  int red;
  int green;
  int blue;
};

std::vector<VideoSample> decode_video_samples(const std::filesystem::path& path) {
  AVFormatContext* format = nullptr;
  assert(avformat_open_input(&format, path.c_str(), nullptr, nullptr) == 0);
  assert(avformat_find_stream_info(format, nullptr) >= 0);
  const int stream_index = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  assert(stream_index >= 0);
  AVStream* stream = format->streams[stream_index];
  const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
  assert(codec != nullptr);
  AVCodecContext* decoder = avcodec_alloc_context3(codec);
  assert(decoder != nullptr);
  assert(avcodec_parameters_to_context(decoder, stream->codecpar) >= 0);
  assert(avcodec_open2(decoder, codec, nullptr) >= 0);
  AVFrame* frame = av_frame_alloc();
  AVFrame* rgb = av_frame_alloc();
  assert(frame != nullptr && rgb != nullptr);
  const int stride = decoder->width * 3;
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride * decoder->height));
  assert(av_image_fill_arrays(rgb->data, rgb->linesize, pixels.data(), AV_PIX_FMT_RGB24,
                              decoder->width, decoder->height, 1) >= 0);
  SwsContext* convert = sws_getContext(decoder->width, decoder->height, decoder->pix_fmt,
                                       decoder->width, decoder->height, AV_PIX_FMT_RGB24,
                                       SWS_BILINEAR, nullptr, nullptr, nullptr);
  assert(convert != nullptr);
  std::vector<VideoSample> result;
  auto collect = [&] {
    assert(sws_scale(convert, frame->data, frame->linesize, 0, decoder->height, rgb->data,
                     rgb->linesize) > 0);
    const int x = decoder->width / 2;
    const int y = decoder->height / 2;
    const auto* pixel = rgb->data[0] + y * rgb->linesize[0] + x * 3;
    const auto timestamp = frame->best_effort_timestamp == AV_NOPTS_VALUE
                               ? 0.0
                               : frame->best_effort_timestamp * av_q2d(stream->time_base);
    result.push_back(VideoSample{timestamp, pixel[0], pixel[1], pixel[2]});
  };
  AVPacket* packet = av_packet_alloc();
  assert(packet != nullptr);
  while (av_read_frame(format, packet) >= 0) {
    if (packet->stream_index == stream_index && avcodec_send_packet(decoder, packet) >= 0) {
      while (avcodec_receive_frame(decoder, frame) == 0) collect();
    }
    av_packet_unref(packet);
  }
  assert(avcodec_send_packet(decoder, nullptr) >= 0);
  while (avcodec_receive_frame(decoder, frame) == 0) collect();
  av_packet_free(&packet);
  sws_freeContext(convert);
  av_frame_free(&rgb);
  av_frame_free(&frame);
  avcodec_free_context(&decoder);
  avformat_close_input(&format);
  return result;
}

VideoSample nearest(const std::vector<VideoSample>& samples, double timestamp) {
  assert(!samples.empty());
  return *std::min_element(samples.begin(), samples.end(), [timestamp](const auto& left,
                                                                        const auto& right) {
    return std::abs(left.timestamp - timestamp) < std::abs(right.timestamp - timestamp);
  });
}

void assert_colour(const VideoSample& sample, int red, int green, int blue) {
  assert(std::abs(sample.red - red) < 35);
  assert(std::abs(sample.green - green) < 35);
  assert(std::abs(sample.blue - blue) < 35);
}

double audio_rms(const std::filesystem::path& path, double start, double end) {
  media::ffmpeg::FFmpegAudioDecodeSession decoder{path, 1};
  constexpr int sample_rate = 48'000;
  const std::size_t begin = static_cast<std::size_t>(start * sample_rate);
  const std::size_t finish = static_cast<std::size_t>(end * sample_rate);
  std::size_t cursor = 0;
  double energy = 0.0;
  std::size_t count = 0;
  while (const auto block = decoder.decode_next()) {
    const auto& samples = block->samples();
    const std::size_t channels = static_cast<std::size_t>(block->channel_count());
    for (std::size_t frame = 0; frame < block->frame_count(); ++frame) {
      const auto absolute = cursor + frame;
      if (absolute >= begin && absolute < finish) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
          const double value = samples[frame * channels + channel];
          energy += value * value;
          ++count;
        }
      }
    }
    cursor += block->frame_count();
  }
  assert(count != 0);
  return std::sqrt(energy / static_cast<double>(count));
}

int codec_profile(const std::filesystem::path& path, AVMediaType type) {
  AVFormatContext* format = nullptr;
  assert(avformat_open_input(&format, path.c_str(), nullptr, nullptr) == 0);
  assert(avformat_find_stream_info(format, nullptr) >= 0);
  const int stream_index = av_find_best_stream(format, type, -1, -1, nullptr, 0);
  assert(stream_index >= 0);
  const int profile = format->streams[stream_index]->codecpar->profile;
  avformat_close_input(&format);
  return profile;
}

struct BuiltProject {
  application::ProjectDocument document;
  domain::ClipId split_right;
};

BuiltProject make_reference_project(const std::filesystem::path& a,
                                    const std::filesystem::path& b) {
  media::ffmpeg::FFmpegMediaProbe probe;
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto asset_a = processor.import_asset(a, probe);
  const auto asset_b = processor.import_asset(b, probe);
  const auto sound = processor.import_asset("pulsar-sound:click_soft", BuiltinSoundProbe{});

  const auto first = processor.add_clip(asset_a, {domain::MediaTime::zero(), seconds(3)},
                                        domain::MediaTime::zero());
  processor.trim_clip_end(first, seconds(2));
  const auto right = processor.split_clip(first, seconds(1));
  processor.move_clip(right, document.video_track_id(), seconds(3));
  (void)processor.add_clip(asset_b, {seconds(1), seconds(2)}, seconds(2));

  const auto audio_track = processor.add_track(domain::TrackKind::Audio);
  const auto independent = processor.add_clip(audio_track, asset_b,
                                               {domain::MediaTime::zero(), seconds(1)},
                                               domain::MediaTime::zero());
  processor.set_clip_audio_gain(independent, -6.0);
  const auto muted = processor.add_clip(audio_track, asset_b,
                                        {seconds(1), seconds(2)}, seconds(1));
  processor.set_clip_muted(muted, true);
  const domain::MediaTime sound_duration{3, domain::Rational{1, 25}};
  (void)processor.add_clip(audio_track, sound, {domain::MediaTime::zero(), sound_duration},
                           domain::MediaTime{5, domain::Rational{1, 2}});
  return BuiltProject{std::move(document), right};
}

void export_reference_project_and_inspect() {
  const auto root = std::filesystem::temp_directory_path() / "pulsar-e6-reference";
  std::filesystem::create_directories(root);
  const auto a = std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/e6_video_a.mp4";
  const auto b = std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/e6_video_b.mp4";
  const auto destination = root / "reference.mp4";
  const auto project_path = root / "reference.pulsar";
  std::filesystem::remove(destination);
  std::filesystem::remove(project_path);

  auto project = make_reference_project(a, b);
  const auto original_snapshot = project.document.snapshot();
  project_file::FileProjectRepository repository;
  const auto original_cwd = std::filesystem::current_path();
  std::filesystem::current_path(PULSAR_SOURCE_DIR);
  const auto save_result = repository.save(project_path, original_snapshot);
  auto restored_state = repository.load(project_path);
  std::filesystem::current_path(original_cwd);
  assert(save_result.status == application::ProjectSaveStatus::Durable);
  auto restored = application::ProjectDocument::restore(restored_state);
  const auto snapshot = restored.snapshot();
  // Mutating the editor after the snapshot must not affect the worker input.
  application::CommandProcessor mutation{restored};
  mutation.move_clip(project.split_right, restored.video_track_id(), seconds(4));
  assert(snapshot.document_generation != restored.document_generation());

  const auto plan = exporting::ExportPlan::build(snapshot);
  assert(plan.output_duration() == seconds(4));
  media::ffmpeg::FFmpegOfflineVideoSourceFactory video_sources;
  media::ffmpeg::FFmpegOfflineAudioSourceFactory audio_sources(
      [](const std::filesystem::path& logical) {
        const auto value = logical.generic_string();
        if (value.rfind("pulsar-sound:", 0) == 0) {
          return std::filesystem::path{PULSAR_SOURCE_DIR} / "assets/sounds" /
                 (value.substr(std::string{"pulsar-sound:"}.size()) + ".wav");
        }
        return logical;
      });
  WriterFactory writers;
  project_file::FilesystemExportOutputPublisher publisher;
  exporting::ExportJob job{video_sources, audio_sources, writers, publisher};
  const auto sentinel = std::string{"old destination"};
  { std::ofstream old{destination}; old << "old destination"; }
  std::vector<double> progress;
  const auto result = job.run({snapshot, destination, exporting::ExportProfile::beta_v01()}, {},
                              [&](const exporting::ExportProgress& value) {
                                progress.push_back(value.fraction);
                              });
  assert(result.progress.fraction == 1.0);
  assert_monotonic_progress(progress);
  assert(std::filesystem::exists(destination));
  assert(std::filesystem::file_size(destination) > std::string{"old destination"}.size());
  assert(codec_profile(destination, AVMEDIA_TYPE_AUDIO) == FF_PROFILE_AAC_LOW);
  assert(codec_profile(destination, AVMEDIA_TYPE_VIDEO) == FF_PROFILE_H264_HIGH);

  media::ffmpeg::FFmpegMediaProbe probe;
  const auto output = probe.probe(destination);
  assert(output.audio_streams.size() == 1 && output.video_streams.size() == 1);
  const domain::Rational audio_time_base{1, 48'000};
  assert(output.audio_streams.front().time_base == audio_time_base);
  assert(output.audio_streams.front().duration == plan.output_duration());
  const domain::FrameRate output_rate{25, 1};
  assert(output.video_streams.front().nominal_frame_rate.has_value());
  assert(output.video_streams.front().nominal_frame_rate->rational() == output_rate.rational());

  const auto frames = decode_video_samples(destination);
  assert_colour(nearest(frames, 0.5), 255, 0, 0);
  assert_colour(nearest(frames, 1.5), 0, 0, 0);
  assert_colour(nearest(frames, 2.5), 255, 0, 255);
  assert_colour(nearest(frames, 3.5), 0, 128, 0);
  assert(audio_rms(destination, 1.05, 1.35) < 0.02);
  assert(audio_rms(destination, 0.25, 0.75) > 0.01);
  assert(audio_rms(destination, 2.25, 2.75) > 0.01);

  // A real pipeline cancellation is requested from the first progress unit;
  // no wall-clock sleep or UI thread is involved.
  { std::ofstream old{destination}; old << sentinel; }
  bool request_cancel = false;
  bool cancelled = false;
  exporting::ExportJob cancel_job{video_sources, audio_sources, writers, publisher};
  try {
    (void)cancel_job.run({snapshot, destination, exporting::ExportProfile::beta_v01()},
                         [&] { return request_cancel; },
                         [&](const exporting::ExportProgress& value) {
                           if (value.video_frames_done >= 1) request_cancel = true;
                         });
  } catch (const exporting::ExportError& error) {
    cancelled = error.code() == exporting::ExportErrorCode::Cancelled;
  }
  assert(cancelled);
  std::ifstream cancelled_destination{destination};
  const std::string cancelled_bytes{std::istreambuf_iterator<char>{cancelled_destination}, {}};
  assert(cancelled_bytes == sentinel);
  for (const auto& entry : std::filesystem::directory_iterator(root)) {
    assert(entry.path().filename().string().find("reference.mp4.pulsar-") == std::string::npos);
  }

  { std::ofstream old{destination}; old << sentinel; }
  auto missing = snapshot;
  missing.assets.front() = application::MediaAsset{
      missing.assets.front().id(), missing.assets.front().source_locator(),
      missing.assets.front().video_stream_index(), missing.assets.front().playable_duration(),
      missing.assets.front().metadata(), application::MediaAvailability::Unavailable,
      std::string{"fixture deliberately unavailable"}, missing.assets.front().audio()};
  exporting::ExportJob missing_job{video_sources, audio_sources, writers, publisher};
  bool missing_media = false;
  try {
    (void)missing_job.run({missing, destination, exporting::ExportProfile::beta_v01()});
  } catch (const exporting::ExportError& error) {
    missing_media = error.code() == exporting::ExportErrorCode::MissingMedia;
  }
  assert(missing_media);
  std::ifstream unchanged{destination};
  const std::string preserved{std::istreambuf_iterator<char>{unchanged}, {}};
  assert(preserved == sentinel);
  std::filesystem::remove(destination);
  std::filesystem::remove(project_path);
  std::filesystem::remove_all(root);
}
} // namespace

int main() {
  export_reference_project_and_inspect();
}
