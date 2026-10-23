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
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
using namespace pulsar;
namespace exporting = application::exporting;

std::filesystem::path isolated_root(const char* name) {
  static const auto run_id = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() / (std::string{name} + "-" + run_id);
}

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
  assert(avformat_open_input(&format, path.string().c_str(), nullptr, nullptr) == 0);
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

media::ffmpeg::FFmpegOfflineAudioSourceFactory::LocatorResolver sound_resolver() {
  return [](const std::filesystem::path& logical) {
    const auto value = logical.generic_string();
    if (value.rfind("pulsar-sound:", 0) == 0) {
      return std::filesystem::path{PULSAR_SOURCE_DIR} / "assets/sounds" /
             (value.substr(std::string{"pulsar-sound:"}.size()) + ".wav");
    }
    return logical;
  };
}

void export_snapshot(const application::ProjectSnapshot& snapshot,
                     const std::filesystem::path& destination) {
  media::ffmpeg::FFmpegOfflineVideoSourceFactory video_sources;
  media::ffmpeg::FFmpegOfflineAudioSourceFactory audio_sources{sound_resolver()};
  WriterFactory writers;
  project_file::FilesystemExportOutputPublisher publisher;
  exporting::ExportJob job{video_sources, audio_sources, writers, publisher};
  (void)job.run({snapshot, destination, exporting::ExportProfile::beta_v01()});
}

int codec_profile(const std::filesystem::path& path, AVMediaType type) {
  AVFormatContext* format = nullptr;
  assert(avformat_open_input(&format, path.string().c_str(), nullptr, nullptr) == 0);
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
  const auto sfx_track = processor.add_track(domain::TrackKind::Audio);
  (void)processor.add_clip(sfx_track, sound, {domain::MediaTime::zero(), sound_duration},
                           seconds(1));
  return BuiltProject{std::move(document), right};
}

application::ProjectSnapshot make_gain_project(const std::filesystem::path& source) {
  media::ffmpeg::FFmpegMediaProbe probe;
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto asset = processor.import_asset(source, probe);
  const auto video = processor.add_clip(asset, {seconds(0), seconds(3)}, seconds(0));
  processor.set_clip_muted(video, true);
  const auto audio = processor.add_track(domain::TrackKind::Audio);
  const auto zero = processor.add_clip(audio, asset, {seconds(0), seconds(1)}, seconds(0));
  const auto minus = processor.add_clip(audio, asset, {seconds(0), seconds(1)}, seconds(1));
  const auto plus = processor.add_clip(audio, asset, {seconds(0), seconds(1)}, seconds(2));
  processor.set_clip_audio_gain(minus, -6.0);
  processor.set_clip_audio_gain(plus, 6.0);
  (void)zero;
  return document.snapshot();
}

application::ProjectSnapshot make_overlap_project(const std::filesystem::path& a,
                                                  const std::filesystem::path& b) {
  media::ffmpeg::FFmpegMediaProbe probe;
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto asset_a = processor.import_asset(a, probe);
  const auto asset_b = processor.import_asset(b, probe);
  const auto video = processor.add_clip(asset_a, {seconds(0), seconds(2)}, seconds(0));
  processor.set_clip_muted(video, true);
  const auto first = processor.add_track(domain::TrackKind::Audio);
  const auto second = processor.add_track(domain::TrackKind::Audio);
  (void)processor.add_clip(first, asset_a, {seconds(0), seconds(2)}, seconds(0));
  (void)processor.add_clip(second, asset_b, {seconds(0), seconds(2)}, seconds(1));
  return document.snapshot();
}

application::ProjectSnapshot make_isolated_sound_project(const std::filesystem::path& source) {
  media::ffmpeg::FFmpegMediaProbe probe;
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto video_asset = processor.import_asset(source, probe);
  const auto sound = processor.import_asset("pulsar-sound:click_soft", BuiltinSoundProbe{});
  const auto video = processor.add_clip(video_asset, {seconds(0), seconds(3)}, seconds(0));
  processor.set_clip_muted(video, true);
  const auto track = processor.add_track(domain::TrackKind::Audio);
  const domain::MediaTime sound_duration{3, domain::Rational{1, 25}};
  (void)processor.add_clip(track, sound, {domain::MediaTime::zero(), sound_duration}, seconds(1));
  return document.snapshot();
}

void gain_evidence(const std::filesystem::path& source, const std::filesystem::path& root) {
  const auto destination = root / "gain.mp4";
  export_snapshot(make_gain_project(source), destination);
  const double rms_zero = audio_rms(destination, 0.20, 0.80);
  const double rms_minus = audio_rms(destination, 1.20, 1.80);
  const double rms_plus = audio_rms(destination, 2.20, 2.80);
  const double minus_ratio = rms_minus / rms_zero;
  const double plus_ratio = rms_plus / rms_zero;
  std::fprintf(stderr, "E6 gain ratios: -6/0=%.3f +6/0=%.3f\n", minus_ratio, plus_ratio);
  assert(std::abs(minus_ratio - 0.501) < 0.10);
  assert(std::abs(plus_ratio - 1.995) < 0.20);
  std::filesystem::remove(destination);
}

void overlap_evidence(const std::filesystem::path& a, const std::filesystem::path& b,
                      const std::filesystem::path& root) {
  const auto destination = root / "overlap.mp4";
  export_snapshot(make_overlap_project(a, b), destination);
  const double solo = audio_rms(destination, 0.20, 0.80);
  const double overlap = audio_rms(destination, 1.20, 1.80);
  const double ratio = overlap / solo;
  std::fprintf(stderr, "E6 overlap RMS: solo=%.4f overlap=%.4f ratio=%.3f\n", solo, overlap,
               ratio);
  assert(overlap > solo * 1.20);
  assert(overlap < solo * 2.50);
  std::filesystem::remove(destination);
}

void sound_and_sync_evidence(const std::filesystem::path& source,
                             const std::filesystem::path& root) {
  const auto destination = root / "isolated-sound.mp4";
  export_snapshot(make_isolated_sound_project(source), destination);
  const double before = audio_rms(destination, 0.70, 0.90);
  const double during = audio_rms(destination, 1.01, 1.11);
  const double after = audio_rms(destination, 1.22, 1.45);
  std::fprintf(stderr, "E6 Core Sound RMS: before=%.4f during=%.4f after=%.4f\n", before,
               during, after);
  assert(before < 0.02);
  assert(during > before * 3.0);
  assert(after < 0.02);

  const auto frames = decode_video_samples(destination);
  const auto visual_event = nearest(frames, 1.0);
  assert_colour(visual_event, 0, 128, 0);
  double event_time = 0.0;
  double event_rms = 0.0;
  for (double cursor = 0.50; cursor < 1.50; cursor += 0.02) {
    const double value = audio_rms(destination, cursor, cursor + 0.02);
    if (value > event_rms) {
      event_rms = value;
      event_time = cursor + 0.01;
    }
  }
  const double sync_delta = std::abs(event_time - 1.0);
  std::fprintf(stderr, "E6 A/V sync: visual=%.3f audio=%.3f delta=%.3f\n",
               visual_event.timestamp, event_time, sync_delta);
  assert(sync_delta <= 0.08);
  std::filesystem::remove(destination);
}

void rational_fps_evidence(const std::filesystem::path& source,
                           const std::filesystem::path& root) {
  media::ffmpeg::FFmpegMediaProbe probe;
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto asset = processor.import_asset(source, probe);
  (void)processor.add_clip(asset, {seconds(0), seconds(4)}, seconds(0));
  const auto snapshot = document.snapshot();
  const auto plan = exporting::ExportPlan::build(snapshot);
  const auto destination = root / "rational.mp4";
  export_snapshot(snapshot, destination);
  const auto output = probe.probe(destination);
  assert(output.video_streams.size() == 1);
  assert(output.video_streams.front().nominal_frame_rate.has_value());
  const auto expected_rate = domain::FrameRate{30'000, 1001};
  assert(output.video_streams.front().nominal_frame_rate->rational() ==
         expected_rate.rational());
  assert(output.video_streams.front().duration == plan.output_duration());
  assert(output.audio_streams.size() == 1);
  const auto frames = decode_video_samples(destination);
  assert_colour(nearest(frames, 0.5), 0, 255, 255);
  assert_colour(nearest(frames, 1.5), 255, 0, 255);
  std::fprintf(stderr, "E6 rational FPS: 30000/1001 duration=%.3f s\n",
               static_cast<double>(plan.output_duration()));
  std::filesystem::remove(destination);
}

void export_reference_project_and_inspect() {
  const auto root = isolated_root("pulsar-e6-reference");
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
  media::ffmpeg::FFmpegOfflineAudioSourceFactory audio_sources{sound_resolver()};
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
  assert(codec_profile(destination, AVMEDIA_TYPE_AUDIO) == AV_PROFILE_AAC_LOW);
  assert(codec_profile(destination, AVMEDIA_TYPE_VIDEO) == AV_PROFILE_H264_HIGH);

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
  const auto before_one = nearest(frames, 0.96);
  const auto at_one = nearest(frames, 1.00);
  const auto before_two = nearest(frames, 1.96);
  const auto at_two = nearest(frames, 2.00);
  const auto before_three = nearest(frames, 2.96);
  const auto at_three = nearest(frames, 3.00);
  assert_colour(before_one, 255, 0, 0);
  assert_colour(at_one, 0, 0, 0);
  assert_colour(before_two, 0, 0, 0);
  assert_colour(at_two, 255, 0, 255);
  assert_colour(before_three, 255, 0, 255);
  assert_colour(at_three, 0, 128, 0);
  assert(std::abs(at_one.timestamp - 1.0) < 0.001);
  assert(std::abs(at_two.timestamp - 2.0) < 0.001);
  assert(std::abs(at_three.timestamp - 3.0) < 0.001);
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
  gain_evidence(a, root);
  overlap_evidence(a, b, root);
  sound_and_sync_evidence(a, root);
  rational_fps_evidence(std::filesystem::path{PULSAR_SOURCE_DIR} /
                           "tests/fixtures/e6_video_rational.mp4",
                       root);
  std::filesystem::remove(destination);
  std::filesystem::remove(project_path);
  std::filesystem::remove_all(root);
}
// Full ExportJob (H.264/AAC/MP4) over a 44.1 kHz source whose clip starts at
// 0.127 s, off the physical sample grid. Regression for RenderFailed with
// "audio sample request escaped source range".
void export_unaligned_source_range_succeeds() {
  const auto root = isolated_root("pulsar-e7-unaligned-audio");
  std::filesystem::create_directories(root);
  const auto destination = root / "unaligned.mp4";
  std::filesystem::remove(destination);
  const auto fixture =
      std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/audio_decode_fixture.mp4";

  media::ffmpeg::FFmpegMediaProbe probe;
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto media = processor.import_asset(fixture, probe);
  (void)processor.add_clip(
      media, domain::TimeRange{domain::MediaTime{127, domain::Rational{1, 1000}},
                               domain::MediaTime{377, domain::Rational{1, 1000}}},
      domain::MediaTime::zero());
  export_snapshot(document.snapshot(), destination);

  assert(std::filesystem::exists(destination));
  assert(codec_profile(destination, AVMEDIA_TYPE_AUDIO) == AV_PROFILE_AAC_LOW);
  const auto output = probe.probe(destination);
  assert(!output.audio_streams.empty() && output.audio_streams.front().sample_rate == 48'000);
  assert(!output.video_streams.empty());
  std::filesystem::remove_all(root);
}
// "Separar audio" must not change the exported sound: the same project exported
// before and after the separation yields the same audio (the embedded track is
// muted and the independent clip carries identical timeline/source ranges).
void export_after_separate_audio_matches_original() {
  const auto root = isolated_root("pulsar-separate-audio-e2e");
  std::filesystem::create_directories(root);
  const auto source = std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/e6_video_a.mp4";
  media::ffmpeg::FFmpegMediaProbe probe;
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto media = processor.import_asset(source, probe);
  // A trimmed clip placed after a gap: the separated audio must follow the clip.
  const auto clip = processor.add_clip(media, domain::TimeRange{domain::MediaTime{1, domain::Rational{1, 2}}, seconds(3)},
                                       domain::MediaTime{1, domain::Rational{1, 2}});
  const auto original_snapshot = document.snapshot();
  const auto original_path = root / "original.mp4";
  const auto separated_path = root / "separated.mp4";
  std::filesystem::remove(original_path);
  std::filesystem::remove(separated_path);
  export_snapshot(original_snapshot, original_path);

  // Same operation the editor performs (one grouped edit).
  processor.run_as_one_edit([&] {
    const auto audio_track = processor.add_track(domain::TrackKind::Audio);
    const auto* source_clip = document.timeline().find_clip(clip);
    (void)processor.add_clip(audio_track, media, source_clip->source_range(), source_clip->timeline_start());
    processor.set_clip_muted(clip, true);
  });
  export_snapshot(document.snapshot(), separated_path);

  assert(std::filesystem::exists(original_path) && std::filesystem::exists(separated_path));
  media::ffmpeg::FFmpegMediaProbe output_probe;
  const auto a = output_probe.probe(original_path);
  const auto b = output_probe.probe(separated_path);
  assert(!b.audio_streams.empty() && b.audio_streams.front().sample_rate == 48'000);
  assert(a.duration.has_value() && b.duration.has_value() && *a.duration == *b.duration);
  for (const auto& [begin, end] : {std::pair{0.60, 1.20}, std::pair{1.50, 2.30}, std::pair{2.60, 3.30}}) {
    const double before = audio_rms(original_path, begin, end);
    const double after = audio_rms(separated_path, begin, end);
    assert(before > 1.0e-3);                              // there is real sound to compare
    assert(std::abs(before - after) <= before * 1.0e-3);  // and it is the same sound
  }
  // The gap before the clip stays silent in both.
  assert(audio_rms(separated_path, 0.05, 0.40) < 1.0e-4);
  std::filesystem::remove_all(root);
}
} // namespace

int main() {
  export_reference_project_and_inspect();
  export_unaligned_source_range_succeeds();
  export_after_separate_audio_matches_original();
}
