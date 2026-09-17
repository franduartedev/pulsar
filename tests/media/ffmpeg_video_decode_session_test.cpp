#include <pulsar/application/media/media_probe_error.hpp>
#include <pulsar/application/playback/monotonic_clock.hpp>
#include <pulsar/application/playback/playback_session.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>
#include <pulsar/media/ffmpeg/decoded_visual_video_frame.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_playback_worker.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>
#include <pulsar/media/ffmpeg/media_decode_error.hpp>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavformat/avformat.h>
}

#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <thread>
#include <variant>
#include <vector>

namespace {

using pulsar::application::MediaProbeError;
using pulsar::application::MediaProbeErrorCode;
using pulsar::application::MonotonicClock;
using pulsar::application::MonotonicInstant;
using pulsar::application::PlaybackAvailability;
using pulsar::application::PlaybackPosition;
using pulsar::application::PlaybackSession;
using pulsar::application::PresentSample;
using pulsar::application::PresentationDecision;
using pulsar::application::StreamTimestamp;
using pulsar::media::ffmpeg::DecodedVisualVideoFrame;
using pulsar::media::ffmpeg::FFmpegPlaybackWorker;
using pulsar::media::ffmpeg::FFmpegVideoDecodeSession;
using pulsar::media::ffmpeg::MediaDecodeError;
using pulsar::media::ffmpeg::MediaDecodeErrorCode;

struct NonZeroStartFixtureInfo {
  std::filesystem::path path;
  std::string container;
  std::string codec;
  int width;
  int height;
  pulsar::domain::Rational stream_time_base;
  std::int64_t raw_start_time;
  pulsar::domain::MediaTime stream_start_time;
  std::vector<pulsar::domain::MediaTime> timestamps;
};

struct TestFormatContextDeleter {
  void operator()(AVFormatContext* context) const noexcept {
    if (context != nullptr) {
      avformat_close_input(&context);
    }
  }
};

class FakeMonotonicClock final : public MonotonicClock {
public:
  [[nodiscard]] MonotonicInstant now() const override {
    return now_;
  }

  void advance(pulsar::domain::MediaTime delta) {
    now_ = MonotonicInstant{now_.value() + delta};
  }

private:
  MonotonicInstant now_{MonotonicInstant::zero()};
};

int fail(std::string_view message) {
  std::cerr << "ffmpeg video decode session test failed: " << message << '\n';
  return 1;
}

template <typename Callable>
bool throws_decode_error(Callable callable, MediaDecodeErrorCode expected_code) {
  try {
    callable();
  } catch (const MediaDecodeError& error) {
    return error.code() == expected_code;
  } catch (...) {
    return false;
  }

  return false;
}

template <typename Callable>
bool throws_probe_error(Callable callable, MediaProbeErrorCode expected_code) {
  try {
    callable();
  } catch (const MediaProbeError& error) {
    return error.code() == expected_code;
  } catch (...) {
    return false;
  }

  return false;
}

void write_u16_le(std::ostream& output, std::uint16_t value) {
  output.put(static_cast<char>(value & 0xffU));
  output.put(static_cast<char>((value >> 8U) & 0xffU));
}

void write_u32_le(std::ostream& output, std::uint32_t value) {
  output.put(static_cast<char>(value & 0xffU));
  output.put(static_cast<char>((value >> 8U) & 0xffU));
  output.put(static_cast<char>((value >> 16U) & 0xffU));
  output.put(static_cast<char>((value >> 24U) & 0xffU));
}

void write_text(std::ostream& output, std::string_view text) {
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

void write_wav_fixture(const std::filesystem::path& path) {
  constexpr std::uint16_t channels = 2;
  constexpr std::uint32_t sample_rate = 48'000;
  constexpr std::uint16_t bits_per_sample = 16;
  constexpr std::uint32_t frames = 480;
  constexpr std::uint16_t block_align = channels * bits_per_sample / 8;
  constexpr std::uint32_t byte_rate = sample_rate * block_align;
  constexpr std::uint32_t data_size = frames * block_align;
  constexpr std::uint32_t riff_size = 36 + data_size;

  std::ofstream output{path, std::ios::binary};
  write_text(output, "RIFF");
  write_u32_le(output, riff_size);
  write_text(output, "WAVE");
  write_text(output, "fmt ");
  write_u32_le(output, 16);
  write_u16_le(output, 1);
  write_u16_le(output, channels);
  write_u32_le(output, sample_rate);
  write_u32_le(output, byte_rate);
  write_u16_le(output, block_align);
  write_u16_le(output, bits_per_sample);
  write_text(output, "data");
  write_u32_le(output, data_size);

  for (std::uint32_t index = 0; index < data_size; ++index) {
    output.put('\0');
  }
}

void write_ppm_fixture(const std::filesystem::path& path) {
  constexpr int width = 16;
  constexpr int height = 16;

  std::ofstream output{path, std::ios::binary};
  output << "P6\n" << width << ' ' << height << "\n255\n";
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const auto red = static_cast<char>((x * 255) / (width - 1));
      const auto green = static_cast<char>((y * 255) / (height - 1));
      const char blue = static_cast<char>(((x + y) % 2) == 0 ? 32 : 220);
      output.put(red);
      output.put(green);
      output.put(blue);
    }
  }
}

std::string shell_quote(const std::filesystem::path& path) {
  std::string quoted{"'"};
  for (const char character : path.string()) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }
  quoted += "'";
  return quoted;
}

std::filesystem::path fixture_root() {
  const auto run_id = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = std::filesystem::temp_directory_path() / "pulsar-ffmpeg-decode-tests" /
                    ("run-" + std::to_string(run_id));
  std::filesystem::create_directories(root);
  return root;
}

void write_non_zero_start_h264_fixture(const std::filesystem::path& path) {
  const std::string encoder_check_command =
      shell_quote(PULSAR_TEST_FFMPEG_CLI) +
      " -hide_banner -loglevel error -h encoder=libx264 >/dev/null 2>&1";
  if (std::system(encoder_check_command.c_str()) != 0) {
    throw std::runtime_error{
        "ffmpeg CLI is available, but the required libx264 encoder is not"};
  }

  const std::string command =
      shell_quote(PULSAR_TEST_FFMPEG_CLI) +
      " -y -hide_banner -loglevel error -f lavfi "
      "-i testsrc2=size=320x180:rate=25:duration=4 "
      "-vf setpts=PTS+5/TB "
      "-c:v libx264 -pix_fmt yuv420p -bf 2 -g 25 -keyint_min 25 "
      "-sc_threshold 0 -preset veryfast -f matroska " +
      shell_quote(path);

  if (std::system(command.c_str()) != 0) {
    throw std::runtime_error{
        "ffmpeg CLI could not create the required non-zero start-time H.264/B-frame fixture"};
  }
}

NonZeroStartFixtureInfo inspect_non_zero_start_fixture(const std::filesystem::path& path) {
  AVFormatContext* raw_context = nullptr;
  const std::string path_text = path.string();
  if (avformat_open_input(&raw_context, path_text.c_str(), nullptr, nullptr) < 0) {
    throw std::runtime_error{"could not open generated non-zero start-time fixture"};
  }

  std::unique_ptr<AVFormatContext, TestFormatContextDeleter> context{raw_context};
  if (avformat_find_stream_info(context.get(), nullptr) < 0) {
    throw std::runtime_error{"could not read generated fixture stream info"};
  }
  if (context->nb_streams == 0 || context->streams[0] == nullptr ||
      context->streams[0]->codecpar == nullptr) {
    throw std::runtime_error{"generated fixture has no inspectable video stream"};
  }

  const AVStream& stream = *context->streams[0];
  const AVCodecParameters& codec_parameters = *stream.codecpar;
  if (codec_parameters.codec_type != AVMEDIA_TYPE_VIDEO) {
    throw std::runtime_error{"generated fixture stream is not video"};
  }

  const pulsar::domain::Rational time_base{stream.time_base.num, stream.time_base.den};
  if (stream.start_time == AV_NOPTS_VALUE || stream.start_time == 0) {
    throw std::runtime_error{"generated fixture did not preserve non-zero AVStream::start_time"};
  }

  auto session = FFmpegVideoDecodeSession{path, 0};
  std::vector<pulsar::domain::MediaTime> timestamps;
  while (auto frame = session.decode_next()) {
    if (frame->presentation_timestamp.has_value()) {
      timestamps.push_back(*frame->presentation_timestamp);
    }
  }
  if (timestamps.size() != 100) {
    throw std::runtime_error{
        "generated fixture did not decode the expected 100 timestamped frames"};
  }

  return NonZeroStartFixtureInfo{
      path,
      context->iformat != nullptr && context->iformat->name != nullptr
          ? std::string{context->iformat->name}
          : std::string{"unknown"},
      std::string{avcodec_get_name(codec_parameters.codec_id)},
      codec_parameters.width,
      codec_parameters.height,
      time_base,
      stream.start_time,
      pulsar::domain::MediaTime{stream.start_time, time_base},
      std::move(timestamps),
  };
}

pulsar::domain::MediaTime one_stream_tick_after(pulsar::domain::MediaTime time) {
  return pulsar::domain::MediaTime{time.value() + 1, time.time_base()};
}

pulsar::domain::MediaTime one_stream_tick_before(pulsar::domain::MediaTime time) {
  return pulsar::domain::MediaTime{time.value() - 1, time.time_base()};
}

bool seek_returns_timestamp(const std::filesystem::path& path,
                            pulsar::domain::MediaTime target,
                            pulsar::domain::MediaTime expected_timestamp) {
  auto session = FFmpegVideoDecodeSession{path, 0};
  const auto result = session.seek_to_stream_timestamp(target);
  return result.frame.has_value() && result.frame->presentation_timestamp.has_value() &&
         *result.frame->presentation_timestamp == expected_timestamp;
}

bool presentation_seek_returns_timestamp(const std::filesystem::path& path,
                                         pulsar::domain::MediaTime target,
                                         pulsar::domain::MediaTime expected_timestamp,
                                         std::optional<pulsar::domain::MediaTime> expected_next) {
  auto session = FFmpegVideoDecodeSession{path, 0};
  const auto result = session.seek_to_presentation_timestamp(target);
  if (!result.frame.has_value() || !result.frame->presentation_timestamp.has_value() ||
      *result.frame->presentation_timestamp != expected_timestamp) {
    return false;
  }

  const auto next = session.decode_next();
  if (!expected_next.has_value()) {
    return !next.has_value();
  }
  return next.has_value() && next->presentation_timestamp.has_value() &&
         *next->presentation_timestamp == *expected_next;
}

template <typename EventType>
std::optional<EventType> wait_for_worker_event(FFmpegPlaybackWorker& worker) {
  for (int attempt = 0; attempt < 100000; ++attempt) {
    if (auto event = worker.poll_event()) {
      if (auto* typed = std::get_if<EventType>(&*event); typed != nullptr) {
        return std::move(*typed);
      }
      return std::nullopt;
    }
    std::this_thread::yield();
  }
  return std::nullopt;
}

std::optional<pulsar::domain::MediaTime> wait_for_session_presentation(
    PlaybackSession& session) {
  for (int attempt = 0; attempt < 100000; ++attempt) {
    const PresentationDecision decision = session.update();
    if (const auto* present = std::get_if<PresentSample>(&decision); present != nullptr &&
        present->sample.presentation_timestamp.has_value()) {
      return present->sample.presentation_timestamp->value();
    }
    std::this_thread::yield();
  }
  return std::nullopt;
}

pulsar::domain::MediaTime playback_position_for_stream_time(pulsar::domain::MediaTime origin,
                                                            pulsar::domain::MediaTime stream_time) {
  return stream_time - origin;
}

bool ppm_decode_test(const std::filesystem::path& root) {
  const auto ppm_path = root / "tiny visual fixture.ppm";
  write_ppm_fixture(ppm_path);

  auto session = FFmpegVideoDecodeSession{ppm_path, 0};
  const auto frame = session.decode_next();
  if (!frame.has_value()) {
    return false;
  }
  if (frame->stream_index != 0 || frame->width != 16 || frame->height != 16) {
    return false;
  }
  if (!frame->pixel_format_name.has_value() || frame->pixel_format_name->empty()) {
    return false;
  }
  if (frame->presentation_timestamp.has_value() &&
      frame->presentation_timestamp->seconds() < pulsar::domain::Rational{0, 1}) {
    return false;
  }

  if (session.decode_next().has_value()) {
    return false;
  }
  return !session.decode_next().has_value();
}

bool visual_frame_lease_test(const std::filesystem::path& root) {
  const auto ppm_path = root / "visual lease fixture.ppm";
  write_ppm_fixture(ppm_path);

  pulsar::media::ffmpeg::FFmpegVideoFrameLease retained_lease;
  {
    auto session = FFmpegVideoDecodeSession{ppm_path, 0};
    const auto frame = session.decode_next_visual();
    if (!frame.has_value()) {
      return false;
    }
    if (frame->stream_index != 0 || !frame->frame.valid()) {
      return false;
    }
    if (frame->image.coded_width != 16 || frame->image.coded_height != 16 ||
        frame->image.visible_width != 16 || frame->image.visible_height != 16) {
      return false;
    }
    if (!frame->image.pixel_format_name.has_value() || frame->image.pixel_format_name->empty()) {
      return false;
    }
    if (frame->image.color_range.empty() || frame->image.color_primaries.empty() ||
        frame->image.color_transfer.empty() || frame->image.color_matrix.empty()) {
      return false;
    }
    if (frame->presentation_timestamp.has_value() &&
        frame->presentation_timestamp->seconds() < pulsar::domain::Rational{0, 1}) {
      return false;
    }

    retained_lease = frame->frame;
    if (session.decode_next_visual().has_value()) {
      return false;
    }
    if (!retained_lease.valid()) {
      return false;
    }
  }

  return retained_lease.valid();
}

bool visual_frame_timestamp_alignment_test(const std::filesystem::path& root) {
  const auto fixture_path = root / "visual-timestamp-h264.mkv";
  write_non_zero_start_h264_fixture(fixture_path);
  const auto fixture = inspect_non_zero_start_fixture(fixture_path);

  auto session = FFmpegVideoDecodeSession{fixture.path, 0};
  const auto frame = session.decode_next_visual();
  if (!frame.has_value() || !frame->frame.valid()) {
    return false;
  }
  if (!frame->presentation_timestamp.has_value() ||
      *frame->presentation_timestamp != fixture.timestamps.front()) {
    return false;
  }
  if (frame->image.coded_width != fixture.width || frame->image.coded_height != fixture.height ||
      frame->image.visible_width != fixture.width || frame->image.visible_height != fixture.height) {
    return false;
  }
  if (!frame->image.pixel_format_name.has_value() || *frame->image.pixel_format_name != "yuv420p") {
    return false;
  }

  DecodedVisualVideoFrame previous = *frame;
  for (int index = 1; index < 20; ++index) {
    const auto next = session.decode_next_visual();
    if (!next.has_value() || !next->frame.valid() ||
        !next->presentation_timestamp.has_value() ||
        *next->presentation_timestamp != fixture.timestamps.at(static_cast<std::size_t>(index))) {
      return false;
    }
    if (!previous.frame.valid()) {
      return false;
    }
    previous = *next;
  }

  return previous.frame.valid();
}

bool audio_only_error_test(const std::filesystem::path& root) {
  const auto wav_path = root / "tiny audio fixture.wav";
  write_wav_fixture(wav_path);

  return throws_decode_error([&] { (void)FFmpegVideoDecodeSession{wav_path, 0}; },
                             MediaDecodeErrorCode::SelectedStreamIsNotVideo);
}

bool invalid_stream_tests(const std::filesystem::path& root) {
  const auto ppm_path = root / "tiny visual fixture.ppm";
  write_ppm_fixture(ppm_path);

  if (!throws_decode_error([&] { (void)FFmpegVideoDecodeSession{ppm_path, -1}; },
                           MediaDecodeErrorCode::InvalidStreamIndex)) {
    return false;
  }
  if (!throws_decode_error([&] { (void)FFmpegVideoDecodeSession{ppm_path, 999}; },
                           MediaDecodeErrorCode::InvalidStreamIndex)) {
    return false;
  }

  const auto garbage = root / "not really media.mp4";
  {
    std::ofstream output{garbage, std::ios::binary};
    output << "this is not multimedia";
  }
  return throws_probe_error([&] { (void)FFmpegVideoDecodeSession{garbage, 0}; },
                            MediaProbeErrorCode::UnsupportedMedia);
}

bool partial_destruction_test(const std::filesystem::path& root) {
  const auto ppm_path = root / "partial destruction fixture.ppm";
  write_ppm_fixture(ppm_path);

  {
    auto session = FFmpegVideoDecodeSession{ppm_path, 0};
    if (!session.decode_next().has_value()) {
      return false;
    }
  }

  return true;
}

bool basic_seek_test(const std::filesystem::path& root) {
  const auto ppm_path = root / "seek fixture.ppm";
  write_ppm_fixture(ppm_path);

  auto session = FFmpegVideoDecodeSession{ppm_path, 0};
  const auto result =
      session.seek_to_stream_timestamp(pulsar::domain::MediaTime{0, pulsar::domain::Rational{1, 1}});
  if (!result.frame.has_value()) {
    return false;
  }
  if (result.frames_decoded_after_seek != 1) {
    return false;
  }
  return result.frame->width == 16 && result.frame->height == 16;
}

bool eof_then_seek_test(const std::filesystem::path& root) {
  const auto ppm_path = root / "eof seek fixture.ppm";
  write_ppm_fixture(ppm_path);

  auto session = FFmpegVideoDecodeSession{ppm_path, 0};
  while (session.decode_next().has_value()) {
  }
  if (session.decode_next().has_value()) {
    return false;
  }

  const auto result =
      session.seek_to_stream_timestamp(pulsar::domain::MediaTime{0, pulsar::domain::Rational{1, 1}});
  return result.frame.has_value() && !session.decode_next().has_value();
}

bool multiple_seek_test(const std::filesystem::path& root) {
  const auto ppm_path = root / "multiple seek fixture.ppm";
  write_ppm_fixture(ppm_path);

  auto session = FFmpegVideoDecodeSession{ppm_path, 0};
  for (int index = 0; index < 3; ++index) {
    const auto result = session.seek_to_stream_timestamp(
        pulsar::domain::MediaTime{0, pulsar::domain::Rational{1, 1}});
    if (!result.frame.has_value()) {
      return false;
    }
  }

  return true;
}

bool target_after_final_frame_test(const std::filesystem::path& root) {
  const auto ppm_path = root / "after final seek fixture.ppm";
  write_ppm_fixture(ppm_path);

  auto session = FFmpegVideoDecodeSession{ppm_path, 0};
  try {
    const auto result = session.seek_to_stream_timestamp(
        pulsar::domain::MediaTime{1, pulsar::domain::Rational{1, 1}});
    return !result.frame.has_value() && !session.decode_next().has_value();
  } catch (const MediaDecodeError& error) {
    return error.code() == MediaDecodeErrorCode::SeekFailed ||
           error.code() == MediaDecodeErrorCode::SeekUnsupported;
  }
}

bool negative_target_test(const std::filesystem::path& root) {
  const auto ppm_path = root / "negative seek fixture.ppm";
  write_ppm_fixture(ppm_path);

  auto session = FFmpegVideoDecodeSession{ppm_path, 0};
  try {
    const auto result = session.seek_to_stream_timestamp(
        pulsar::domain::MediaTime{-1, pulsar::domain::Rational{1, 1}});
    return result.frame.has_value();
  } catch (const MediaDecodeError& error) {
    return error.code() == MediaDecodeErrorCode::SeekFailed ||
           error.code() == MediaDecodeErrorCode::SeekUnsupported;
  }
}

bool invalid_seek_target_test(const std::filesystem::path& root) {
  const auto ppm_path = root / "invalid seek target fixture.ppm";
  write_ppm_fixture(ppm_path);

  auto session = FFmpegVideoDecodeSession{ppm_path, 0};
  return throws_decode_error(
      [&] {
        (void)session.seek_to_stream_timestamp(pulsar::domain::MediaTime{
            std::numeric_limits<std::int64_t>::max(),
            pulsar::domain::Rational{std::numeric_limits<std::int64_t>::max(), 1}});
      },
      MediaDecodeErrorCode::InvalidSeekTarget);
}

bool non_zero_start_time_seek_test(const std::filesystem::path& root) {
  const auto fixture_path = root / "nonzero-start-time-h264.mkv";
  write_non_zero_start_h264_fixture(fixture_path);
  const auto fixture = inspect_non_zero_start_fixture(fixture_path);

  if (fixture.container != "matroska,webm" || fixture.codec != "h264" || fixture.width != 320 ||
      fixture.height != 180 || fixture.stream_time_base != pulsar::domain::Rational{1, 1000}) {
    return false;
  }
  if (fixture.raw_start_time == 0 || fixture.stream_start_time != fixture.timestamps.front()) {
    return false;
  }

  const auto first_timestamp = fixture.timestamps.front();
  const auto exact_interior = fixture.timestamps.at(37);
  if (!seek_returns_timestamp(fixture.path, first_timestamp, first_timestamp)) {
    return false;
  }
  if (!seek_returns_timestamp(fixture.path, exact_interior, exact_interior)) {
    return false;
  }

  const auto lower_timestamp = fixture.timestamps.at(37);
  const auto upper_timestamp = fixture.timestamps.at(38);
  const auto between_frames = one_stream_tick_after(lower_timestamp);
  if (!(lower_timestamp < between_frames && between_frames < upper_timestamp)) {
    return false;
  }
  if (!seek_returns_timestamp(fixture.path, between_frames, upper_timestamp)) {
    return false;
  }
  if (!presentation_seek_returns_timestamp(fixture.path, lower_timestamp, lower_timestamp,
                                           upper_timestamp)) {
    return false;
  }
  if (!presentation_seek_returns_timestamp(fixture.path, between_frames, lower_timestamp,
                                           upper_timestamp)) {
    return false;
  }
  {
    auto worker = FFmpegPlaybackWorker{fixture.path, 0, FFmpegPlaybackWorker::Limits{4}};
    worker.request_seek_for_presentation(12, pulsar::application::StreamTimestamp{between_frames});
    const auto seek = wait_for_worker_event<pulsar::application::PlaybackWorkerSeekComplete>(worker);
    if (!seek.has_value() || seek->generation != 12) {
      return false;
    }
    const auto* hit = std::get_if<pulsar::application::VideoSeekHit>(&seek->outcome);
    if (hit == nullptr || !hit->sample.presentation_timestamp.has_value() ||
        hit->sample.presentation_timestamp->value() != lower_timestamp || !hit->lookahead.has_value() ||
        !hit->lookahead->presentation_timestamp.has_value() ||
        hit->lookahead->presentation_timestamp->value() != upper_timestamp) {
      return false;
    }

    worker.request_read(13);
    const auto sample = wait_for_worker_event<pulsar::application::PlaybackWorkerSample>(worker);
    if (!sample.has_value() || sample->generation != 13 ||
        !sample->sample.presentation_timestamp.has_value() ||
        sample->sample.presentation_timestamp->value() != upper_timestamp) {
      return false;
    }
    const auto stats = worker.stats();
    if (worker.event_queue_high_water_mark() > 4 || stats.event_queue_high_water_mark > 4 ||
        stats.seek_request_count != 1 || stats.read_request_count != 1 ||
        stats.last_seek_decoded_frames == 0 || stats.open_latency.count() <= 0 ||
        stats.last_seek_latency.count() <= 0 || stats.last_read_latency.count() <= 0) {
      return false;
    }
  }
  {
    FakeMonotonicClock clock;
    auto worker = std::make_unique<FFmpegPlaybackWorker>(fixture.path, 0,
                                                         FFmpegPlaybackWorker::Limits{8});
    PlaybackSession session{clock, std::move(worker)};
    session.open();
    const auto opened = wait_for_session_presentation(session);
    if (!opened.has_value() || *opened != first_timestamp ||
        session.availability() != PlaybackAvailability::Ready) {
      return false;
    }

    session.play();
    clock.advance(upper_timestamp - first_timestamp);
    const auto played = wait_for_session_presentation(session);
    if (!played.has_value() || *played != upper_timestamp) {
      return false;
    }

    session.seek(PlaybackPosition{playback_position_for_stream_time(first_timestamp, between_frames)});
    clock.advance(pulsar::domain::MediaTime{10, pulsar::domain::Rational{1, 1}});
    const auto sought = wait_for_session_presentation(session);
    if (!sought.has_value() || *sought != lower_timestamp) {
      return false;
    }
    clock.advance(one_stream_tick_after(lower_timestamp) - lower_timestamp);
    if (session.position().value() !=
        playback_position_for_stream_time(first_timestamp, one_stream_tick_after(between_frames))) {
      return false;
    }
  }

  const auto before_first = one_stream_tick_before(first_timestamp);
  if (!seek_returns_timestamp(fixture.path, before_first, first_timestamp)) {
    return false;
  }

  const auto near_final = fixture.timestamps.at(fixture.timestamps.size() - 3);
  {
    auto session = FFmpegVideoDecodeSession{fixture.path, 0};
    const auto result = session.seek_to_stream_timestamp(near_final);
    if (!result.frame.has_value() || !result.frame->presentation_timestamp.has_value() ||
        *result.frame->presentation_timestamp < near_final) {
      return false;
    }
  }

  const auto after_final = one_stream_tick_after(fixture.timestamps.back());
  {
    auto session = FFmpegVideoDecodeSession{fixture.path, 0};
    const auto result = session.seek_to_presentation_timestamp(after_final);
    if (!result.frame.has_value() || !result.frame->presentation_timestamp.has_value() ||
        *result.frame->presentation_timestamp != fixture.timestamps.back() ||
        session.decode_next().has_value()) {
      return false;
    }
  }
  {
    auto session = FFmpegVideoDecodeSession{fixture.path, 0};
    const auto result = session.seek_to_stream_timestamp(after_final);
    if (result.frame.has_value() || session.decode_next().has_value()) {
      return false;
    }
  }

  {
    auto session = FFmpegVideoDecodeSession{fixture.path, 0};
    const auto no_result = session.seek_to_stream_timestamp(after_final);
    if (no_result.frame.has_value()) {
      return false;
    }

    const auto valid_after_no_result = session.seek_to_stream_timestamp(exact_interior);
    if (!valid_after_no_result.frame.has_value() ||
        !valid_after_no_result.frame->presentation_timestamp.has_value() ||
        *valid_after_no_result.frame->presentation_timestamp != exact_interior) {
      return false;
    }
  }

  {
    auto session = FFmpegVideoDecodeSession{fixture.path, 0};
    while (session.decode_next().has_value()) {
    }

    const auto valid_after_eof = session.seek_to_stream_timestamp(exact_interior);
    if (!valid_after_eof.frame.has_value() ||
        !valid_after_eof.frame->presentation_timestamp.has_value() ||
        *valid_after_eof.frame->presentation_timestamp != exact_interior) {
      return false;
    }
  }

  return true;
}

} // namespace

int main() {
  const auto root = fixture_root();

  if (!ppm_decode_test(root)) {
    return fail("ppm decode test");
  }
  if (!visual_frame_lease_test(root)) {
    return fail("visual frame lease test");
  }
  if (!visual_frame_timestamp_alignment_test(root)) {
    return fail("visual frame timestamp alignment test");
  }
  if (!audio_only_error_test(root)) {
    return fail("audio-only error test");
  }
  if (!invalid_stream_tests(root)) {
    return fail("invalid stream tests");
  }
  if (!partial_destruction_test(root)) {
    return fail("partial destruction test");
  }
  if (!basic_seek_test(root)) {
    return fail("basic seek test");
  }
  if (!eof_then_seek_test(root)) {
    return fail("eof then seek test");
  }
  if (!multiple_seek_test(root)) {
    return fail("multiple seek test");
  }
  if (!target_after_final_frame_test(root)) {
    return fail("target after final frame test");
  }
  if (!negative_target_test(root)) {
    return fail("negative target test");
  }
  if (!invalid_seek_target_test(root)) {
    return fail("invalid seek target test");
  }
  if (!non_zero_start_time_seek_test(root)) {
    return fail("non-zero start-time seek test");
  }

  return 0;
}
