#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>

#include <pulsar/application/media/media_probe_error.hpp>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/codec_desc.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
}

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

namespace pulsar::media::ffmpeg {
namespace {

using application::MediaProbeError;
using application::MediaProbeErrorCode;

struct FormatContextDeleter {
  void operator()(AVFormatContext* context) const noexcept {
    if (context != nullptr) {
      avformat_close_input(&context);
    }
  }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;

[[nodiscard]] std::string ffmpeg_error_text(int error_code) {
  std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
  if (av_strerror(error_code, buffer.data(), buffer.size()) < 0) {
    return "unknown FFmpeg error";
  }

  return buffer.data();
}

[[nodiscard]] bool is_explicit_protocol_like_input(const std::filesystem::path& path) {
  const std::string text = path.string();
  return text.find("://") != std::string::npos || text.rfind("file:", 0) == 0;
}

[[nodiscard]] std::filesystem::path canonical_local_regular_file(const std::filesystem::path& path) {
  if (is_explicit_protocol_like_input(path)) {
    throw MediaProbeError{MediaProbeErrorCode::RemoteResourceNotAllowed,
                          "media probe accepts local filesystem paths only"};
  }

  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    throw MediaProbeError{MediaProbeErrorCode::FileNotFound, "media file does not exist"};
  }
  if (error) {
    throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                          "failed to inspect media path: " + error.message()};
  }

  if (!std::filesystem::is_regular_file(path, error)) {
    throw MediaProbeError{MediaProbeErrorCode::NotRegularFile,
                          "media path is not a regular file"};
  }
  if (error) {
    throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                          "failed to inspect media file type: " + error.message()};
  }

  const std::filesystem::path canonical_path = std::filesystem::canonical(path, error);
  if (error) {
    throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                          "failed to resolve local media path: " + error.message()};
  }

  if (!std::filesystem::is_regular_file(canonical_path, error)) {
    throw MediaProbeError{MediaProbeErrorCode::NotRegularFile,
                          "resolved media path is not a regular file"};
  }
  if (error) {
    throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                          "failed to inspect resolved media path: " + error.message()};
  }

  return canonical_path;
}

[[nodiscard]] domain::Rational convert_required_time_base(AVRational rational) {
  try {
    const domain::Rational converted{rational.num, rational.den};
    if (converted <= domain::Rational{0, 1}) {
      throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                            "FFmpeg reported a non-positive stream time base"};
    }
    return converted;
  } catch (const std::invalid_argument& error) {
    throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                          std::string{"FFmpeg reported an invalid stream time base: "} +
                              error.what()};
  } catch (const std::overflow_error& error) {
    throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                          std::string{"FFmpeg stream time base is outside supported range: "} +
                              error.what()};
  }
}

[[nodiscard]] std::optional<domain::FrameRate> convert_frame_rate(AVRational rational) {
  if (rational.num <= 0 || rational.den <= 0) {
    return std::nullopt;
  }

  return domain::FrameRate{rational.num, rational.den};
}

[[nodiscard]] std::optional<domain::MediaTime> convert_duration(std::int64_t duration,
                                                                domain::Rational time_base) {
  if (duration == AV_NOPTS_VALUE || duration < 0) {
    return std::nullopt;
  }

  return domain::MediaTime{duration, time_base};
}

[[nodiscard]] std::string codec_name(const AVCodecParameters& codec_parameters) {
  const char* name = avcodec_get_name(codec_parameters.codec_id);
  if (name == nullptr) {
    return "unknown";
  }

  return name;
}

[[nodiscard]] application::VideoStreamInfo video_stream_info(const AVStream& stream) {
  const AVCodecParameters& codec_parameters = *stream.codecpar;
  const domain::Rational time_base = convert_required_time_base(stream.time_base);

  return application::VideoStreamInfo{
      static_cast<int>(stream.index),
      codec_name(codec_parameters),
      codec_parameters.width,
      codec_parameters.height,
      time_base,
      convert_duration(stream.duration, time_base),
      convert_frame_rate(stream.avg_frame_rate),
  };
}

[[nodiscard]] application::AudioStreamInfo audio_stream_info(const AVStream& stream) {
  const AVCodecParameters& codec_parameters = *stream.codecpar;
  const domain::Rational time_base = convert_required_time_base(stream.time_base);

  return application::AudioStreamInfo{
      static_cast<int>(stream.index),
      codec_name(codec_parameters),
      codec_parameters.sample_rate,
      codec_parameters.ch_layout.nb_channels,
      time_base,
      convert_duration(stream.duration, time_base),
  };
}

[[nodiscard]] FormatContextPtr open_input(const std::filesystem::path& path) {
  AVFormatContext* raw_context = nullptr;
  const std::string path_text = path.string();
  const int open_result = avformat_open_input(&raw_context, path_text.c_str(), nullptr, nullptr);
  if (open_result < 0) {
    throw MediaProbeError{MediaProbeErrorCode::UnsupportedMedia,
                          "FFmpeg could not open media file '" + path_text +
                              "': " + ffmpeg_error_text(open_result)};
  }

  FormatContextPtr context{raw_context};
  const int stream_info_result = avformat_find_stream_info(context.get(), nullptr);
  if (stream_info_result < 0) {
    throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                          "FFmpeg could not read stream info for '" + path_text + "': " +
                              ffmpeg_error_text(stream_info_result)};
  }

  return context;
}

} // namespace

application::MediaProbeResult FFmpegMediaProbe::probe(const std::filesystem::path& path) const {
  const std::filesystem::path canonical_path = canonical_local_regular_file(path);

  FormatContextPtr context = open_input(canonical_path);

  application::MediaProbeResult result;
  result.path = canonical_path;

  if (context->iformat != nullptr && context->iformat->name != nullptr) {
    result.container_name = context->iformat->name;
  }

  result.duration = convert_duration(context->duration, domain::Rational{1, AV_TIME_BASE});

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
