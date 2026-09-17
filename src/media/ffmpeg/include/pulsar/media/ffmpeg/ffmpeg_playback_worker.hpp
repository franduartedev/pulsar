#pragma once

#include <pulsar/application/playback/playback_worker.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_playback_visual_mailbox.hpp>
#include <pulsar/media/ffmpeg/detail/latency_sample_ring.hpp>
#include <pulsar/media/ffmpeg/detail/playback_worker_event_queue.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace pulsar::media::ffmpeg {

struct FFmpegPlaybackWorkerStats {
  std::size_t read_request_count{0};
  std::size_t seek_request_count{0};
  std::size_t events_dropped_due_to_capacity{0};
  std::size_t event_queue_high_water_mark{0};
  std::size_t samples_dropped_due_to_capacity{0};
  std::size_t control_wait_count{0};
  std::size_t last_seek_decoded_frames{0};
  std::chrono::nanoseconds descriptor_latency{0};
  std::chrono::nanoseconds open_latency{0};
  std::chrono::nanoseconds last_read_latency{0};
  std::chrono::nanoseconds last_seek_latency{0};
  // Bounded recent read-latency observations (oldest-first snapshot) for
  // p50/p95/max reporting outside decode hot paths.
  std::array<std::chrono::nanoseconds, detail::LatencySampleRing::capacity> read_latency_ring{};
  std::size_t read_latency_count{0};
};

class FFmpegPlaybackWorker final : public application::PlaybackMediaWorker {
public:
  struct Limits {
    std::size_t event_queue_capacity{8};
  };

  FFmpegPlaybackWorker(std::filesystem::path path, int video_stream_index,
                       Limits limits,
                       std::shared_ptr<FFmpegPlaybackVisualMailbox> visual_mailbox = nullptr);
  ~FFmpegPlaybackWorker() override;

  FFmpegPlaybackWorker(const FFmpegPlaybackWorker&) = delete;
  FFmpegPlaybackWorker& operator=(const FFmpegPlaybackWorker&) = delete;

  [[nodiscard]] application::VideoSourceDescriptor describe() const override;
  void request_read(application::PlaybackGeneration generation) override;
  void request_seek_for_presentation(application::PlaybackGeneration generation,
                                     application::StreamTimestamp target) override;
  void cancel_pending() override;
  [[nodiscard]] std::optional<application::PlaybackWorkerEvent> poll_event() override;

  [[nodiscard]] std::size_t event_queue_high_water_mark() const noexcept;
  [[nodiscard]] FFmpegPlaybackWorkerStats stats() const noexcept;
  [[nodiscard]] FFmpegPlaybackVisualMailbox::SourceEpoch source_epoch() const noexcept { return source_epoch_; }

private:
  struct ReadRequest {
    application::PlaybackGeneration generation;
  };

  struct SeekRequest {
    application::PlaybackGeneration generation;
    application::StreamTimestamp target;
  };

  void run();
  void enqueue_event(application::PlaybackWorkerEvent event);
  [[nodiscard]] application::VideoSample to_sample(const DecodedVisualVideoFrame& frame);
  [[nodiscard]] application::VideoSourceError to_error(const std::exception& error) const;

  std::filesystem::path path_;
  int video_stream_index_;
  Limits limits_;
  std::chrono::nanoseconds descriptor_latency_{0};
  application::VideoSourceDescriptor descriptor_;
  mutable std::mutex mutex_;
  std::condition_variable wake_;
  bool stop_{false};
  bool cancel_requested_{false};
  std::optional<ReadRequest> pending_read_;
  std::optional<SeekRequest> pending_seek_;
  detail::PlaybackWorkerEventQueue events_;
  FFmpegPlaybackWorkerStats stats_;
  // Guarded by mutex_; recorded alongside last_read_latency.
  detail::LatencySampleRing read_latencies_;
  std::uint64_t next_ordinal_{1};
  std::shared_ptr<FFmpegPlaybackVisualMailbox> visual_mailbox_;
  FFmpegPlaybackVisualMailbox::SourceEpoch source_epoch_{0};
  std::thread worker_;
};

} // namespace pulsar::media::ffmpeg
