#pragma once

#include <stdexcept>
#include <string>

namespace pulsar::media::ffmpeg {

enum class MediaDecodeErrorCode {
  InvalidStreamIndex,
  SelectedStreamIsNotVideo,
  SelectedStreamIsNotAudio,
  DecoderUnavailable,
  DecoderInitializationFailed,
  DemuxReadFailed,
  DecodeFailed,
  InvalidSeekTarget,
  SeekUnsupported,
  SeekFailed,
};

class MediaDecodeError : public std::runtime_error {
public:
  MediaDecodeError(MediaDecodeErrorCode code, std::string message);

  [[nodiscard]] constexpr MediaDecodeErrorCode code() const noexcept {
    return code_;
  }

private:
  MediaDecodeErrorCode code_;
};

} // namespace pulsar::media::ffmpeg
