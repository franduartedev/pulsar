#include <pulsar/application/bootstrap.hpp>
#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/domain/time/frame_number.hpp>
#include <pulsar/domain/time/frame_rate.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/time_range.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/timeline.hpp>
#include <pulsar/domain/timeline/track.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>
#include <pulsar/application/media/media_probe_error.hpp>
#include <pulsar/application/playback/monotonic_clock.hpp>
#include <pulsar/application/playback/playback_session.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_playback_worker.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_playback_visual_mailbox.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/media_decode_error.hpp>

#include <algorithm>
#include <thread>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_seconds(const std::optional<pulsar::domain::MediaTime>& time) {
  if (!time.has_value()) {
    std::cout << "unknown";
    return;
  }

  const auto seconds = time->seconds();
  std::cout << seconds.numerator();
  if (seconds.denominator() != 1) {
    std::cout << '/' << seconds.denominator();
  }
  std::cout << " s";
}

void print_timestamp(const std::optional<pulsar::domain::MediaTime>& time) {
  print_seconds(time);
}

void print_media_time(pulsar::domain::MediaTime time) {
  const auto seconds = time.seconds();
  std::cout << seconds.numerator();
  if (seconds.denominator() != 1) {
    std::cout << '/' << seconds.denominator();
  }
  std::cout << " s";
}

pulsar::domain::MediaTime parse_exact_seconds(std::string_view text) {
  const auto slash = text.find('/');
  if (slash == std::string_view::npos || slash == 0 || slash + 1 == text.size()) {
    throw std::invalid_argument{"expected exact rational seconds as numerator/denominator"};
  }

  const auto numerator = std::stoll(std::string{text.substr(0, slash)});
  const auto denominator = std::stoll(std::string{text.substr(slash + 1)});
  return pulsar::domain::MediaTime{
      numerator,
      pulsar::domain::Rational{1, denominator},
  };
}

void print_probe_result(const pulsar::application::MediaProbeResult& result) {
  std::cout << "PULSAR Engine Lab\n";
  std::cout << "Media Probe\n";
  std::cout << "File: " << result.path.filename().string() << '\n';
  std::cout << "Container: "
            << (result.container_name.has_value() ? *result.container_name : "unknown") << '\n';
  std::cout << "Duration: ";
  print_seconds(result.duration);
  std::cout << "\n";

  for (const auto& stream : result.video_streams) {
    std::cout << "Video stream #" << stream.stream_index << '\n';
    std::cout << "Codec: " << stream.codec_name << '\n';
    std::cout << "Resolution: " << stream.width << 'x' << stream.height << '\n';
    std::cout << "Time base: " << stream.time_base.numerator() << '/'
              << stream.time_base.denominator() << '\n';
    std::cout << "Duration: ";
    print_seconds(stream.duration);
    std::cout << "\n";
    std::cout << "Nominal FPS: ";
    if (stream.nominal_frame_rate.has_value()) {
      const auto rate = stream.nominal_frame_rate->rational();
      std::cout << rate.numerator() << '/' << rate.denominator();
    } else {
      std::cout << "unknown";
    }
    std::cout << "\n";
  }

  for (const auto& stream : result.audio_streams) {
    std::cout << "Audio stream #" << stream.stream_index << '\n';
    std::cout << "Codec: " << stream.codec_name << '\n';
    std::cout << "Sample rate: " << stream.sample_rate << '\n';
    std::cout << "Channels: " << stream.channel_count << '\n';
    std::cout << "Time base: " << stream.time_base.numerator() << '/'
              << stream.time_base.denominator() << '\n';
    std::cout << "Duration: ";
    print_seconds(stream.duration);
    std::cout << "\n";
  }
}

int run_probe(const std::filesystem::path& path) {
  const auto probe = pulsar::media::ffmpeg::FFmpegMediaProbe{};
  const auto result = probe.probe(path);
  print_probe_result(result);
  return 0;
}

int run_decode(const std::filesystem::path& path) {
  const auto probe = pulsar::media::ffmpeg::FFmpegMediaProbe{};
  const auto probe_result = probe.probe(path);
  if (probe_result.video_streams.empty()) {
    std::cerr << "Video decode failed: media file contains no video stream\n";
    return 2;
  }

  const int stream_index = probe_result.video_streams.front().stream_index;
  auto session = pulsar::media::ffmpeg::FFmpegVideoDecodeSession{path, stream_index};

  std::size_t frame_count = 0;
  std::size_t frames_without_timestamp = 0;
  bool resolution_changed = false;
  bool pixel_format_changed = false;
  int first_width = 0;
  int first_height = 0;
  std::optional<std::string> first_pixel_format;
  std::optional<pulsar::domain::MediaTime> first_timestamp;
  std::optional<pulsar::domain::MediaTime> last_timestamp;
  std::optional<pulsar::domain::MediaTime> previous_timestamp;
  bool observed_timestamp_order = false;
  bool timestamps_monotonic = true;

  const auto started_at = std::chrono::steady_clock::now();
  while (auto frame = session.decode_next()) {
    if (frame_count == 0) {
      first_width = frame->width;
      first_height = frame->height;
      first_pixel_format = frame->pixel_format_name;
      first_timestamp = frame->presentation_timestamp;
    } else {
      resolution_changed =
          resolution_changed || frame->width != first_width || frame->height != first_height;
      pixel_format_changed =
          pixel_format_changed || frame->pixel_format_name != first_pixel_format;
    }

    if (!frame->presentation_timestamp.has_value()) {
      ++frames_without_timestamp;
    } else {
      if (previous_timestamp.has_value()) {
        observed_timestamp_order = true;
        timestamps_monotonic =
            timestamps_monotonic && *previous_timestamp <= *frame->presentation_timestamp;
      }
      previous_timestamp = frame->presentation_timestamp;
      last_timestamp = frame->presentation_timestamp;
    }

    ++frame_count;
  }
  const auto finished_at = std::chrono::steady_clock::now();
  const bool eof_stable = !session.decode_next().has_value();
  const auto elapsed = std::chrono::duration<double>(finished_at - started_at).count();
  const double frames_per_second =
      elapsed > 0.0 ? static_cast<double>(frame_count) / elapsed : 0.0;

  std::cout << "PULSAR Engine Lab\n";
  std::cout << "Video Decode\n";
  std::cout << "File: " << path.filename().string() << '\n';
  std::cout << "Stream: #" << session.stream_index() << '\n';
  std::cout << "Codec: " << session.codec_name() << '\n';
  std::cout << "Frames decoded: " << frame_count << '\n';
  std::cout << "Resolution: ";
  if (frame_count == 0) {
    std::cout << "unknown\n";
  } else {
    std::cout << first_width << 'x' << first_height << '\n';
  }
  std::cout << "Pixel format: "
            << (first_pixel_format.has_value() ? *first_pixel_format : "unknown") << '\n';
  std::cout << "First timestamp: ";
  print_timestamp(first_timestamp);
  std::cout << "\n";
  std::cout << "Last timestamp: ";
  print_timestamp(last_timestamp);
  std::cout << "\n";
  std::cout << "Frames without timestamp: " << frames_without_timestamp << '\n';
  std::cout << "Timestamps monotonic: ";
  if (!observed_timestamp_order) {
    std::cout << "n/a\n";
  } else {
    std::cout << (timestamps_monotonic ? "yes" : "no") << '\n';
  }
  std::cout << "Resolution changed: " << (resolution_changed ? "yes" : "no") << '\n';
  std::cout << "Pixel format changed: " << (pixel_format_changed ? "yes" : "no") << '\n';
  std::cout << "EOF drain: " << (eof_stable ? "OK" : "FAILED") << '\n';
  std::cout << "Decode time: " << std::fixed << std::setprecision(3) << (elapsed * 1000.0)
            << " ms\n";
  std::cout << "Approx throughput: " << std::fixed << std::setprecision(1) << frames_per_second
            << " fps\n";

  return eof_stable ? 0 : 2;
}

int run_seek(const std::filesystem::path& path, pulsar::domain::MediaTime target) {
  const auto probe = pulsar::media::ffmpeg::FFmpegMediaProbe{};
  const auto probe_result = probe.probe(path);
  if (probe_result.video_streams.empty()) {
    std::cerr << "Video seek failed: media file contains no video stream\n";
    return 2;
  }

  const int stream_index = probe_result.video_streams.front().stream_index;
  auto session = pulsar::media::ffmpeg::FFmpegVideoDecodeSession{path, stream_index};

  const auto started_at = std::chrono::steady_clock::now();
  const auto result = session.seek_to_stream_timestamp(target);
  const auto finished_at = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration<double>(finished_at - started_at).count();

  std::cout << "PULSAR Engine Lab\n";
  std::cout << "Video Seek\n";
  std::cout << "File: " << path.filename().string() << '\n';
  std::cout << "Stream: #" << session.stream_index() << '\n';
  std::cout << "Codec: " << session.codec_name() << '\n';
  std::cout << "Stream start: ";
  print_timestamp(session.stream_start_timestamp());
  std::cout << "\n";
  std::cout << "Requested: ";
  print_media_time(result.requested_timestamp);
  std::cout << "\n";
  std::cout << "Reached: ";
  if (result.frame.has_value()) {
    print_timestamp(result.frame->presentation_timestamp);
  } else {
    std::cout << "not found";
  }
  std::cout << "\n";
  std::cout << "Frames decoded after seek: " << result.frames_decoded_after_seek << '\n';
  if (result.frame.has_value()) {
    std::cout << "Resolution: " << result.frame->width << 'x' << result.frame->height << '\n';
    std::cout << "Pixel format: "
              << (result.frame->pixel_format_name.has_value() ? *result.frame->pixel_format_name
                                                               : "unknown")
              << '\n';
  }
  std::cout << "Seek: " << (result.frame.has_value() ? "OK" : "EOF") << '\n';
  std::cout << "Seek time: " << std::fixed << std::setprecision(3) << (elapsed * 1000.0)
            << " ms\n";

  return 0;
}

int run_seek_series(const std::filesystem::path& path, const char* const* targets,
                    int target_count) {
  const auto probe = pulsar::media::ffmpeg::FFmpegMediaProbe{};
  const auto probe_result = probe.probe(path);
  if (probe_result.video_streams.empty()) {
    std::cerr << "Video seek failed: media file contains no video stream\n";
    return 2;
  }

  const int stream_index = probe_result.video_streams.front().stream_index;
  auto session = pulsar::media::ffmpeg::FFmpegVideoDecodeSession{path, stream_index};

  std::cout << "PULSAR Engine Lab\n";
  std::cout << "Video Seek Series\n";
  std::cout << "File: " << path.filename().string() << '\n';
  std::cout << "Stream: #" << session.stream_index() << '\n';
  std::cout << "Codec: " << session.codec_name() << '\n';
  std::cout << "Stream start: ";
  print_timestamp(session.stream_start_timestamp());
  std::cout << "\n";

  for (int index = 0; index < target_count; ++index) {
    const auto target = parse_exact_seconds(targets[index]);
    const auto started_at = std::chrono::steady_clock::now();
    const auto result = session.seek_to_stream_timestamp(target);
    const auto finished_at = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double>(finished_at - started_at).count();

    std::cout << "Seek #" << (index + 1) << ": requested ";
    print_media_time(result.requested_timestamp);
    std::cout << ", reached ";
    if (result.frame.has_value()) {
      print_timestamp(result.frame->presentation_timestamp);
    } else {
      std::cout << "not found";
    }
    std::cout << ", frames " << result.frames_decoded_after_seek;
    std::cout << ", time " << std::fixed << std::setprecision(3) << (elapsed * 1000.0)
              << " ms\n";
  }

  return 0;
}

int run_seek_after_eof(const std::filesystem::path& path, pulsar::domain::MediaTime target) {
  const auto probe = pulsar::media::ffmpeg::FFmpegMediaProbe{};
  const auto probe_result = probe.probe(path);
  if (probe_result.video_streams.empty()) {
    std::cerr << "Video seek failed: media file contains no video stream\n";
    return 2;
  }

  const int stream_index = probe_result.video_streams.front().stream_index;
  auto session = pulsar::media::ffmpeg::FFmpegVideoDecodeSession{path, stream_index};

  std::size_t frames_before_eof = 0;
  while (session.decode_next().has_value()) {
    ++frames_before_eof;
  }

  const auto started_at = std::chrono::steady_clock::now();
  const auto result = session.seek_to_stream_timestamp(target);
  const auto finished_at = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration<double>(finished_at - started_at).count();

  std::cout << "PULSAR Engine Lab\n";
  std::cout << "Video Seek After EOF\n";
  std::cout << "File: " << path.filename().string() << '\n';
  std::cout << "Stream: #" << session.stream_index() << '\n';
  std::cout << "Frames before EOF: " << frames_before_eof << '\n';
  std::cout << "Requested: ";
  print_media_time(result.requested_timestamp);
  std::cout << "\n";
  std::cout << "Reached: ";
  if (result.frame.has_value()) {
    print_timestamp(result.frame->presentation_timestamp);
  } else {
    std::cout << "not found";
  }
  std::cout << "\n";
  std::cout << "Frames decoded after seek: " << result.frames_decoded_after_seek << '\n';
  std::cout << "Seek: " << (result.frame.has_value() ? "OK" : "EOF") << '\n';
  std::cout << "Seek time: " << std::fixed << std::setprecision(3) << (elapsed * 1000.0)
            << " ms\n";

  return result.frame.has_value() ? 0 : 2;
}

class FakeClock final : public pulsar::application::MonotonicClock {
 public:
  [[nodiscard]] pulsar::application::MonotonicInstant now() const override { return now_; }
  void advance(pulsar::domain::MediaTime delta) {
    now_ = pulsar::application::MonotonicInstant{now_.value() + delta};
  }

 private:
  pulsar::application::MonotonicInstant now_{pulsar::application::MonotonicInstant::zero()};
};

// Nearest-rank percentile over a sorted copy; harness-side only, never production.
std::chrono::nanoseconds percentile_sorted(const std::vector<std::chrono::nanoseconds>& sorted,
                                           double rank) {
  if (sorted.empty()) return std::chrono::nanoseconds{0};
  std::size_t index = static_cast<std::size_t>(rank * static_cast<double>(sorted.size()));
  if (index >= sorted.size()) index = sorted.size() - 1;
  return sorted[index];
}

void bounded_yield(int iteration) {
  if (iteration % 64 == 63) {
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  } else {
    std::this_thread::yield();
  }
}

int run_playback_metrics(const std::filesystem::path& path, int ticks) {
  if (ticks <= 0) {
    std::cerr << "Playback metrics failed: tick count must be positive\n";
    return 2;
  }
  const auto probe = pulsar::media::ffmpeg::FFmpegMediaProbe{};
  const auto probe_result = probe.probe(path);
  if (probe_result.video_streams.empty()) {
    std::cerr << "Playback metrics failed: media file contains no video stream\n";
    return 2;
  }
  const int stream_index = probe_result.video_streams.front().stream_index;

  auto mailbox = std::make_shared<pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox>(8);
  auto worker = std::make_unique<pulsar::media::ffmpeg::FFmpegPlaybackWorker>(
      path, stream_index, pulsar::media::ffmpeg::FFmpegPlaybackWorker::Limits{},
      mailbox);
  // Non-owning observation handle: valid while session owns the worker below.
  const auto* observed = worker.get();
  FakeClock clock;
  auto session =
      pulsar::application::PlaybackSession{clock, std::move(worker)};

  const auto wall_start = std::chrono::steady_clock::now();
  session.open();
  // Bounded cooperative wait for the async open/seek; deterministic tick
  // advancement below stays separate from worker waiting.
  for (int i = 0; i < 20000; ++i) {
    (void)session.update();
    const auto availability = session.availability();
    if (availability == pulsar::application::PlaybackAvailability::Ready ||
        availability == pulsar::application::PlaybackAvailability::Error) {
      break;
    }
    bounded_yield(i);
  }

  session.play();
  const pulsar::domain::MediaTime tick_step{1, pulsar::domain::Rational{1, 60}};
  for (int i = 0; i < ticks; ++i) {
    clock.advance(tick_step);
    (void)session.update();
  }

  // Seek-epoch observation for X1: baselines, one known seek, settle, deltas.
  // Scheduler counters reset on seek by design, so post-seek values below
  // already describe exactly this seek epoch.
  const auto epoch_reads_before = observed->stats().read_request_count;
  const auto epoch_mail_before = mailbox->stats();
  session.seek(pulsar::application::PlaybackPosition{pulsar::domain::MediaTime{2, pulsar::domain::Rational{1, 1}}});
  for (int i = 0; i < 20000; ++i) {
    (void)session.update();
    const auto availability = session.availability();
    if (availability == pulsar::application::PlaybackAvailability::Ready ||
        availability == pulsar::application::PlaybackAvailability::Error) {
      break;
    }
    bounded_yield(i);
  }
  for (int i = 0; i < 240; ++i) {
    (void)session.update();
    bounded_yield(i);
  }
  const auto epoch_sched_after = session.scheduler_stats();
  const auto epoch_reads_after = observed->stats().read_request_count;
  const auto epoch_mail_after = mailbox->stats();
  const auto wall_end = std::chrono::steady_clock::now();

  const auto session_stats = session.stats();
  const auto worker_stats = observed->stats();
  const auto mail_stats = mailbox->stats();
  std::vector<std::chrono::nanoseconds> latencies(
      worker_stats.read_latency_ring.begin(),
      worker_stats.read_latency_ring.begin() +
          static_cast<std::ptrdiff_t>(worker_stats.read_latency_count));
  std::sort(latencies.begin(), latencies.end());
  const auto to_us = [](std::chrono::nanoseconds value) {
    return std::chrono::duration_cast<std::chrono::microseconds>(value).count();
  };

  const auto wall_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(wall_end - wall_start).count();
  std::cout << "media=" << path.filename().string() << '\n';
  std::cout << "ticks=" << ticks << '\n';
  std::cout << "wall_ms=" << wall_ms << '\n';
  std::cout << "session.stale_event_count=" << session_stats.stale_event_count << '\n';
  std::cout << "session.control_accept_count=" << session_stats.control_accept_count << '\n';
  std::cout << "scheduler.starvation_count=" << epoch_sched_after.starvation_count << '\n';
  std::cout << "scheduler.skipped_presentation_count="
            << epoch_sched_after.skipped_presentation_count << '\n';
  std::cout << "worker.read_request_count=" << worker_stats.read_request_count << '\n';
  std::cout << "worker.seek_request_count=" << worker_stats.seek_request_count << '\n';
  std::cout << "worker.read_latency_p50_us=" << to_us(percentile_sorted(latencies, 0.50)) << '\n';
  std::cout << "worker.read_latency_p95_us=" << to_us(percentile_sorted(latencies, 0.95)) << '\n';
  std::cout << "worker.read_latency_max_us="
            << to_us(latencies.empty() ? std::chrono::nanoseconds{0} : latencies.back()) << '\n';
  std::cout << "worker.queue_high_water=" << worker_stats.event_queue_high_water_mark << '\n';
  std::cout << "worker.events_dropped=" << worker_stats.events_dropped_due_to_capacity << '\n';
  std::cout << "worker.samples_dropped=" << worker_stats.samples_dropped_due_to_capacity << '\n';
  std::cout << "worker.control_wait_count=" << worker_stats.control_wait_count << '\n';
  std::cout << "mailbox.published=" << mail_stats.published << '\n';
  std::cout << "mailbox.evicted=" << mail_stats.evicted << '\n';
  std::cout << "mailbox.high_water=" << mail_stats.high_water_mark << '\n';
  std::cout << "mailbox.current_size=" << mail_stats.current_size << '\n';
  std::cout << "seek_epoch.starvation_count=" << epoch_sched_after.starvation_count << '\n';
  std::cout << "seek_epoch.skipped_presentation_count="
            << epoch_sched_after.skipped_presentation_count << '\n';
  std::cout << "seek_epoch.read_request_delta=" << (epoch_reads_after - epoch_reads_before) << '\n';
  std::cout << "seek_epoch.mailbox_published_delta="
            << (epoch_mail_after.published - epoch_mail_before.published) << '\n';
  std::cout << "seek_epoch.mailbox_evicted_delta="
            << (epoch_mail_after.evicted - epoch_mail_before.evicted) << '\n';
  std::cout << "seek_epoch.mailbox_current_size=" << epoch_mail_after.current_size << '\n';
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string_view{argv[1]} == "--probe") {
    try {
      return run_probe(std::filesystem::path{argv[2]});
    } catch (const pulsar::application::MediaProbeError& error) {
      std::cerr << "Media probe failed: " << error.what() << '\n';
      return 2;
    } catch (const std::exception& error) {
      std::cerr << "Media probe failed: " << error.what() << '\n';
      return 2;
    }
  }

  if (argc == 3 && std::string_view{argv[1]} == "--decode") {
    try {
      return run_decode(std::filesystem::path{argv[2]});
    } catch (const pulsar::application::MediaProbeError& error) {
      std::cerr << "Video decode failed: " << error.what() << '\n';
      return 2;
    } catch (const pulsar::media::ffmpeg::MediaDecodeError& error) {
      std::cerr << "Video decode failed: " << error.what() << '\n';
      return 2;
    } catch (const std::exception& error) {
      std::cerr << "Video decode failed: " << error.what() << '\n';
      return 2;
    }
  }

  if (argc == 4 && std::string_view{argv[1]} == "--seek") {
    try {
      return run_seek(std::filesystem::path{argv[2]}, parse_exact_seconds(argv[3]));
    } catch (const pulsar::application::MediaProbeError& error) {
      std::cerr << "Video seek failed: " << error.what() << '\n';
      return 2;
    } catch (const pulsar::media::ffmpeg::MediaDecodeError& error) {
      std::cerr << "Video seek failed: " << error.what() << '\n';
      return 2;
    } catch (const std::exception& error) {
      std::cerr << "Video seek failed: " << error.what() << '\n';
      return 2;
    }
  }

  if (argc >= 4 && std::string_view{argv[1]} == "--seek-series") {
    try {
      return run_seek_series(std::filesystem::path{argv[2]}, argv + 3, argc - 3);
    } catch (const pulsar::application::MediaProbeError& error) {
      std::cerr << "Video seek failed: " << error.what() << '\n';
      return 2;
    } catch (const pulsar::media::ffmpeg::MediaDecodeError& error) {
      std::cerr << "Video seek failed: " << error.what() << '\n';
      return 2;
    } catch (const std::exception& error) {
      std::cerr << "Video seek failed: " << error.what() << '\n';
      return 2;
    }
  }

  if (argc == 4 && std::string_view{argv[1]} == "--seek-after-eof") {
    try {
      return run_seek_after_eof(std::filesystem::path{argv[2]}, parse_exact_seconds(argv[3]));
    } catch (const pulsar::application::MediaProbeError& error) {
      std::cerr << "Video seek failed: " << error.what() << '\n';
      return 2;
    } catch (const pulsar::media::ffmpeg::MediaDecodeError& error) {
      std::cerr << "Video seek failed: " << error.what() << '\n';
      return 2;
    } catch (const std::exception& error) {
      std::cerr << "Video seek failed: " << error.what() << '\n';
      return 2;
    }
  }

  if ((argc == 3 || argc == 4) && std::string_view{argv[1]} == "--playback-metrics") {
    try {
      int ticks = 3600;
      if (argc == 4) {
        ticks = std::stoi(argv[3]);
      }
      return run_playback_metrics(std::filesystem::path{argv[2]}, ticks);
    } catch (const pulsar::application::MediaProbeError& error) {
      std::cerr << "Playback metrics failed: " << error.what() << '\n';
      return 2;
    } catch (const pulsar::media::ffmpeg::MediaDecodeError& error) {
      std::cerr << "Playback metrics failed: " << error.what() << '\n';
      return 2;
    } catch (const std::exception& error) {
      std::cerr << "Playback metrics failed: " << error.what() << '\n';
      return 2;
    }
  }

  if (argc != 1) {
    std::cerr << "Usage: pulsar-engine-lab [--probe <local-media-file> | "
                 "--decode <local-media-file> | "
                 "--seek <local-media-file> <seconds-num/seconds-den> | "
                 "--seek-series <local-media-file> <target>... | "
                 "--seek-after-eof <local-media-file> <target> | "
                 "--playback-metrics <local-media-file> [ticks]]\n";
    return 2;
  }

  using namespace pulsar::domain;

  const auto rate = pulsar::domain::FrameRate{30000, 1001};
  const auto frame = pulsar::domain::FrameNumber{30000};
  const auto time = pulsar::domain::MediaTime::from_frame(frame, rate);
  const auto seconds = time.seconds();
  auto timeline = Timeline{TimelineId{1}};
  timeline.add_track(Track{TrackId{1}, TrackKind::Video});
  timeline.add_track(Track{TrackId{2}, TrackKind::Audio});
  timeline.insert_clip(TrackId{1},
                       Clip{ClipId{1}, MediaId{1},
                            TimeRange{MediaTime{0, Rational{1, 1}}, MediaTime{5, Rational{1, 1}}},
                            MediaTime{0, Rational{1, 1}}});
  timeline.insert_clip(TrackId{1},
                       Clip{ClipId{2}, MediaId{2},
                            TimeRange{MediaTime{0, Rational{1, 1}}, MediaTime{5, Rational{1, 1}}},
                            MediaTime{5, Rational{1, 1}}});
  timeline.insert_clip(TrackId{2},
                       Clip{ClipId{3}, MediaId{3},
                            TimeRange{MediaTime{0, Rational{1, 1}}, MediaTime{10, Rational{1, 1}}},
                            MediaTime{0, Rational{1, 1}}});
  const auto timeline_duration = timeline.duration().seconds();
  std::size_t clip_count = 0;
  for (const auto& track : timeline.tracks()) {
    clip_count += track.clips().size();
  }

  std::cout << "PULSAR Engine Lab\n";
  std::cout << "Foundation build OK\n";
  std::cout << "Time model\n";
  std::cout << "Rate: " << rate.rational().numerator() << '/' << rate.rational().denominator()
            << " fps\n";
  std::cout << "Frame: " << frame.value() << '\n';
  std::cout << "Time: " << seconds.numerator();
  if (seconds.denominator() != 1) {
    std::cout << '/' << seconds.denominator();
  }
  std::cout << " s\n";
  std::cout << "Timeline model\n";
  std::cout << "Tracks: " << timeline.tracks().size() << '\n';
  std::cout << "Clips: " << clip_count << '\n';
  std::cout << "Duration: " << timeline_duration.numerator();
  if (timeline_duration.denominator() != 1) {
    std::cout << '/' << timeline_duration.denominator();
  }
  std::cout << " s\n";

  pulsar::application::ProjectDocument command_document;
  auto processor = pulsar::application::CommandProcessor{command_document};
  const auto asset = pulsar::application::MediaAsset{
      MediaId{1}, "engine-lab/source.mp4", 0, MediaTime{10, Rational{1, 1}},
      pulsar::application::VideoAssetMetadata{"synthetic", 1920, 1080, Rational{1, 1000},
                                               std::nullopt, std::nullopt}};
  processor.execute(pulsar::application::ImportAssetCommand{asset});
  const auto clip_id = processor.add_clip(
      MediaId{1}, TimeRange{MediaTime{0, Rational{1, 1}}, MediaTime{1, Rational{1, 1}}},
      MediaTime{0, Rational{1, 1}});
  const bool add_ok = command_document.timeline().find_clip(clip_id) != nullptr;
  const bool undo_ok = processor.undo() && command_document.timeline().find_clip(clip_id) == nullptr;
  const bool redo_ok = processor.redo() && command_document.timeline().find_clip(clip_id) != nullptr;

  std::cout << "Command model\n";
  std::cout << "Add clip: " << (add_ok ? "OK" : "FAILED") << '\n';
  std::cout << "Undo: " << (undo_ok ? "OK" : "FAILED") << '\n';
  std::cout << "Redo: " << (redo_ok ? "OK" : "FAILED") << '\n';
  return 0;
}
