#include <pulsar/media/ffmpeg/media_decode_error.hpp>

#include <utility>

namespace pulsar::media::ffmpeg {

MediaDecodeError::MediaDecodeError(MediaDecodeErrorCode code, std::string message)
    : std::runtime_error{std::move(message)}, code_{code} {}

} // namespace pulsar::media::ffmpeg
