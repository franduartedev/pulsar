#pragma once

#include <pulsar/media/ffmpeg/decoded_video_frame_info.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace pulsar::media::ffmpeg {

class FFmpegVideoDecodeSession {
public:
  FFmpegVideoDecodeSession(const std::filesystem::path& path, int video_stream_index);
  ~FFmpegVideoDecodeSession();

  FFmpegVideoDecodeSession(const FFmpegVideoDecodeSession&) = delete;
  FFmpegVideoDecodeSession& operator=(const FFmpegVideoDecodeSession&) = delete;

  FFmpegVideoDecodeSession(FFmpegVideoDecodeSession&&) noexcept;
  FFmpegVideoDecodeSession& operator=(FFmpegVideoDecodeSession&&) noexcept;

  [[nodiscard]] std::optional<DecodedVideoFrameInfo> decode_next();
  [[nodiscard]] int stream_index() const noexcept;
  [[nodiscard]] const std::string& codec_name() const noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace pulsar::media::ffmpeg
