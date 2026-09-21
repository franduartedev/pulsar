#include <pulsar/application/waveform/waveform_cache.hpp>
#include <pulsar/application/waveform/waveform_renderer.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_audio_playback_source.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_waveform_source.hpp>

#include <cassert>
#include <atomic>
#include <filesystem>
#include <memory>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error{message};
}
}

int main() {
#ifndef PULSAR_SOURCE_DIR
#error "PULSAR_SOURCE_DIR is required"
#endif
  const std::filesystem::path fixture = std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/audio_decode_fixture.mp4";
  pulsar::media::ffmpeg::FFmpegWaveformSource source{fixture, 1};
  const auto info = source.info();
  pulsar::media::ffmpeg::FFmpegAudioPlaybackSource playback_source{fixture, 1};
  check(info.stream_origin == playback_source.stream_origin(),
        "A3 playback and A7 waveform must use the same stream origin");
  check(info.stream_origin.has_value() && *info.stream_origin == pulsar::domain::MediaTime::zero(),
        "zero-origin fixture must resolve to zero");
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
  pulsar::media::ffmpeg::FFmpegAudioPlaybackSource delayed_playback{late, 1};
  check(delayed.info().stream_origin == delayed_playback.stream_origin(),
        "audio-late A3/A7 stream origins must agree");
  check(delayed.info().stream_origin.has_value(), "audio-late origin must be present");
  check(static_cast<double>(*delayed.info().stream_origin) > 0.3,
        "audio-late origin must remain nonzero");
  assert(delayed.info().stream_origin.has_value());
  assert(static_cast<double>(*delayed.info().stream_origin) > 0.3);
  const auto late_key = pulsar::application::make_waveform_cache_key(
      late, 1, delayed.info().sample_rate, delayed.info().channel_count,
      delayed.info().time_base, delayed.info().stream_origin);
  const auto late_path = directory / "late.pwfc";

  // A8 supplies known stream identity, not an origin guess. A7 resolves the
  // canonical origin on its worker before cache generation and publishes it
  // back through the request API.
  auto identity_key = pulsar::application::make_waveform_cache_key(
      late, 1, delayed.info().sample_rate, delayed.info().channel_count,
      delayed.info().time_base, std::nullopt);
  const auto repeated_identity_key = pulsar::application::make_waveform_cache_key(
      late, 1, delayed.info().sample_rate, delayed.info().channel_count,
      delayed.info().time_base, std::nullopt);
  check(identity_key == repeated_identity_key,
        "same source metadata must resolve to a deterministic cache identity");
  check(identity_key == late_key,
        "origin metadata must not split one exact file/stream cache identity");
  pulsar::application::WaveformCacheManager manager{1};
  std::atomic<int> factory_calls{0};
  std::atomic<std::int64_t> metadata_resolution_us{0};
  const auto source_factory = [&] {
    const auto started = std::chrono::steady_clock::now();
    ++factory_calls;
    auto source = std::make_unique<pulsar::media::ffmpeg::FFmpegWaveformSource>(late, 1);
    metadata_resolution_us += std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now() - started).count();
    return source;
  };
  auto request = manager.request(directory / "manager", identity_key, source_factory);
  auto coalesced = manager.request(directory / "manager", identity_key, source_factory);
  const bool completed = request->wait_for(std::chrono::seconds{10});
  const bool coalesced_completed = coalesced->wait_for(std::chrono::seconds{10});
  check(completed && coalesced_completed, "origin bridge generation timed out");
  check(request->progress().status == pulsar::application::WaveformCacheStatus::Ready,
        "origin bridge generation failed");
  const auto resolved_key = request->resolved_key();
  check(resolved_key.has_value() && resolved_key->stream_origin == delayed.info().stream_origin,
        "A7 request must publish canonical audio-late origin");
  check(factory_calls.load() == 1, "coalesced A7 requests must create one source");
  std::cout << "ffmpeg waveform metadata_resolve_us=" << metadata_resolution_us.load()
            << " (source open/probe, no PCM block requested)\n";
  const auto generated_frames = request->progress().generated_frames;
  const auto visible_end = *resolved_key->stream_origin + pulsar::domain::MediaTime{
      static_cast<std::int64_t>(generated_frames),
      pulsar::domain::Rational{1, delayed.info().sample_rate}};
  const auto query_started = std::chrono::steady_clock::now();
  const auto rendered_slice = request->query_partial(
      {pulsar::domain::MediaTime::zero(), visible_end, 128, 1.0});
  const auto query_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - query_started);
  std::cout << "ffmpeg waveform cached_query_us=" << query_elapsed.count() << '\n';
  check(rendered_slice.has_value(), "real A7 query must reach waveform renderer");
  const auto rendered = pulsar::application::prepare_waveform_geometry(
      *rendered_slice, delayed.info().sample_rate, {1.0, false, 128, 40.0});
  check(!rendered.unavailable_columns.empty(),
        "real audio-late visual must mark pre-origin region unavailable");
  const auto origin_column = static_cast<std::uint32_t>(
      static_cast<double>(*resolved_key->stream_origin) /
      static_cast<double>(visible_end) * 128.0);
  const auto first_peak = std::min_element(
      rendered.segments.begin(), rendered.segments.end(),
      [](const auto& left, const auto& right) { return left.column < right.column; });
  check(first_peak != rendered.segments.end() && first_peak->column + 1 >= origin_column,
        "real audio-late visual must not shift waveform left of stream origin");
  manager.shutdown();
  pulsar::application::WaveformCacheManager reuse_manager{1};
  auto reused = reuse_manager.request(directory / "manager", identity_key, source_factory);
  const bool reused_ready = reused->wait_for(std::chrono::seconds{10});
  check(reused_ready && reused->progress().status ==
                            pulsar::application::WaveformCacheStatus::Ready,
        "ready originless identity cache must be reused");
  check(factory_calls.load() == 1, "ready cache reuse must not reopen/decode its source");
  check(reused->resolved_key()->stream_origin == delayed.info().stream_origin,
        "ready cache must publish its stored canonical origin");
  reuse_manager.shutdown();
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
