#pragma once

#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace pulsar::media::ffmpeg {

struct FFmpegVideoImageDescriptor {
  int coded_width;
  int coded_height;
  int visible_width;
  int visible_height;
  std::optional<std::string> pixel_format_name;
  std::optional<domain::Rational> sample_aspect_ratio;
  std::string color_range;
  std::string color_primaries;
  std::string color_transfer;
  std::string color_matrix;
};

class FFmpegPreviewConverter;
class FFmpegOfflineVideoSource;
class FFmpegOfflineVideoFrameConverter;

class FFmpegVideoFrameLease {
public:
  FFmpegVideoFrameLease();
  ~FFmpegVideoFrameLease();

  FFmpegVideoFrameLease(const FFmpegVideoFrameLease&) noexcept;
  FFmpegVideoFrameLease& operator=(const FFmpegVideoFrameLease&) noexcept;

  FFmpegVideoFrameLease(FFmpegVideoFrameLease&&) noexcept;
  FFmpegVideoFrameLease& operator=(FFmpegVideoFrameLease&&) noexcept;

  [[nodiscard]] bool valid() const noexcept;

private:
  class Impl;
  explicit FFmpegVideoFrameLease(std::shared_ptr<const Impl> impl) noexcept;

  std::shared_ptr<const Impl> impl_;

  friend class FFmpegVideoDecodeSession;
  friend class FFmpegPreviewConverter;
  friend class FFmpegOfflineVideoSource;
  friend class FFmpegOfflineVideoFrameConverter;
};

struct DecodedVisualVideoFrame {
  // Stable identity of the physical decoded AVFrame for one
  // FFmpegVideoDecodeSession lifetime. Re-exposing the same object (e.g. a
  // retained pending lookahead) preserves it; it is unrelated to timestamps.
  std::uint64_t physical_frame_id;
  int stream_index;
  FFmpegVideoImageDescriptor image;
  std::optional<domain::MediaTime> presentation_timestamp;
  FFmpegVideoFrameLease frame;
};

} // namespace pulsar::media::ffmpeg
