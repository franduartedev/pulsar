#pragma once

#include <pulsar/application/export/offline_video_renderer.hpp>

namespace pulsar::media::ffmpeg {

class FFmpegOfflineVideoSource final : public application::exporting::OfflineVideoSource {
public:
  FFmpegOfflineVideoSource(const std::filesystem::path& path, int video_stream_index);
  ~FFmpegOfflineVideoSource() override;

  FFmpegOfflineVideoSource(const FFmpegOfflineVideoSource&) = delete;
  FFmpegOfflineVideoSource& operator=(const FFmpegOfflineVideoSource&) = delete;

  [[nodiscard]] application::exporting::OfflineVideoSeekResult
  seek_to_presentation_timestamp(domain::MediaTime target) override;
  [[nodiscard]] std::optional<application::exporting::OfflineVideoSourceFrame>
  decode_next() override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

class FFmpegOfflineVideoSourceFactory final
    : public application::exporting::OfflineVideoSourceFactory {
public:
  [[nodiscard]] std::unique_ptr<application::exporting::OfflineVideoSource> open(
      const std::filesystem::path& source_locator, int video_stream_index) override;
};

} // namespace pulsar::media::ffmpeg
