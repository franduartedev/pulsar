#pragma once

#include <pulsar/application/export/offline_video_renderer.hpp>
#include <pulsar/media/ffmpeg/decoded_visual_video_frame.hpp>

#include <memory>

namespace pulsar::media::ffmpeg {

class FFmpegOfflineVideoFrameConverter final {
public:
  FFmpegOfflineVideoFrameConverter();
  ~FFmpegOfflineVideoFrameConverter();

  FFmpegOfflineVideoFrameConverter(const FFmpegOfflineVideoFrameConverter&) = delete;
  FFmpegOfflineVideoFrameConverter& operator=(const FFmpegOfflineVideoFrameConverter&) = delete;

  [[nodiscard]] application::exporting::OfflineVideoSourceFrame convert(
      const DecodedVisualVideoFrame& frame) const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace pulsar::media::ffmpeg
