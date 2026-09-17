#pragma once

#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/media/ffmpeg/decoded_video_frame_info.hpp>

#include <cstddef>
#include <optional>

namespace pulsar::media::ffmpeg {

struct VideoPresentationSeekResult {
  domain::MediaTime requested_timestamp;
  std::optional<DecodedVideoFrameInfo> frame;
  std::optional<DecodedVideoFrameInfo> lookahead;
  std::size_t frames_decoded_after_seek;
};

} // namespace pulsar::media::ffmpeg
