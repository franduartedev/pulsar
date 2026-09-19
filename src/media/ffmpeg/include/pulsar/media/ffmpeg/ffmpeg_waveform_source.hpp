#pragma once

#include <pulsar/application/waveform/waveform_cache.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_audio_decode_session.hpp>

namespace pulsar::media::ffmpeg {

class FFmpegWaveformSource final : public application::WaveformSource {
public:
  FFmpegWaveformSource(const std::filesystem::path& path, int audio_stream_index);
  [[nodiscard]] application::WaveformSourceInfo info() const override;
  [[nodiscard]] std::optional<application::AudioBlock> next_block() override;

private:
  FFmpegAudioDecodeSession session_;
};

} // namespace pulsar::media::ffmpeg
