#pragma once

#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/media/ffmpeg/decoded_visual_video_frame.hpp>

#include <cstddef>
#include <optional>

namespace pulsar::media::ffmpeg {

struct VideoPresentationVisualSeekResult {
  domain::MediaTime target;
  std::optional<DecodedVisualVideoFrame> frame;
  std::optional<DecodedVisualVideoFrame> lookahead;
  std::size_t frames_decoded_after_seek;
};

} // namespace pulsar::media::ffmpeg
