#pragma once

#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace pulsar::media::ffmpeg::internal {

struct FormatContextDeleter {
  void operator()(AVFormatContext* context) const noexcept;
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;

[[nodiscard]] std::string ffmpeg_error_text(int error_code);
[[nodiscard]] std::filesystem::path canonical_local_regular_file(
    const std::filesystem::path& path);
[[nodiscard]] domain::Rational convert_required_time_base(AVRational rational);
[[nodiscard]] std::optional<domain::MediaTime> convert_timestamp(std::int64_t timestamp,
                                                                 domain::Rational time_base);
[[nodiscard]] std::optional<domain::MediaTime> convert_duration(std::int64_t duration,
                                                                domain::Rational time_base);
[[nodiscard]] std::string codec_name(const AVCodecParameters& codec_parameters);
[[nodiscard]] FormatContextPtr open_input_with_stream_info(const std::filesystem::path& path);

} // namespace pulsar::media::ffmpeg::internal
