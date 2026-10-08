#pragma once

#include <pulsar/application/export/offline_video_renderer.hpp>

#include <memory>

namespace pulsar::media::ffmpeg {

// Export-quality RGBA rescaling through libswscale (Lanczos). One instance per export thread:
// it keeps a cached scaling context for the last source/target geometry.
class FFmpegOfflineFrameScaler final : public application::exporting::OfflineFrameScaler {
public:
  FFmpegOfflineFrameScaler();
  ~FFmpegOfflineFrameScaler() override;
  FFmpegOfflineFrameScaler(const FFmpegOfflineFrameScaler&) = delete;
  FFmpegOfflineFrameScaler& operator=(const FFmpegOfflineFrameScaler&) = delete;

  [[nodiscard]] application::exporting::OfflineVideoPixelBuffer scale(
      const application::exporting::OfflineVideoPixelBuffer& source, int target_width,
      int target_height) override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace pulsar::media::ffmpeg
