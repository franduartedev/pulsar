#include <pulsar/media/ffmpeg/ffmpeg_playback_worker.hpp>

#include <pulsar/media/ffmpeg/media_decode_error.hpp>

#include <chrono>
#include <exception>
#include <stdexcept>
#include <utility>
#include <type_traits>

namespace pulsar::media::ffmpeg {
namespace {

[[nodiscard]] application::VideoSourceDescriptor describe_source(
    const std::filesystem::path& path, int video_stream_index,
    std::chrono::nanoseconds* elapsed = nullptr) {
  const auto start = std::chrono::steady_clock::now();
  auto session = FFmpegVideoDecodeSession{path, video_stream_index};
  auto descriptor = application::VideoSourceDescriptor{
      session.stream_start_timestamp().has_value()
          ? std::optional<application::StreamTimestamp>{
                application::StreamTimestamp{*session.stream_start_timestamp()}}
          : std::nullopt,
      std::nullopt,
      std::nullopt,
      true,
  };
  if (elapsed != nullptr) {
    *elapsed = std::chrono::steady_clock::now() - start;
  }
  return descriptor;
}

} // namespace

FFmpegPlaybackWorker::FFmpegPlaybackWorker(
    std::filesystem::path path, int video_stream_index, Limits limits,
    std::shared_ptr<FFmpegPlaybackVisualMailbox> visual_mailbox)
    : path_{std::move(path)},
      video_stream_index_{video_stream_index},
      limits_{limits},
      descriptor_{describe_source(path_, video_stream_index_, &descriptor_latency_)},
      visual_mailbox_{std::move(visual_mailbox)} {
  if (limits_.event_queue_capacity == 0) {
    throw std::invalid_argument{"playback worker event queue capacity must be positive"};
  }
  stats_.descriptor_latency = descriptor_latency_;
  if (visual_mailbox_) {
    source_epoch_ = visual_mailbox_->activate();
  }
  worker_ = std::thread{&FFmpegPlaybackWorker::run, this};
}

FFmpegPlaybackWorker::~FFmpegPlaybackWorker() {
  {
    std::lock_guard lock{mutex_};
    stop_ = true;
    cancel_requested_ = true;
    pending_read_.reset();
    pending_seek_.reset();
  }
  wake_.notify_one();
  event_space_.notify_all();
  if (visual_mailbox_) {
    visual_mailbox_->deactivate(source_epoch_);
  }
  if (worker_.joinable()) {
    worker_.join();
  }
}

application::VideoSourceDescriptor FFmpegPlaybackWorker::describe() const {
  return descriptor_;
}

void FFmpegPlaybackWorker::request_read(application::PlaybackGeneration generation) {
  {
    std::lock_guard lock{mutex_};
    pending_read_ = ReadRequest{generation};
    ++stats_.read_request_count;
  }
  wake_.notify_one();
}

void FFmpegPlaybackWorker::request_seek_for_presentation(application::PlaybackGeneration generation,
                                                        application::StreamTimestamp target) {
  {
    std::lock_guard lock{mutex_};
    pending_seek_ = SeekRequest{generation, target};
    pending_read_.reset();
    ++stats_.seek_request_count;
  }
  wake_.notify_one();
}

void FFmpegPlaybackWorker::cancel_pending() {
  {
    std::lock_guard lock{mutex_};
    cancel_requested_ = true;
    pending_read_.reset();
    pending_seek_.reset();
  }
  wake_.notify_one();
}

std::optional<application::PlaybackWorkerEvent> FFmpegPlaybackWorker::poll_event() {
  std::lock_guard lock{mutex_};
  if (events_.empty()) {
    return std::nullopt;
  }

  auto event = std::move(events_.front());
  events_.pop_front();
  event_space_.notify_one();
  return event;
}

std::size_t FFmpegPlaybackWorker::event_queue_high_water_mark() const noexcept {
  std::lock_guard lock{mutex_};
  return stats_.event_queue_high_water_mark;
}

FFmpegPlaybackWorkerStats FFmpegPlaybackWorker::stats() const noexcept {
  std::lock_guard lock{mutex_};
  return stats_;
}

void FFmpegPlaybackWorker::run() {
  try {
    const auto open_start = std::chrono::steady_clock::now();
    auto session = FFmpegVideoDecodeSession{path_, video_stream_index_};
    {
      std::lock_guard lock{mutex_};
      stats_.open_latency = std::chrono::steady_clock::now() - open_start;
    }

    while (true) {
      std::optional<SeekRequest> seek;
      std::optional<ReadRequest> read;
      std::optional<application::PlaybackWorkerCancelled> cancelled;
      {
        std::unique_lock lock{mutex_};
        wake_.wait(lock, [this] {
          return stop_ || pending_seek_.has_value() || pending_read_.has_value() ||
                 cancel_requested_;
        });

        if (stop_) {
          return;
        }
        if (cancel_requested_) {
          cancel_requested_ = false;
          if (pending_seek_.has_value()) {
            cancelled = application::PlaybackWorkerCancelled{pending_seek_->generation};
          } else if (pending_read_.has_value()) {
            cancelled = application::PlaybackWorkerCancelled{pending_read_->generation};
          }
          pending_seek_.reset();
          pending_read_.reset();
        } else if (pending_seek_.has_value()) {
          seek = pending_seek_;
          pending_seek_.reset();
          pending_read_.reset();
        } else if (pending_read_.has_value()) {
          read = pending_read_;
          pending_read_.reset();
        }
      }

      if (cancelled.has_value()) {
        enqueue_event(*cancelled);
        continue;
      }

      if (seek.has_value()) {
        try {
          const auto seek_start = std::chrono::steady_clock::now();
          const auto result = session.seek_to_presentation_timestamp_visual(seek->target.value());
          {
            std::lock_guard lock{mutex_};
            stats_.last_seek_latency = std::chrono::steady_clock::now() - seek_start;
            stats_.last_seek_decoded_frames = result.frames_decoded_after_seek;
          }
          if (result.frame.has_value()) {
            application::VideoSeekHit hit{to_sample(*result.frame), std::nullopt};
            if (result.lookahead.has_value()) {
              hit.lookahead = to_sample(*result.lookahead);
            }
            enqueue_event(application::PlaybackWorkerSeekComplete{seek->generation, std::move(hit)});
          } else {
            application::VideoSeekBeforeFirst before_first;
            if (result.lookahead.has_value()) {
              before_first.first_sample = to_sample(*result.lookahead);
            }
            enqueue_event(application::PlaybackWorkerSeekComplete{seek->generation,
                                                                  std::move(before_first)});
          }
        } catch (const std::exception& error) {
          enqueue_event(application::PlaybackWorkerError{seek->generation, to_error(error)});
        }
        continue;
      }

      if (read.has_value()) {
        try {
          const auto read_start = std::chrono::steady_clock::now();
          if (auto frame = session.decode_next_visual()) {
            {
              std::lock_guard lock{mutex_};
              stats_.last_read_latency = std::chrono::steady_clock::now() - read_start;
            }
            enqueue_event(application::PlaybackWorkerSample{read->generation, to_sample(*frame)});
          } else {
            {
              std::lock_guard lock{mutex_};
              stats_.last_read_latency = std::chrono::steady_clock::now() - read_start;
            }
            enqueue_event(application::PlaybackWorkerEndOfStream{
                read->generation, application::VideoSourceEndOfStream{std::nullopt, false}});
          }
        } catch (const std::exception& error) {
          enqueue_event(application::PlaybackWorkerError{read->generation, to_error(error)});
        }
      }
    }
  } catch (const std::exception& error) {
    enqueue_event(application::PlaybackWorkerError{0, to_error(error)});
  }
}

void FFmpegPlaybackWorker::enqueue_event(application::PlaybackWorkerEvent event) {
  const bool sample = std::holds_alternative<application::PlaybackWorkerSample>(event);
  std::unique_lock lock{mutex_};
  while (events_.size() >= limits_.event_queue_capacity && !stop_) {
    auto disposable = std::find_if(events_.begin(), events_.end(), [](const auto& queued) {
      return std::holds_alternative<application::PlaybackWorkerSample>(queued);
    });
    if (disposable != events_.end()) {
      events_.erase(disposable);
      ++stats_.events_dropped_due_to_capacity;
      ++stats_.samples_dropped_due_to_capacity;
      break;
    }
    if (sample) {
      ++stats_.events_dropped_due_to_capacity;
      ++stats_.samples_dropped_due_to_capacity;
      return;
    }
    ++stats_.control_wait_count;
    event_space_.wait(lock);
  }
  if (stop_) {
    return;
  }
  events_.push_back(std::move(event));
  if (events_.size() > stats_.event_queue_high_water_mark) {
    stats_.event_queue_high_water_mark = events_.size();
  }
}

application::VideoSample FFmpegPlaybackWorker::to_sample(const DecodedVisualVideoFrame& frame) {
  const auto ordinal = next_ordinal_++;
  if (visual_mailbox_) {
    visual_mailbox_->publish(source_epoch_, ordinal, frame);
  }
  return application::VideoSample{
      ordinal,
      frame.presentation_timestamp.has_value()
          ? std::optional<application::StreamTimestamp>{
                application::StreamTimestamp{*frame.presentation_timestamp}}
          : std::nullopt,
      std::nullopt,
  };
}

application::VideoSourceError FFmpegPlaybackWorker::to_error(const std::exception& error) const {
  return application::VideoSourceError{error.what()};
}

} // namespace pulsar::media::ffmpeg
