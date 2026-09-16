#pragma once

#include <pulsar/domain/time/media_time.hpp>

#include <optional>
#include <string>

namespace pulsar::media::ffmpeg {

struct DecodedVideoFrameInfo {
  int stream_index;
  int width;
  int height;
  std::optional<std::string> pixel_format_name;
  std::optional<domain::MediaTime> presentation_timestamp;
};

} // namespace pulsar::media::ffmpeg
