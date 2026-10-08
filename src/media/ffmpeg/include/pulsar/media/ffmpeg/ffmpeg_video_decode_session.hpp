#pragma once

#include <pulsar/media/ffmpeg/decoded_video_frame_info.hpp>
#include <pulsar/media/ffmpeg/decoded_visual_video_frame.hpp>
#include <pulsar/media/ffmpeg/video_presentation_seek_result.hpp>
#include <pulsar/media/ffmpeg/video_presentation_visual_seek_result.hpp>
#include <pulsar/media/ffmpeg/video_seek_result.hpp>

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

  // The session owns one FFmpeg demuxer/decoder state machine. Use it from one thread at a time;
  // playback orchestration must not retain pointers/iterators into mutable Timeline data.
  [[nodiscard]] std::optional<DecodedVideoFrameInfo> decode_next();
  // Returns a refcounted FFmpeg-backed visual frame lease. The lease remains valid after the
  // next decode call and after this session is destroyed; callers still must not treat it as a
  // general engine frame abstraction or hide CPU/GPU mapping behind implicit accessors.
  [[nodiscard]] std::optional<DecodedVisualVideoFrame> decode_next_visual();
  [[nodiscard]] VideoSeekResult seek_to_stream_timestamp(domain::MediaTime target);
  [[nodiscard]] VideoPresentationSeekResult seek_to_presentation_timestamp(domain::MediaTime target);
  [[nodiscard]] VideoPresentationVisualSeekResult seek_to_presentation_timestamp_visual(
      domain::MediaTime target);
  [[nodiscard]] int stream_index() const noexcept;
  [[nodiscard]] const std::string& codec_name() const noexcept;
  [[nodiscard]] std::optional<domain::MediaTime> stream_start_timestamp() const;
  // A photograph or colour card: one picture that stands for every instant from its start.
  [[nodiscard]] bool is_still() const noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace pulsar::media::ffmpeg
