#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>

#include "internal/ffmpeg_common.hpp"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
}

#include <filesystem>
#include <optional>

namespace pulsar::media::ffmpeg {
namespace {

[[nodiscard]] std::optional<domain::FrameRate> convert_frame_rate(AVRational rational) {
  if (rational.num <= 0 || rational.den <= 0) {
    return std::nullopt;
  }

  return domain::FrameRate{rational.num, rational.den};
}
[[nodiscard]] application::VideoStreamInfo video_stream_info(const AVStream& stream) {
  const AVCodecParameters& codec_parameters = *stream.codecpar;
  const domain::Rational time_base = internal::convert_required_time_base(stream.time_base);

  return application::VideoStreamInfo{
      static_cast<int>(stream.index),
      internal::codec_name(codec_parameters),
      codec_parameters.width,
      codec_parameters.height,
      time_base,
      internal::convert_duration(stream.duration, time_base),
      convert_frame_rate(stream.avg_frame_rate),
  };
}

[[nodiscard]] application::AudioStreamInfo audio_stream_info(const AVStream& stream) {
  const AVCodecParameters& codec_parameters = *stream.codecpar;
  const domain::Rational time_base = internal::convert_required_time_base(stream.time_base);

  return application::AudioStreamInfo{
      static_cast<int>(stream.index),
      internal::codec_name(codec_parameters),
      codec_parameters.sample_rate,
      codec_parameters.ch_layout.nb_channels,
      time_base,
      internal::convert_duration(stream.duration, time_base),
  };
}

} // namespace

application::MediaProbeResult FFmpegMediaProbe::probe(const std::filesystem::path& path) const {
  const std::filesystem::path canonical_path = internal::canonical_local_regular_file(path);

  internal::FormatContextPtr context = internal::open_input_with_stream_info(canonical_path);

  application::MediaProbeResult result;
  result.path = canonical_path;

  if (context->iformat != nullptr && context->iformat->name != nullptr) {
    result.container_name = context->iformat->name;
  }

  result.duration = internal::convert_duration(context->duration, domain::Rational{1, AV_TIME_BASE});

  for (unsigned int index = 0; index < context->nb_streams; ++index) {
    const AVStream& stream = *context->streams[index];
    if (stream.codecpar == nullptr) {
      continue;
    }

    if (stream.codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      result.video_streams.push_back(video_stream_info(stream));
    } else if (stream.codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      result.audio_streams.push_back(audio_stream_info(stream));
    }
  }

  return result;
}

} // namespace pulsar::media::ffmpeg
