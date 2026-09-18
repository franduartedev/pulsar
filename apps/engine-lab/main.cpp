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
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
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

// Simulates the desktop visual consumer: each new presentation ordinal is
// taken at most once from the epoch-gated mailbox. Missing entries are
// counted, never hidden. Harness-side only.
class VisualConsumer {
 public:
  void consider(pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox& mailbox,
                pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox::SourceEpoch epoch,
                std::uint64_t ordinal) {
    if (last_ordinal_.has_value() && *last_ordinal_ == ordinal) {
      return;
    }
    last_ordinal_ = ordinal;
    if (!mailbox.take(epoch, ordinal).has_value()) {
      ++misses_;
    }
  }

  void consider_decision(
      pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox& mailbox,
      pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox::SourceEpoch epoch,
      const pulsar::application::PresentationDecision& decision) {
    if (const auto* present = std::get_if<pulsar::application::PresentSample>(&decision)) {
      consider(mailbox, epoch, present->sample.ordinal);
    } else if (const auto* hold = std::get_if<pulsar::application::HoldPresentation>(&decision)) {
      consider(mailbox, epoch, hold->sample.ordinal);
    }
  }

  [[nodiscard]] std::uint64_t misses() const noexcept { return misses_; }

 private:
  std::optional<std::uint64_t> last_ordinal_;
  std::uint64_t misses_{0};
};

// Bounded wait for worker read completions without evaluating the scheduler
// again and without advancing fake time. Returns true on observed progress.
bool await_read_completion(const pulsar::media::ffmpeg::FFmpegPlaybackWorker& worker,
                           std::uint64_t baseline_completions) {
  for (int i = 0; i < 20000; ++i) {
    if (worker.stats().read_completion_count > baseline_completions) {
      return true;
    }
    bounded_yield(i);
  }
  return worker.stats().read_completion_count > baseline_completions;
}

// Bounded wait until the session leaves a transitional availability.
void await_settled(pulsar::application::PlaybackSession& session) {
  for (int i = 0; i < 20000; ++i) {
    (void)session.update();
    const auto availability = session.availability();
    if (availability == pulsar::application::PlaybackAvailability::Ready ||
        availability == pulsar::application::PlaybackAvailability::Error) {
      return;
    }
    bounded_yield(i);
  }
}

// Single-line form for key=value output; error text must not break the schema.
std::string single_line(std::string text) {
  for (char& c : text) {
    if (c == '\n' || c == '\r') {
      c = ' ';
    }
  }
  return text;
}

// Terminal product error visible after one evaluated update, or empty.
std::string update_error(const pulsar::application::PresentationDecision& decision,
                         const pulsar::application::PlaybackSession& session) {
  if (const auto* temporal =
          std::get_if<pulsar::application::PresentationTemporalError>(&decision)) {
    return single_line(temporal->message);
  }
  if (session.availability() == pulsar::application::PlaybackAvailability::Error) {
    const auto message = session.error_message();
    return message.has_value() ? single_line(*message) : std::string{"session error"};
  }
  return "";
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
  await_settled(session);

  // Phase A: main playback. Exactly one scheduler evaluation per fake tick;
  // newly issued reads settle cooperatively without advancing fake time.
  // A terminal session error stops the workload immediately; metrics gathered
  // so far are preserved and reported.
  VisualConsumer consumer;
  session.play();
  const pulsar::domain::MediaTime tick_step{1, pulsar::domain::Rational{1, 60}};
  int main_completed_ticks = 0;
  std::string main_error;
  for (int i = 0; i < ticks; ++i) {
    const auto reads_before = observed->stats().read_request_count;
    const auto completions_before = observed->stats().read_completion_count;
    clock.advance(tick_step);
    const auto decision = session.update();
    consumer.consider_decision(*mailbox, observed->source_epoch(), decision);
    if (observed->stats().read_request_count > reads_before) {
      (void)await_read_completion(*observed, completions_before);
    }
    ++main_completed_ticks;
    main_error = update_error(decision, session);
    if (!main_error.empty()) {
      break;
    }
  }
  const bool main_healthy = main_error.empty();

  // Main snapshot is captured BEFORE the seek experiment below, so top-level
  // fields describe main playback only.
  const auto main_session = session.stats();
  const auto main_scheduler = session.scheduler_stats();
  const auto main_worker = observed->stats();
  const auto main_mailbox = mailbox->stats();
  const auto main_misses = consumer.misses();
  const auto main_wall_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                            wall_start)
          .count();

  // Phase B: seek epoch. Interior target only; otherwise report unavailable
  // instead of fabricating X1 numbers.
  bool seek_available = false;
  pulsar::domain::MediaTime seek_target = pulsar::domain::MediaTime::zero();
  if (probe_result.duration.has_value()) {
    const auto duration = *probe_result.duration;
    const pulsar::domain::MediaTime minimum{4, pulsar::domain::Rational{1, 1}};
    if (duration > minimum) {
      const auto half = pulsar::domain::MediaTime{duration.value() / 2, duration.time_base()};
      const pulsar::domain::MediaTime two{2, pulsar::domain::Rational{1, 1}};
      seek_target = (two < half) ? two : half;
      seek_available = seek_target > pulsar::domain::MediaTime::zero();
    }
  }

  bool seek_completed = false;
  bool seek_read_exercised = false;
  bool seek_healthy = false;
  std::string seek_error;
  pulsar::application::PresentationSchedulerStats epoch_sched{};
  std::uint64_t epoch_read_delta = 0;
  std::uint64_t epoch_completion_delta = 0;
  pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox::Stats epoch_mail_before{};
  pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox::Stats epoch_mail_after{};
  std::uint64_t epoch_misses = 0;
  pulsar::domain::Rational target_seconds{0, 1};
  if (seek_available) {
    target_seconds = seek_target.seconds();
    const auto reads_before = observed->stats().read_request_count;
    const auto completions_before = observed->stats().read_completion_count;
    epoch_mail_before = mailbox->stats();
    const auto misses_before = consumer.misses();
    session.seek(pulsar::application::PlaybackPosition{seek_target});
    await_settled(session);
    consumer.consider_decision(*mailbox, observed->source_epoch(), session.update());
    // Advance fake time tick by tick until one normal post-seek read has been
    // both issued and completed (seek hit/lookahead settling included).
    // A terminal error stops the workload; metrics so far are preserved.
    for (int i = 0; i < 3000; ++i) {
      const auto tick_reads = observed->stats().read_request_count;
      const auto tick_completions = observed->stats().read_completion_count;
      clock.advance(tick_step);
      const auto decision = session.update();
      consumer.consider_decision(*mailbox, observed->source_epoch(), decision);
      if (observed->stats().read_request_count > tick_reads) {
        (void)await_read_completion(*observed, tick_completions);
      }
      seek_error = update_error(decision, session);
      if (!seek_error.empty()) {
        break;
      }
      if (observed->stats().read_request_count > reads_before &&
          observed->stats().read_completion_count > completions_before) {
        seek_completed = true;
        break;
      }
    }
    if (seek_completed) {
      // Deliberate ingestion update for the completed read: no clock advance,
      // exactly one scheduler evaluation, fed through the visual consumer.
      // This is what reveals a duplicate-timestamp defect if present.
      const auto decision = session.update();
      consumer.consider_decision(*mailbox, observed->source_epoch(), decision);
      seek_error = update_error(decision, session);
      seek_read_exercised = true;
      seek_healthy = seek_error.empty();
    }
    epoch_sched = session.scheduler_stats();
    epoch_read_delta = observed->stats().read_request_count - reads_before;
    epoch_completion_delta = observed->stats().read_completion_count - completions_before;
    epoch_mail_after = mailbox->stats();
    epoch_misses = consumer.misses() - misses_before;
  }
  std::vector<std::chrono::nanoseconds> latencies(
      main_worker.read_latency_ring.begin(),
      main_worker.read_latency_ring.begin() +
          static_cast<std::ptrdiff_t>(main_worker.read_latency_count));
  std::sort(latencies.begin(), latencies.end());
  const auto to_us = [](std::chrono::nanoseconds value) {
    return std::chrono::duration_cast<std::chrono::microseconds>(value).count();
  };

  std::cout << "media=" << path.filename().string() << '\n';
  std::cout << "ticks=" << ticks << '\n';
  std::cout << "wall_ms=" << main_wall_ms << '\n';
  std::cout << "main.requested_ticks=" << ticks << '\n';
  std::cout << "main.completed_ticks=" << main_completed_ticks << '\n';
  std::cout << "main.healthy=" << (main_healthy ? 1 : 0) << '\n';
  std::cout << "main.error_message=" << main_error << '\n';
  std::cout << "session.stale_event_count=" << main_session.stale_event_count << '\n';
  std::cout << "session.control_accept_count=" << main_session.control_accept_count << '\n';
  std::cout << "scheduler.starvation_count=" << main_scheduler.starvation_count << '\n';
  std::cout << "scheduler.skipped_presentation_count="
            << main_scheduler.skipped_presentation_count << '\n';
  std::cout << "worker.read_request_count=" << main_worker.read_request_count << '\n';
  std::cout << "worker.seek_request_count=" << main_worker.seek_request_count << '\n';
  std::cout << "worker.read_completion_count=" << main_worker.read_completion_count << '\n';
  std::cout << "worker.read_latency_p50_us=" << to_us(percentile_sorted(latencies, 0.50)) << '\n';
  std::cout << "worker.read_latency_p95_us=" << to_us(percentile_sorted(latencies, 0.95)) << '\n';
  std::cout << "worker.read_latency_max_us="
            << to_us(latencies.empty() ? std::chrono::nanoseconds{0} : latencies.back()) << '\n';
  std::cout << "worker.descriptor_latency_us=" << to_us(main_worker.descriptor_latency) << '\n';
  std::cout << "worker.open_latency_us=" << to_us(main_worker.open_latency) << '\n';
  std::cout << "worker.queue_high_water=" << main_worker.event_queue_high_water_mark << '\n';
  std::cout << "worker.events_dropped=" << main_worker.events_dropped_due_to_capacity << '\n';
  std::cout << "worker.samples_dropped=" << main_worker.samples_dropped_due_to_capacity << '\n';
  std::cout << "worker.control_wait_count=" << main_worker.control_wait_count << '\n';
  std::cout << "mailbox.published=" << main_mailbox.published << '\n';
  std::cout << "mailbox.evicted=" << main_mailbox.evicted << '\n';
  std::cout << "mailbox.high_water=" << main_mailbox.high_water_mark << '\n';
  std::cout << "mailbox.current_size=" << main_mailbox.current_size << '\n';
  std::cout << "harness.visual_misses=" << main_misses << '\n';
  std::cout << "seek_epoch.available=" << (seek_available ? 1 : 0) << '\n';
  std::cout << "seek_epoch.completed=" << (seek_completed ? 1 : 0) << '\n';
  std::cout << "seek_epoch.read_exercised=" << (seek_read_exercised ? 1 : 0) << '\n';
  std::cout << "seek_epoch.healthy=" << (seek_healthy ? 1 : 0) << '\n';
  std::cout << "seek_epoch.error_message=" << seek_error << '\n';
  std::cout << "seek_epoch.target_num=" << target_seconds.numerator() << '\n';
  std::cout << "seek_epoch.target_den=" << target_seconds.denominator() << '\n';
  std::cout << "seek_epoch.starvation_count=" << epoch_sched.starvation_count << '\n';
  std::cout << "seek_epoch.skipped_presentation_count=" << epoch_sched.skipped_presentation_count
            << '\n';
  std::cout << "seek_epoch.read_request_delta=" << epoch_read_delta << '\n';
  std::cout << "seek_epoch.read_completion_delta=" << epoch_completion_delta << '\n';
  std::cout << "seek_epoch.mailbox_published_delta="
            << (epoch_mail_after.published - epoch_mail_before.published) << '\n';
  std::cout << "seek_epoch.mailbox_evicted_delta="
            << (epoch_mail_after.evicted - epoch_mail_before.evicted) << '\n';
  std::cout << "seek_epoch.mailbox_current_size=" << epoch_mail_after.current_size << '\n';
  std::cout << "seek_epoch.visual_misses=" << epoch_misses << '\n';
  if (!main_healthy) {
    std::cerr << "Playback metrics failed: main playback entered error: " << main_error << '\n';
    return 2;
  }
  if (seek_available && (!seek_read_exercised || !seek_healthy)) {
    std::cerr << "Playback metrics failed: seek epoch not healthy\n";
    return 2;
  }
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
