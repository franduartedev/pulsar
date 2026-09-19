#include <pulsar/application/waveform/waveform_cache.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_waveform_source.hpp>

#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
#ifndef PULSAR_SOURCE_DIR
#error "PULSAR_SOURCE_DIR is required"
#endif
  const std::filesystem::path fixture = std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/audio_decode_fixture.mp4";
  pulsar::media::ffmpeg::FFmpegWaveformSource source{fixture, 1};
  const auto info = source.info();
  const auto key = pulsar::application::make_waveform_cache_key(
      fixture, 1, info.sample_rate, info.channel_count, info.time_base, info.stream_origin);
  const auto directory = std::filesystem::temp_directory_path() / "pulsar-a7-ffmpeg";
  std::filesystem::create_directories(directory);
  const auto path = directory / "fixture.pwfc";
  const auto report = pulsar::application::generate_waveform_cache(source, key, path);
  assert(report.source_frames > 0);
  auto reader = pulsar::application::WaveformCacheReader::open(path, key);
  assert(reader.has_value());
  assert(reader->source_frames() == report.source_frames);
  const auto end = pulsar::domain::MediaTime{static_cast<std::int64_t>(report.source_frames),
                                              pulsar::domain::Rational{1, info.sample_rate}};
  const auto slice = reader->query({pulsar::domain::MediaTime::zero(), end, 128, 1.0});
  assert(slice.completeness == pulsar::application::WaveformCompleteness::Complete);
  assert(!slice.buckets.empty());
  std::filesystem::remove_all(directory);
  std::cout << "ffmpeg waveform source test passed\n";
}
