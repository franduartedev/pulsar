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
  const auto multichannel = std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/audio_multichannel_s16p.nut";
  pulsar::media::ffmpeg::FFmpegWaveformSource sixteen{multichannel, 0};
  assert(sixteen.info().channel_count == 16);
  const auto sixteen_key = pulsar::application::make_waveform_cache_key(
      multichannel, 0, sixteen.info().sample_rate, sixteen.info().channel_count,
      sixteen.info().time_base, sixteen.info().stream_origin);
  const auto sixteen_path = directory / "sixteen.pwfc";
  const auto sixteen_report = pulsar::application::generate_waveform_cache(sixteen, sixteen_key, sixteen_path);
  auto sixteen_reader = pulsar::application::WaveformCacheReader::open(sixteen_path, sixteen_key);
  assert(sixteen_reader.has_value());
  const auto sixteen_end = pulsar::domain::MediaTime{static_cast<std::int64_t>(sixteen_report.source_frames),
                                                     pulsar::domain::Rational{1, sixteen.info().sample_rate}};
  const auto sixteen_slice = sixteen_reader->query({pulsar::domain::MediaTime::zero(), sixteen_end, 64, 1.0});
  assert(sixteen_slice.channel_count == 16);
  assert(sixteen_slice.buckets.size() % 16 == 0);
  assert(sixteen_slice.buckets.size() >= 16);
  for (int channel = 1; channel < 16; ++channel) {
    assert(sixteen_slice.buckets[static_cast<std::size_t>(channel)].minimum !=
           sixteen_slice.buckets[static_cast<std::size_t>(channel - 1)].minimum);
  }

  const auto late = std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/audio_late.mkv";
  pulsar::media::ffmpeg::FFmpegWaveformSource delayed{late, 1};
  assert(delayed.info().stream_origin.has_value());
  assert(static_cast<double>(*delayed.info().stream_origin) > 0.3);
  const auto late_key = pulsar::application::make_waveform_cache_key(
      late, 1, delayed.info().sample_rate, delayed.info().channel_count,
      delayed.info().time_base, delayed.info().stream_origin);
  const auto late_path = directory / "late.pwfc";
  const auto late_report = pulsar::application::generate_waveform_cache(delayed, late_key, late_path);
  auto late_reader = pulsar::application::WaveformCacheReader::open(late_path, late_key);
  assert(late_reader.has_value());
  const auto early = late_reader->query({pulsar::domain::MediaTime::zero(), *delayed.info().stream_origin, 64, 1.0});
  assert(early.buckets.empty());
  const auto late_end = *delayed.info().stream_origin + pulsar::domain::MediaTime{
      static_cast<std::int64_t>(late_report.source_frames), pulsar::domain::Rational{1, delayed.info().sample_rate}};
  const auto audible = late_reader->query({*delayed.info().stream_origin, late_end, 64, 1.0});
  assert(!audible.buckets.empty());
  std::filesystem::remove_all(directory);
  std::cout << "ffmpeg waveform source test passed\n";
}
