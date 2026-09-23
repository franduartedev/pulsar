#include <pulsar/media/ffmpeg/ffmpeg_offline_video_source.hpp>

#include <pulsar/media/ffmpeg/ffmpeg_offline_video_converter.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>
#include <pulsar/media/ffmpeg/media_decode_error.hpp>

#include <memory>
#include <stdexcept>
#include <utility>

namespace pulsar::media::ffmpeg {
namespace {

[[nodiscard]] application::exporting::OfflineVideoRenderError map_error(
    const MediaDecodeError& error, bool seeking) {
  using Code = application::exporting::OfflineVideoRenderErrorCode;
  if (seeking) {
    return {Code::SeekFailed, error.what()};
  }
  return {Code::DecodeFailed, error.what()};
}

} // namespace

class FFmpegOfflineVideoSource::Impl {
public:
  Impl(const std::filesystem::path& path, int video_stream_index)
      : session_{path, video_stream_index} {}

  [[nodiscard]] application::exporting::OfflineVideoSourceFrame convert(
      const DecodedVisualVideoFrame& visual) {
    auto frame = converter_.convert(visual);
    if (frame.presentation_timestamp.has_value() && stream_origin_.has_value()) {
      frame.presentation_timestamp = *frame.presentation_timestamp - *stream_origin_;
    }
    return frame;
  }

  FFmpegVideoDecodeSession session_;
  FFmpegOfflineVideoFrameConverter converter_;
  std::optional<domain::MediaTime> stream_origin_;
};

FFmpegOfflineVideoSource::FFmpegOfflineVideoSource(const std::filesystem::path& path,
                                                   int video_stream_index)
    : impl_{std::make_unique<Impl>(path, video_stream_index)} {
  impl_->stream_origin_ = impl_->session_.stream_start_timestamp();
}

FFmpegOfflineVideoSource::~FFmpegOfflineVideoSource() = default;

application::exporting::OfflineVideoSeekResult
FFmpegOfflineVideoSource::seek_to_presentation_timestamp(domain::MediaTime target) {
  try {
    const auto raw_target = impl_->stream_origin_.has_value()
                                ? *impl_->stream_origin_ + target
                                : target;
    const auto result = impl_->session_.seek_to_presentation_timestamp_visual(raw_target);
    std::optional<application::exporting::OfflineVideoSourceFrame> candidate;
    std::optional<application::exporting::OfflineVideoSourceFrame> lookahead;
    if (result.frame.has_value()) candidate = impl_->convert(*result.frame);
    if (result.lookahead.has_value()) lookahead = impl_->convert(*result.lookahead);
    if (candidate.has_value() && lookahead.has_value() &&
        candidate->presentation_timestamp.has_value() &&
        lookahead->presentation_timestamp.has_value()) {
      candidate->presentation_duration = *lookahead->presentation_timestamp -
                                         *candidate->presentation_timestamp;
    }
    return {std::move(candidate), std::move(lookahead), result.frames_decoded_after_seek};
  } catch (const application::exporting::OfflineVideoRenderError&) {
    throw;
  } catch (const MediaDecodeError& error) {
    throw map_error(error, true);
  } catch (const std::exception& error) {
    throw application::exporting::OfflineVideoRenderError{
        application::exporting::OfflineVideoRenderErrorCode::SeekFailed, error.what()};
  }
}

std::optional<application::exporting::OfflineVideoSourceFrame>
FFmpegOfflineVideoSource::decode_next() {
  try {
    const auto frame = impl_->session_.decode_next_visual();
    if (!frame.has_value()) return std::nullopt;
    return impl_->convert(*frame);
  } catch (const application::exporting::OfflineVideoRenderError&) {
    throw;
  } catch (const MediaDecodeError& error) {
    throw map_error(error, false);
  } catch (const std::exception& error) {
    throw application::exporting::OfflineVideoRenderError{
        application::exporting::OfflineVideoRenderErrorCode::DecodeFailed, error.what()};
  }
}

std::unique_ptr<application::exporting::OfflineVideoSource>
FFmpegOfflineVideoSourceFactory::open(const std::filesystem::path& source_locator,
                                      int video_stream_index) {
  try {
    return std::make_unique<FFmpegOfflineVideoSource>(source_locator, video_stream_index);
  } catch (const application::exporting::OfflineVideoRenderError&) {
    throw;
  } catch (const MediaDecodeError& error) {
    throw application::exporting::OfflineVideoRenderError{
        application::exporting::OfflineVideoRenderErrorCode::SourceOpenFailed, error.what()};
  } catch (const std::exception& error) {
    throw application::exporting::OfflineVideoRenderError{
        application::exporting::OfflineVideoRenderErrorCode::SourceOpenFailed, error.what()};
  }
}

} // namespace pulsar::media::ffmpeg
