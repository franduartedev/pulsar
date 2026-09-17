#pragma once

#include <pulsar/application/playback/playback_worker.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>

namespace pulsar::media::ffmpeg {

class FFmpegPlaybackWorker final : public application::PlaybackMediaWorker {
public:
  struct Limits {
    std::size_t event_queue_capacity{8};
  };

  FFmpegPlaybackWorker(std::filesystem::path path, int video_stream_index,
                       Limits limits);
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

private:
  struct ReadRequest {
    application::PlaybackGeneration generation;
  };

  struct SeekRequest {
    application::PlaybackGeneration generation;
    application::StreamTimestamp target;
  };

  enum class ActiveKind {
    None,
    Read,
    Seek,
  };

  void run();
  void enqueue_event(application::PlaybackWorkerEvent event);
  [[nodiscard]] application::VideoSample to_sample(const DecodedVideoFrameInfo& frame);
  [[nodiscard]] application::VideoSourceError to_error(const std::exception& error) const;

  std::filesystem::path path_;
  int video_stream_index_;
  Limits limits_;
  application::VideoSourceDescriptor descriptor_;
  mutable std::mutex mutex_;
  std::condition_variable wake_;
  bool stop_{false};
  bool cancel_requested_{false};
  std::optional<ReadRequest> pending_read_;
  std::optional<SeekRequest> active_seek_;
  std::optional<SeekRequest> pending_seek_;
  std::deque<application::PlaybackWorkerEvent> events_;
  std::size_t event_queue_high_water_mark_{0};
  std::uint64_t next_ordinal_{1};
  std::thread worker_;
};

} // namespace pulsar::media::ffmpeg
