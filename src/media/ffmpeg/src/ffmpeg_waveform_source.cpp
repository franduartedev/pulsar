#include <pulsar/media/ffmpeg/ffmpeg_waveform_source.hpp>

namespace pulsar::media::ffmpeg {

FFmpegWaveformSource::FFmpegWaveformSource(const std::filesystem::path& path, int audio_stream_index)
    : session_{path, audio_stream_index} {}

application::WaveformSourceInfo FFmpegWaveformSource::info() const {
  return {session_.sample_rate(), session_.channel_count(), session_.stream_time_base(),
          session_.stream_start_timestamp()};
}

std::optional<application::AudioBlock> FFmpegWaveformSource::next_block() {
  return session_.decode_next();
}

} // namespace pulsar::media::ffmpeg
