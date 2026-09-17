#include <pulsar/media/ffmpeg/ffmpeg_playback_visual_mailbox.hpp>

#include <stdexcept>

namespace pulsar::media::ffmpeg {

FFmpegPlaybackVisualMailbox::FFmpegPlaybackVisualMailbox(std::size_t capacity)
    : capacity_{capacity} {
  if (capacity_ == 0) {
    throw std::invalid_argument{"visual mailbox capacity must be positive"};
  }
}

FFmpegPlaybackVisualMailbox::SourceEpoch FFmpegPlaybackVisualMailbox::activate() {
  std::lock_guard lock{mutex_};
  active_epoch_ = next_epoch_++;
  entries_.clear();
  return active_epoch_;
}

void FFmpegPlaybackVisualMailbox::deactivate(SourceEpoch epoch) {
  std::lock_guard lock{mutex_};
  if (active_epoch_ == epoch) {
    active_epoch_ = 0;
    entries_.clear();
  }
}

void FFmpegPlaybackVisualMailbox::publish(SourceEpoch epoch, std::uint64_t ordinal,
                                          DecodedVisualVideoFrame frame) {
  std::lock_guard lock{mutex_};
  if (epoch == 0 || epoch != active_epoch_) {
    return;
  }
  while (entries_.size() >= capacity_) {
    entries_.pop_front();
    ++evicted_;
  }
  entries_.push_back(Entry{epoch, ordinal, std::move(frame)});
  ++published_;
  if (entries_.size() > high_water_mark_) {
    high_water_mark_ = entries_.size();
  }
}

std::optional<DecodedVisualVideoFrame> FFmpegPlaybackVisualMailbox::take(SourceEpoch epoch,
                                                                         std::uint64_t ordinal) {
  std::lock_guard lock{mutex_};
  if (epoch == 0 || epoch != active_epoch_) {
    return std::nullopt;
  }
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->epoch == epoch && it->ordinal == ordinal) {
      auto frame = std::move(it->frame);
      entries_.erase(it);
      return frame;
    }
  }
  return std::nullopt;
}

void FFmpegPlaybackVisualMailbox::clear() {
  std::lock_guard lock{mutex_};
  entries_.clear();
}

std::size_t FFmpegPlaybackVisualMailbox::size() const noexcept {
  std::lock_guard lock{mutex_};
  return entries_.size();
}

FFmpegPlaybackVisualMailbox::SourceEpoch FFmpegPlaybackVisualMailbox::active_epoch() const noexcept {
  std::lock_guard lock{mutex_};
  return active_epoch_;
}

FFmpegPlaybackVisualMailbox::Stats FFmpegPlaybackVisualMailbox::stats() const noexcept {
  std::lock_guard lock{mutex_};
  return Stats{published_, evicted_, high_water_mark_, entries_.size()};
}

} // namespace pulsar::media::ffmpeg
