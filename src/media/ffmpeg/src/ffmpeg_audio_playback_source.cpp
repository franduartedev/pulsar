#include <pulsar/media/ffmpeg/ffmpeg_audio_playback_source.hpp>

namespace pulsar::media::ffmpeg {

FFmpegAudioPlaybackSource::FFmpegAudioPlaybackSource(const std::filesystem::path& path,
                                                     int audio_stream_index)
    : session_{path, audio_stream_index} {}

int FFmpegAudioPlaybackSource::sample_rate() const noexcept { return session_.sample_rate(); }

int FFmpegAudioPlaybackSource::channel_count() const noexcept {
  return session_.channel_count();
}

std::optional<domain::MediaTime> FFmpegAudioPlaybackSource::stream_origin() const {
  return session_.stream_start_timestamp();
}

bool FFmpegAudioPlaybackSource::seek_to(domain::MediaTime timestamp) {
  return session_.seek_to(timestamp);
}

std::optional<application::AudioBlock> FFmpegAudioPlaybackSource::decode_next() {
  return session_.decode_next();
}

} // namespace pulsar::media::ffmpeg
