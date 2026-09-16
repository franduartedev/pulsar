#pragma once

#include <pulsar/domain/time/frame_rate.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pulsar::application {

struct VideoStreamInfo {
  int stream_index;
  std::string codec_name;
  int width;
  int height;
  domain::Rational time_base;
  std::optional<domain::MediaTime> duration;
  std::optional<domain::FrameRate> nominal_frame_rate;
};

struct AudioStreamInfo {
  int stream_index;
  std::string codec_name;
  int sample_rate;
  int channel_count;
  domain::Rational time_base;
  std::optional<domain::MediaTime> duration;
};

struct MediaProbeResult {
  std::filesystem::path path;
  std::optional<std::string> container_name;
  std::optional<domain::MediaTime> duration;
  std::vector<VideoStreamInfo> video_streams;
  std::vector<AudioStreamInfo> audio_streams;
};

} // namespace pulsar::application
