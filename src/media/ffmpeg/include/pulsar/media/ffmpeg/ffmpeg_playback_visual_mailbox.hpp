#pragma once

#include <pulsar/media/ffmpeg/decoded_visual_video_frame.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>

namespace pulsar::media::ffmpeg {

class FFmpegPlaybackVisualMailbox final {
public:
  using SourceEpoch = std::uint64_t;

  explicit FFmpegPlaybackVisualMailbox(std::size_t capacity = 8);

  FFmpegPlaybackVisualMailbox(const FFmpegPlaybackVisualMailbox&) = delete;
  FFmpegPlaybackVisualMailbox& operator=(const FFmpegPlaybackVisualMailbox&) = delete;

  [[nodiscard]] SourceEpoch activate();
  void deactivate(SourceEpoch epoch);
  void publish(SourceEpoch epoch, std::uint64_t ordinal, DecodedVisualVideoFrame frame);
  [[nodiscard]] std::optional<DecodedVisualVideoFrame> take(SourceEpoch epoch,
                                                             std::uint64_t ordinal);
  void clear();
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
  [[nodiscard]] SourceEpoch active_epoch() const noexcept;

private:
  struct Entry {
    SourceEpoch epoch;
    std::uint64_t ordinal;
    DecodedVisualVideoFrame frame;
  };

  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::deque<Entry> entries_;
  SourceEpoch next_epoch_{1};
  SourceEpoch active_epoch_{0};
};

} // namespace pulsar::media::ffmpeg
