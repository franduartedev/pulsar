#pragma once

#include <pulsar/application/playback/audio_playback_session.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_audio_decode_session.hpp>

#include <filesystem>

namespace pulsar::media::ffmpeg {

// A3 adapter: recreating this small sequential source is the seek operation.
// The application layer never sees FFmpeg state or seek primitives.
class FFmpegAudioPlaybackSource final : public application::AudioPlaybackSource {
public:
  FFmpegAudioPlaybackSource(const std::filesystem::path& path, int audio_stream_index);

  [[nodiscard]] int sample_rate() const noexcept override;
  [[nodiscard]] int channel_count() const noexcept override;
  [[nodiscard]] std::optional<domain::MediaTime> stream_origin() const override;
  [[nodiscard]] bool seek_to(domain::MediaTime timestamp) override;
  [[nodiscard]] std::optional<application::AudioBlock> decode_next() override;

private:
  FFmpegAudioDecodeSession session_;
};

} // namespace pulsar::media::ffmpeg
