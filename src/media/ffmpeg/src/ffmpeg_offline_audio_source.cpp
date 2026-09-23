#include <pulsar/media/ffmpeg/ffmpeg_offline_audio_source.hpp>

#include <pulsar/media/ffmpeg/ffmpeg_audio_decode_session.hpp>
#include <pulsar/media/ffmpeg/media_decode_error.hpp>

#include <stdexcept>
#include <utility>

namespace pulsar::media::ffmpeg {
namespace {

using application::exporting::OfflineAudioRenderError;
using application::exporting::OfflineAudioRenderErrorCode;

[[noreturn]] void rethrow_media_error(const MediaDecodeError& error) {
  auto code = OfflineAudioRenderErrorCode::DecodeFailed;
  switch (error.code()) {
  case MediaDecodeErrorCode::InvalidSeekTarget:
  case MediaDecodeErrorCode::SeekUnsupported:
  case MediaDecodeErrorCode::SeekFailed:
    code = OfflineAudioRenderErrorCode::SeekFailed;
    break;
  case MediaDecodeErrorCode::InvalidStreamIndex:
  case MediaDecodeErrorCode::SelectedStreamIsNotAudio:
  case MediaDecodeErrorCode::DecoderUnavailable:
  case MediaDecodeErrorCode::DecoderInitializationFailed:
    code = OfflineAudioRenderErrorCode::SourceOpenFailed;
    break;
  default:
    code = OfflineAudioRenderErrorCode::DecodeFailed;
    break;
  }
  throw OfflineAudioRenderError{code, error.what()};
}

} // namespace

class FFmpegOfflineAudioSource::Impl {
public:
  Impl(const std::filesystem::path& path, int stream_index)
      : session{path, stream_index} {}

  FFmpegAudioDecodeSession session;
};

FFmpegOfflineAudioSource::FFmpegOfflineAudioSource(const std::filesystem::path& path,
                                                   int audio_stream_index) {
  try {
    impl_ = std::make_unique<Impl>(path, audio_stream_index);
  } catch (const MediaDecodeError& error) {
    rethrow_media_error(error);
  } catch (const std::exception& error) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::SourceOpenFailed, error.what()};
  }
}

FFmpegOfflineAudioSource::~FFmpegOfflineAudioSource() = default;

int FFmpegOfflineAudioSource::sample_rate() const noexcept { return impl_->session.sample_rate(); }

int FFmpegOfflineAudioSource::channel_count() const noexcept {
  return impl_->session.channel_count();
}

std::optional<domain::MediaTime> FFmpegOfflineAudioSource::stream_origin() const {
  return impl_->session.stream_start_timestamp();
}

void FFmpegOfflineAudioSource::seek_to(domain::MediaTime stream_timestamp) {
  try {
    if (!impl_->session.seek_to(stream_timestamp)) {
      throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::SeekFailed,
                                    "FFmpeg audio seek failed"};
    }
  } catch (const OfflineAudioRenderError&) {
    throw;
  } catch (const MediaDecodeError& error) {
    rethrow_media_error(error);
  } catch (const std::exception& error) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::SeekFailed, error.what()};
  }
}

std::optional<application::exporting::OfflineAudioSourceBlock>
FFmpegOfflineAudioSource::decode_next() {
  try {
    const auto decoded = impl_->session.decode_next();
    if (!decoded.has_value()) return std::nullopt;
    return application::exporting::OfflineAudioSourceBlock{
        decoded->sample_rate(), decoded->channel_count(), decoded->frame_count(),
        decoded->samples(), decoded->presentation_timestamp()};
  } catch (const MediaDecodeError& error) {
    rethrow_media_error(error);
  } catch (const std::exception& error) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::DecodeFailed, error.what()};
  }
}

FFmpegOfflineAudioSourceFactory::FFmpegOfflineAudioSourceFactory(LocatorResolver resolver)
    : resolver_{std::move(resolver)} {
  if (!resolver_) resolver_ = [](const std::filesystem::path& locator) { return locator; };
}

std::unique_ptr<application::exporting::OfflineAudioSource>
FFmpegOfflineAudioSourceFactory::open(const std::filesystem::path& logical_locator,
                                      int audio_stream_index) {
  try {
    return std::make_unique<FFmpegOfflineAudioSource>(resolver_(logical_locator),
                                                       audio_stream_index);
  } catch (const OfflineAudioRenderError&) {
    throw;
  } catch (const std::exception& error) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::SourceOpenFailed, error.what()};
  }
}

} // namespace pulsar::media::ffmpeg
