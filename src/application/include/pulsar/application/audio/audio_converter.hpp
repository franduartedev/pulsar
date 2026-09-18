#pragma once

#include <pulsar/application/audio/audio_output.hpp>
#include <pulsar/application/media/audio_block.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace pulsar::application {

// Private A2 PCM conversion: AudioBlock (interleaved float32, native rate
// and channels) into device-format interleaved bytes. Little-endian device
// byte order is assumed (x86_64 Linux target).
struct ConvertedAudioBlock {
  AudioDeviceFormat format;
  std::size_t frame_count{0};
  std::vector<std::uint8_t> frames;
};

class AudioConversionError : public std::invalid_argument {
public:
  explicit AudioConversionError(const std::string& message) : std::invalid_argument{message} {}
};

// Supported scope: equal channel counts (any count, no rematrixing), plus
// mono<->stereo. Anything else throws AudioConversionError; in particular
// A2 never guesses multichannel speaker layouts and never silently
// downmixes.
[[nodiscard]] ConvertedAudioBlock convert_block_for_device(const AudioBlock& source,
                                                           const AudioDeviceFormat& device);

[[nodiscard]] constexpr std::size_t device_sample_bytes(AudioSampleFormat format) noexcept {
  switch (format) {
  case AudioSampleFormat::Float32:
    return 4;
  case AudioSampleFormat::Int16:
    return 2;
  case AudioSampleFormat::UInt8:
    return 1;
  }
  return 0;
}

} // namespace pulsar::application
