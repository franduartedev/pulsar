#include <pulsar/application/media/media_probe_error.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace {

using pulsar::application::MediaProbeError;
using pulsar::application::MediaProbeErrorCode;
using pulsar::media::ffmpeg::FFmpegMediaProbe;

class CurrentPathGuard {
public:
  explicit CurrentPathGuard(std::filesystem::path replacement)
      : previous_{std::filesystem::current_path()} {
    std::filesystem::current_path(replacement);
  }

  ~CurrentPathGuard() {
    std::filesystem::current_path(previous_);
  }

  CurrentPathGuard(const CurrentPathGuard&) = delete;
  CurrentPathGuard& operator=(const CurrentPathGuard&) = delete;

private:
  std::filesystem::path previous_;
};

int fail(std::string_view message) {
  std::cerr << "ffmpeg probe test failed: " << message << '\n';
  return 1;
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
  const auto root = std::filesystem::temp_directory_path() / "pulsar-ffmpeg-probe-tests";
  std::filesystem::create_directories(root);
  return root;
}

bool wav_probe_tests(const FFmpegMediaProbe& probe, const std::filesystem::path& root) {
  const auto wav_path = root / "tiny audio fixture.wav";
  write_wav_fixture(wav_path);

  const auto result = probe.probe(wav_path);
  if (result.path != wav_path) {
    return false;
  }
  if (result.audio_streams.size() != 1 || !result.video_streams.empty()) {
    return false;
  }

  const auto& audio = result.audio_streams.front();
  if (audio.sample_rate != 48'000 || audio.channel_count != 2) {
    return false;
  }
  if (audio.time_base <= pulsar::domain::Rational{0, 1}) {
    return false;
  }
  if (!audio.duration.has_value()) {
    return false;
  }

  const auto duration = audio.duration->seconds();
  if (duration != pulsar::domain::Rational{1, 100}) {
    return false;
  }

  return true;
}

bool visual_probe_tests(const FFmpegMediaProbe& probe, const std::filesystem::path& root) {
  const auto ppm_path = root / "tiny visual fixture.ppm";
  write_ppm_fixture(ppm_path);

  const auto result = probe.probe(ppm_path);
  if (result.video_streams.empty()) {
    return false;
  }

  const auto& video = result.video_streams.front();
  if (video.width != 16 || video.height != 16) {
    return false;
  }
  if (video.codec_name.empty()) {
    return false;
  }
  if (video.time_base <= pulsar::domain::Rational{0, 1}) {
    return false;
  }

  return true;
}

bool path_tests(const FFmpegMediaProbe& probe, const std::filesystem::path& root) {
  const auto nested = root / "relative-paths";
  std::filesystem::create_directories(nested);
  const auto wav_path = nested / "audio:local-name.wav";
  write_wav_fixture(wav_path);

  const auto absolute_result = probe.probe(std::filesystem::absolute(wav_path));
  if (absolute_result.audio_streams.size() != 1) {
    return false;
  }

  CurrentPathGuard current_path{root};
  const auto relative_result =
      probe.probe(std::filesystem::path{"relative-paths"} / "audio:local-name.wav");
  if (relative_result.audio_streams.size() != 1) {
    return false;
  }

  const auto link_path = root / "wav symlink.wav";
  std::error_code error;
  std::filesystem::remove(link_path, error);
  std::filesystem::create_symlink(wav_path, link_path, error);
  if (!error) {
    const auto symlink_result = probe.probe(link_path);
    if (symlink_result.audio_streams.size() != 1) {
      return false;
    }
  }

  return true;
}

bool error_tests(const FFmpegMediaProbe& probe, const std::filesystem::path& root) {
  if (!throws_probe_error([&] { (void)probe.probe(root / "missing.wav"); },
                          MediaProbeErrorCode::FileNotFound)) {
    return false;
  }
  if (!throws_probe_error([&] { (void)probe.probe(root); },
                          MediaProbeErrorCode::NotRegularFile)) {
    return false;
  }
  if (!throws_probe_error([&] { (void)probe.probe("https://example.invalid/video.mp4"); },
                          MediaProbeErrorCode::RemoteResourceNotAllowed)) {
    return false;
  }

  const auto garbage = root / "not really media.mp4";
  {
    std::ofstream output{garbage, std::ios::binary};
    output << "this is not multimedia";
  }
  if (!throws_probe_error([&] { (void)probe.probe(garbage); },
                          MediaProbeErrorCode::UnsupportedMedia)) {
    return false;
  }

  const auto empty = root / "empty.wav";
  {
    std::ofstream output{empty, std::ios::binary};
  }
  if (!throws_probe_error([&] { (void)probe.probe(empty); },
                          MediaProbeErrorCode::UnsupportedMedia)) {
    return false;
  }

  const auto unicode_wav = root / "audio-unicode-ñ.wav";
  write_wav_fixture(unicode_wav);
  const auto result = probe.probe(unicode_wav);
  return result.audio_streams.size() == 1;
}

} // namespace

int main() {
  const FFmpegMediaProbe probe;
  const auto root = fixture_root();

  if (!wav_probe_tests(probe, root)) {
    return fail("wav probe tests");
  }
  if (!visual_probe_tests(probe, root)) {
    return fail("visual probe tests");
  }
  if (!path_tests(probe, root)) {
    return fail("path tests");
  }
  if (!error_tests(probe, root)) {
    return fail("error tests");
  }

  return 0;
}
