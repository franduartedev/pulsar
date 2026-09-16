#include <pulsar/application/media/media_probe_error.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>
#include <pulsar/media/ffmpeg/media_decode_error.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>

namespace {

using pulsar::application::MediaProbeError;
using pulsar::application::MediaProbeErrorCode;
using pulsar::media::ffmpeg::FFmpegVideoDecodeSession;
using pulsar::media::ffmpeg::MediaDecodeError;
using pulsar::media::ffmpeg::MediaDecodeErrorCode;

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

std::filesystem::path fixture_root() {
  const auto root = std::filesystem::temp_directory_path() / "pulsar-ffmpeg-decode-tests";
  std::filesystem::create_directories(root);
  return root;
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

} // namespace

int main() {
  const auto root = fixture_root();

  if (!ppm_decode_test(root)) {
    return fail("ppm decode test");
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

  return 0;
}
