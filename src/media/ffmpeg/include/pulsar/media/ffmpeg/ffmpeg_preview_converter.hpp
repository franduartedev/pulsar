#pragma once

#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/media/ffmpeg/decoded_visual_video_frame.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace pulsar::media::ffmpeg {

enum class FFmpegPreviewPixelFormat {
  Rgba8888,
};

struct FFmpegPreviewFrame {
  int width;
  int height;
  int stride_bytes;
  FFmpegPreviewPixelFormat pixel_format;
  std::vector<std::uint8_t> pixels;
  std::optional<domain::MediaTime> presentation_timestamp;
  FFmpegVideoImageDescriptor source_image;
  std::chrono::nanoseconds conversion_time{0};
};

class FFmpegPreviewConverter {
public:
  [[nodiscard]] FFmpegPreviewFrame convert(const DecodedVisualVideoFrame& frame) const;
};

} // namespace pulsar::media::ffmpeg
