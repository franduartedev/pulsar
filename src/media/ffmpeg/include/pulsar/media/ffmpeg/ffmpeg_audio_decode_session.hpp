#pragma once

#include <pulsar/application/media/audio_block.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace pulsar::media::ffmpeg {

// A1 sequential audio decoder. Opens exactly the requested FFmpeg audio
// stream index and yields owned interleaved float32 PCM blocks at the source
// stream's native sample rate and channel count. Single-threaded: use from
// one thread at a time. A decoder, not a player: no seeking, no clock, no
// timeline semantics, no device output.
class FFmpegAudioDecodeSession {
public:
  FFmpegAudioDecodeSession(const std::filesystem::path& path, int audio_stream_index);
  ~FFmpegAudioDecodeSession();

  FFmpegAudioDecodeSession(const FFmpegAudioDecodeSession&) = delete;
  FFmpegAudioDecodeSession& operator=(const FFmpegAudioDecodeSession&) = delete;

  FFmpegAudioDecodeSession(FFmpegAudioDecodeSession&&) noexcept;
  FFmpegAudioDecodeSession& operator=(FFmpegAudioDecodeSession&&) noexcept;

  // Returns the next owned PCM block, or std::nullopt at clean end of
  // stream. Delayed decoder and converter samples are flushed before EOF;
  // repeated calls after EOF keep returning std::nullopt.
  [[nodiscard]] std::optional<application::AudioBlock> decode_next();

  [[nodiscard]] int stream_index() const noexcept;
  [[nodiscard]] int sample_rate() const noexcept;
  [[nodiscard]] int channel_count() const noexcept;
  [[nodiscard]] const std::string& codec_name() const noexcept;
  [[nodiscard]] const domain::Rational& stream_time_base() const noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace pulsar::media::ffmpeg
