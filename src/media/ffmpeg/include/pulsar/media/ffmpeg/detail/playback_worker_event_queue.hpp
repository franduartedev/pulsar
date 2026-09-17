#pragma once
#include <pulsar/application/playback/playback_worker.hpp>
#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <variant>
namespace pulsar::media::ffmpeg::detail {
class PlaybackWorkerEventQueue final {
public:
  struct Stats { std::size_t samples_dropped{0}; std::size_t events_dropped{0}; std::size_t control_wait_count{0}; std::size_t high_water_mark{0}; };
  explicit PlaybackWorkerEventQueue(std::size_t capacity) : capacity_{capacity} { if (capacity_ == 0) throw std::invalid_argument{"playback worker event queue capacity must be positive"}; }
  PlaybackWorkerEventQueue(const PlaybackWorkerEventQueue&) = delete;
  PlaybackWorkerEventQueue& operator=(const PlaybackWorkerEventQueue&) = delete;
  bool enqueue(application::PlaybackWorkerEvent event) {
    const bool sample = std::holds_alternative<application::PlaybackWorkerSample>(event);
    std::unique_lock lock{mutex_};
    while (events_.size() >= capacity_ && !stopped_) {
      const auto disposable = std::find_if(events_.begin(), events_.end(), [](const auto& queued) { return std::holds_alternative<application::PlaybackWorkerSample>(queued); });
      if (disposable != events_.end()) { events_.erase(disposable); ++stats_.events_dropped; ++stats_.samples_dropped; break; }
      if (sample) { ++stats_.events_dropped; ++stats_.samples_dropped; return true; }
      ++stats_.control_wait_count;
      space_.wait(lock);
    }
    if (stopped_) return false;
    events_.push_back(std::move(event));
    stats_.high_water_mark = std::max(stats_.high_water_mark, events_.size());
    return true;
  }
  [[nodiscard]] std::optional<application::PlaybackWorkerEvent> pop() {
    std::lock_guard lock{mutex_};
    if (events_.empty()) return std::nullopt;
    auto event = std::move(events_.front()); events_.pop_front(); space_.notify_one(); return event;
  }
  void stop() { { std::lock_guard lock{mutex_}; stopped_ = true; } space_.notify_all(); }
  [[nodiscard]] std::size_t size() const noexcept { std::lock_guard lock{mutex_}; return events_.size(); }
  [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
  [[nodiscard]] bool stopped() const noexcept { std::lock_guard lock{mutex_}; return stopped_; }
  [[nodiscard]] Stats stats() const noexcept { std::lock_guard lock{mutex_}; return stats_; }
private:
  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable space_;
  std::deque<application::PlaybackWorkerEvent> events_;
  Stats stats_;
  bool stopped_{false};
};
} // namespace pulsar::media::ffmpeg::detail
