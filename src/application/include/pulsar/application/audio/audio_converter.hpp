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
  AudioDeviceFormat format{0, 0, AudioSampleFormat::Float32};
  std::size_t frame_count{0};
  std::vector<std::uint8_t> frames;
};

// Device-independent output of the streaming resampler/rematrixer. The mix
// engine consumes this form directly; AudioOutput packing remains a separate
// boundary concern.
struct ConvertedPcmBlock {
  int sample_rate{0};
  int channel_count{0};
  std::size_t frame_count{0};
  std::vector<float> samples;
};

class AudioConversionError : public std::invalid_argument {
public:
  explicit AudioConversionError(const std::string& message) : std::invalid_argument{message} {}
};

// Persistent streaming converter for one opened output session. Resampling
// keeps fractional phase and one history frame across AudioBlocks, so
// converting consecutive pieces of a stream equals converting the
// contiguous buffer in timing and waveform continuity. Copyable: the engine
// trials conversions on a copy and commits only accepted blocks, so a
// rejected enqueue never mutates streaming state.
//
// Supported scope: equal channel counts (any count, no rematrixing), plus
// mono<->stereo. Anything else throws AudioConversionError; in particular
// A2 never guesses multichannel speaker layouts and never silently
// downmixes.
class StreamingAudioConverter {
public:
  StreamingAudioConverter(int source_rate, int source_channels, AudioDeviceFormat device);

  StreamingAudioConverter(const StreamingAudioConverter&) = default;
  StreamingAudioConverter& operator=(const StreamingAudioConverter&) = default;

  // Converts one block of the ongoing stream, advancing the lattice and
  // the sample reservoir. Blocks must be contiguous pieces of one stream.
  [[nodiscard]] ConvertedAudioBlock convert(const AudioBlock& block);
  [[nodiscard]] ConvertedPcmBlock convert_pcm(const AudioBlock& block);
  // Emits the pending tail (already-accepted input held back for want of a
  // successor sample) without mutating state; the caller resets afterwards.
  // Bounded to a few device frames. Used exactly once at drain.
  [[nodiscard]] ConvertedAudioBlock flush_tail() const;
  [[nodiscard]] ConvertedPcmBlock flush_tail_pcm() const;
  // Forgets lattice/reservoir: call on logical discontinuities (open,
  // flush, stop/discard, close) so playback never interpolates against a
  // previous audio segment. Drain does not reset while samples remain
  // pending (it flushes the tail instead, then the session ends).
  void reset() noexcept;

  [[nodiscard]] bool can_convert(int source_channels) const noexcept;

private:
  int source_rate_;
  int source_channels_;
  AudioDeviceFormat device_;
  // Absolute source-frame position of the next output lattice point,
  // counting from stream start. Monotonic; identical grids for split and
  // contiguous input regardless of block boundaries.
  double next_position_{0.0};
  // Recent mapped source samples backing interpolation, with the absolute
  // index of the first stored frame. Bounded to ~step+2 frames.
  std::vector<float> reservoir_;
  std::int64_t reservoir_base_{0};
  std::int64_t absolute_frames_{0};
};

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
