#include "ffmpeg_common.hpp"

#include <pulsar/application/media/media_probe_error.hpp>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
}

#include <array>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace pulsar::media::ffmpeg::internal {
namespace {

using application::MediaProbeError;
using application::MediaProbeErrorCode;

[[nodiscard]] std::string ffmpeg_path_text(const std::filesystem::path& path) {
#if defined(_WIN32)
  // libavformat's Windows URL parser recognizes a fully-qualified DOS path
  // (`C:/...`) as a local file. Use generic UTF-8 separators instead of the
  // native `C:\\...` form; this also keeps non-ASCII paths intact.
  const auto value = path.generic_u8string();
  std::string text{reinterpret_cast<const char*>(value.data()), value.size()};
  if (text.starts_with("//?/")) text.erase(0, 4);
  return text;
#else
  return path.generic_string();
#endif
}

[[nodiscard]] bool is_explicit_protocol_like_input(const std::filesystem::path& path) {
  const std::string text = path.generic_string();
  if (text.rfind("file:", 0) == 0 || text.find("://") != std::string::npos) return true;

  // std::filesystem normalization collapses the two slashes of a URI scheme
  // on every platform ("https://..." becomes "https:/..."). Treat any
  // non-drive-letter scheme as a protocol while keeping ordinary local paths
  // such as "C:/media/file.mp4" local.
  const auto colon = text.find(':');
  return colon != std::string::npos && colon + 1 < text.size() &&
         text[colon + 1] == '/' &&
         (colon != 1 || !std::isalpha(static_cast<unsigned char>(text[0])));
}

} // namespace

void FormatContextDeleter::operator()(AVFormatContext* context) const noexcept {
  if (context != nullptr) {
    avformat_close_input(&context);
  }
}

std::string ffmpeg_error_text(int error_code) {
  std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
  if (av_strerror(error_code, buffer.data(), buffer.size()) < 0) {
    return "unknown FFmpeg error";
  }

  return buffer.data();
}

std::filesystem::path canonical_local_regular_file(const std::filesystem::path& path) {
  if (is_explicit_protocol_like_input(path)) {
    throw MediaProbeError{MediaProbeErrorCode::RemoteResourceNotAllowed,
                          "media operations accept local filesystem paths only"};
  }

  std::error_code error;
  const bool path_exists = std::filesystem::exists(path, error);
  if (error) {
    throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                          "failed to inspect media path: " + error.message()};
  }
  if (!path_exists) {
    throw MediaProbeError{MediaProbeErrorCode::FileNotFound, "media file does not exist"};
  }

  const bool path_is_regular = std::filesystem::is_regular_file(path, error);
  if (error) {
    throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                          "failed to inspect media file type: " + error.message()};
  }
  if (!path_is_regular) {
    throw MediaProbeError{MediaProbeErrorCode::NotRegularFile,
                          "media path is not a regular file"};
  }

  const std::filesystem::path canonical_path = std::filesystem::canonical(path, error);
  if (error) {
    throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                          "failed to resolve local media path: " + error.message()};
  }

  const bool canonical_path_is_regular = std::filesystem::is_regular_file(canonical_path, error);
  if (error) {
    throw MediaProbeError{MediaProbeErrorCode::ProbeFailed,
                          "failed to inspect resolved media path: " + error.message()};
  }
  if (!canonical_path_is_regular) {
    throw MediaProbeError{MediaProbeErrorCode::NotRegularFile,
                          "resolved media path is not a regular file"};
  }

  return canonical_path;
}

domain::Rational convert_required_time_base(AVRational rational) {
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

std::optional<domain::MediaTime> convert_timestamp(std::int64_t timestamp,
                                                   domain::Rational time_base) {
  if (timestamp == AV_NOPTS_VALUE) {
    return std::nullopt;
  }

  return domain::MediaTime{timestamp, time_base};
}

std::optional<domain::MediaTime> convert_duration(std::int64_t duration,
                                                  domain::Rational time_base) {
  if (duration == AV_NOPTS_VALUE || duration < 0) {
    return std::nullopt;
  }

  return domain::MediaTime{duration, time_base};
}

std::string codec_name(const AVCodecParameters& codec_parameters) {
  const char* name = avcodec_get_name(codec_parameters.codec_id);
  if (name == nullptr) {
    return "unknown";
  }

  return name;
}

FormatContextPtr open_input_with_stream_info(const std::filesystem::path& path) {
  AVFormatContext* raw_context = nullptr;
  const std::string path_text = ffmpeg_path_text(path);
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

} // namespace pulsar::media::ffmpeg::internal
