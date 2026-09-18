#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>

#include <pulsar/media/ffmpeg/decoded_visual_video_frame.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>

#include <pulsar/media/ffmpeg/media_decode_error.hpp>

#include "internal/ffmpeg_common.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <chrono>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace pulsar::media::ffmpeg {
namespace {

using internal::ffmpeg_error_text;

struct CodecContextDeleter {
  void operator()(AVCodecContext* context) const noexcept {
    if (context != nullptr) {
      avcodec_free_context(&context);
    }
  }
};

struct PacketDeleter {
  void operator()(AVPacket* packet) const noexcept {
    if (packet != nullptr) {
      av_packet_free(&packet);
    }
  }
};

struct FrameDeleter {
  void operator()(AVFrame* frame) const noexcept {
    if (frame != nullptr) {
      av_frame_free(&frame);
    }
  }
};

struct SwsContextDeleter {
  void operator()(SwsContext* context) const noexcept {
    sws_freeContext(context);
  }
};

using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

#if defined(__SIZEOF_INT128__)
using WideInt = __int128_t;
#else
#error "PULSAR requires signed __int128 support for exact seek timestamp conversion."
#endif

[[nodiscard]] CodecContextPtr allocate_decoder_context(const AVCodec& decoder,
                                                       const AVCodecParameters& parameters) {
  AVCodecContext* raw_context = avcodec_alloc_context3(&decoder);
  if (raw_context == nullptr) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                           "FFmpeg could not allocate video decoder context"};
  }

  CodecContextPtr context{raw_context};
  const int parameters_result = avcodec_parameters_to_context(context.get(), &parameters);
  if (parameters_result < 0) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                           "FFmpeg could not copy video decoder parameters: " +
                               ffmpeg_error_text(parameters_result)};
  }

  const int open_result = avcodec_open2(context.get(), &decoder, nullptr);
  if (open_result < 0) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                           "FFmpeg could not open video decoder: " +
                               ffmpeg_error_text(open_result)};
  }

  return context;
}

[[nodiscard]] PacketPtr allocate_packet() {
  AVPacket* packet = av_packet_alloc();
  if (packet == nullptr) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "FFmpeg could not allocate decode packet"};
  }

  return PacketPtr{packet};
}

[[nodiscard]] FramePtr allocate_frame() {
  AVFrame* frame = av_frame_alloc();
  if (frame == nullptr) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "FFmpeg could not allocate decode frame"};
  }

  return FramePtr{frame};
}

[[nodiscard]] std::optional<std::string> pixel_format_name(int format) {
  if (format < 0) {
    return std::nullopt;
  }

  const char* name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(format));
  if (name == nullptr) {
    return std::nullopt;
  }

  return std::string{name};
}

[[nodiscard]] std::string color_range_name(AVColorRange range) {
  const char* name = av_color_range_name(range);
  if (name == nullptr) {
    return "unknown";
  }
  return std::string{name};
}

[[nodiscard]] std::string color_primaries_name(AVColorPrimaries primaries) {
  const char* name = av_color_primaries_name(primaries);
  if (name == nullptr) {
    return "unknown";
  }
  return std::string{name};
}

[[nodiscard]] std::string color_transfer_name(AVColorTransferCharacteristic transfer) {
  const char* name = av_color_transfer_name(transfer);
  if (name == nullptr) {
    return "unknown";
  }
  return std::string{name};
}

[[nodiscard]] std::string color_matrix_name(AVColorSpace color_space) {
  const char* name = av_color_space_name(color_space);
  if (name == nullptr) {
    return "unknown";
  }
  return std::string{name};
}

[[nodiscard]] std::optional<domain::Rational> sample_aspect_ratio(const AVFrame& frame) {
  if (frame.sample_aspect_ratio.num <= 0 || frame.sample_aspect_ratio.den <= 0) {
    return std::nullopt;
  }
  return domain::Rational{frame.sample_aspect_ratio.num, frame.sample_aspect_ratio.den};
}

[[nodiscard]] FFmpegVideoImageDescriptor image_descriptor(const AVFrame& frame) {
  return FFmpegVideoImageDescriptor{
      frame.width,
      frame.height,
      frame.width,
      frame.height,
      pixel_format_name(frame.format),
      sample_aspect_ratio(frame),
      color_range_name(frame.color_range),
      color_primaries_name(frame.color_primaries),
      color_transfer_name(frame.color_trc),
      color_matrix_name(frame.colorspace),
  };
}

[[nodiscard]] int checked_rgba_stride(int width) {
  if (width <= 0 || width > std::numeric_limits<int>::max() / 4) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "preview conversion received invalid frame width"};
  }
  return width * 4;
}

[[nodiscard]] std::size_t checked_preview_buffer_size(int stride, int height) {
  if (height <= 0) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "preview conversion received invalid frame height"};
  }
  const auto wide_size = static_cast<std::uint64_t>(stride) * static_cast<std::uint64_t>(height);
  if (wide_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "preview conversion buffer size overflowed"};
  }
  return static_cast<std::size_t>(wide_size);
}

[[nodiscard]] std::int64_t checked_to_int64(WideInt value) {
  if (value < static_cast<WideInt>(std::numeric_limits<std::int64_t>::min()) ||
      value > static_cast<WideInt>(std::numeric_limits<std::int64_t>::max())) {
    throw MediaDecodeError{MediaDecodeErrorCode::InvalidSeekTarget,
                           "seek target is outside FFmpeg timestamp range"};
  }

  return static_cast<std::int64_t>(value);
}

[[nodiscard]] WideInt checked_multiply(WideInt lhs, WideInt rhs) {
  constexpr WideInt wide_max =
      (static_cast<WideInt>(1) << 126) + ((static_cast<WideInt>(1) << 126) - 1);
  constexpr WideInt wide_min = -wide_max - 1;

  if (lhs == 0 || rhs == 0) {
    return 0;
  }

  if (lhs > 0) {
    if (rhs > 0) {
      if (lhs > wide_max / rhs) {
        throw MediaDecodeError{MediaDecodeErrorCode::InvalidSeekTarget,
                               "seek target conversion overflowed"};
      }
    } else if (rhs < wide_min / lhs) {
      throw MediaDecodeError{MediaDecodeErrorCode::InvalidSeekTarget,
                             "seek target conversion overflowed"};
    }
  } else {
    if (rhs > 0) {
      if (lhs < wide_min / rhs) {
        throw MediaDecodeError{MediaDecodeErrorCode::InvalidSeekTarget,
                               "seek target conversion overflowed"};
      }
    } else if (lhs < wide_max / rhs) {
      throw MediaDecodeError{MediaDecodeErrorCode::InvalidSeekTarget,
                             "seek target conversion overflowed"};
    }
  }

  return lhs * rhs;
}

[[nodiscard]] WideInt floor_divide(WideInt numerator, WideInt denominator) {
  if (denominator <= 0) {
    throw MediaDecodeError{MediaDecodeErrorCode::InvalidSeekTarget,
                           "seek target conversion had invalid denominator"};
  }

  WideInt quotient = numerator / denominator;
  const WideInt remainder = numerator % denominator;
  if (remainder != 0 && numerator < 0) {
    --quotient;
  }

  return quotient;
}

[[nodiscard]] std::int64_t floor_media_time_to_stream_timestamp(
    domain::MediaTime target,
    domain::Rational stream_time_base) {
  const domain::Rational target_time_base = target.time_base();
  const WideInt numerator = checked_multiply(
      checked_multiply(static_cast<WideInt>(target.value()), target_time_base.numerator()),
      stream_time_base.denominator());
  const WideInt denominator = checked_multiply(target_time_base.denominator(),
                                               stream_time_base.numerator());
  return checked_to_int64(floor_divide(numerator, denominator));
}

} // namespace

class FFmpegVideoFrameLease::Impl {
public:
  explicit Impl(FramePtr frame) noexcept : frame_{std::move(frame)} {}

  [[nodiscard]] bool valid() const noexcept {
    return frame_ != nullptr;
  }

  [[nodiscard]] const AVFrame& frame() const {
    if (frame_ == nullptr) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                             "preview conversion received an invalid frame lease"};
    }
    return *frame_;
  }

private:
  FramePtr frame_;
};

FFmpegVideoFrameLease::FFmpegVideoFrameLease() = default;
FFmpegVideoFrameLease::~FFmpegVideoFrameLease() = default;
FFmpegVideoFrameLease::FFmpegVideoFrameLease(const FFmpegVideoFrameLease&) noexcept = default;
FFmpegVideoFrameLease& FFmpegVideoFrameLease::operator=(const FFmpegVideoFrameLease&) noexcept =
    default;
FFmpegVideoFrameLease::FFmpegVideoFrameLease(FFmpegVideoFrameLease&&) noexcept = default;
FFmpegVideoFrameLease& FFmpegVideoFrameLease::operator=(FFmpegVideoFrameLease&&) noexcept =
    default;
FFmpegVideoFrameLease::FFmpegVideoFrameLease(std::shared_ptr<const Impl> impl) noexcept
    : impl_{std::move(impl)} {}

bool FFmpegVideoFrameLease::valid() const noexcept {
  return impl_ != nullptr && impl_->valid();
}

FFmpegPreviewFrame FFmpegPreviewConverter::convert(const DecodedVisualVideoFrame& frame) const {
  if (!frame.frame.valid() || frame.frame.impl_ == nullptr) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "preview conversion requires a valid frame lease"};
  }

  const AVFrame& source = frame.frame.impl_->frame();
  if (source.width <= 0 || source.height <= 0 || source.format < 0) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "preview conversion received unsupported frame metadata"};
  }

  const auto start = std::chrono::steady_clock::now();
  const int stride = checked_rgba_stride(source.width);
  std::vector<std::uint8_t> pixels(checked_preview_buffer_size(stride, source.height));

  std::uint8_t* destination_data[4] = {pixels.data(), nullptr, nullptr, nullptr};
  int destination_linesize[4] = {stride, 0, 0, 0};

  SwsContextPtr context{sws_getContext(source.width,
                                       source.height,
                                       static_cast<AVPixelFormat>(source.format),
                                       source.width,
                                       source.height,
                                       AV_PIX_FMT_RGBA,
                                       SWS_BILINEAR,
                                       nullptr,
                                       nullptr,
                                       nullptr)};
  if (context == nullptr) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "libswscale could not create preview conversion context"};
  }

  const int converted_height = sws_scale(context.get(),
                                         source.data,
                                         source.linesize,
                                         0,
                                         source.height,
                                         destination_data,
                                         destination_linesize);
  if (converted_height != source.height) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "libswscale did not convert the full preview frame"};
  }

  return FFmpegPreviewFrame{
      source.width,
      source.height,
      stride,
      FFmpegPreviewPixelFormat::Rgba8888,
      std::move(pixels),
      frame.presentation_timestamp,
      frame.image,
      std::chrono::steady_clock::now() - start,
  };
}

class FFmpegVideoDecodeSession::Impl {
public:
  Impl(const std::filesystem::path& path, int video_stream_index)
      : context_{internal::open_input_with_stream_info(
            internal::canonical_local_regular_file(path))},
        stream_index_{video_stream_index} {
    if (stream_index_ < 0 ||
        static_cast<unsigned int>(stream_index_) >= context_->nb_streams) {
      throw MediaDecodeError{MediaDecodeErrorCode::InvalidStreamIndex,
                             "selected video stream index is out of range"};
    }

    AVStream& stream = *context_->streams[static_cast<unsigned int>(stream_index_)];
    if (stream.codecpar == nullptr) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                             "selected video stream has no codec parameters"};
    }
    if (stream.codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
      throw MediaDecodeError{MediaDecodeErrorCode::SelectedStreamIsNotVideo,
                             "selected stream is not a video stream"};
    }

    const AVCodec* decoder = avcodec_find_decoder(stream.codecpar->codec_id);
    if (decoder == nullptr) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecoderUnavailable,
                             "no FFmpeg decoder is available for selected video stream"};
    }

    codec_name_ = internal::codec_name(*stream.codecpar);
    stream_time_base_ = internal::convert_required_time_base(stream.time_base);
    stream_start_timestamp_ = internal::convert_timestamp(stream.start_time, stream_time_base_);
    codec_context_ = allocate_decoder_context(*decoder, *stream.codecpar);
    packet_ = allocate_packet();
    frame_ = allocate_frame();
  }

  [[nodiscard]] std::optional<DecodedVideoFrameInfo> decode_next() {
    if (pending_visual_frame_.has_value()) {
      auto pending = std::move(pending_visual_frame_);
      pending_visual_frame_.reset();
      return DecodedVideoFrameInfo{
          pending->stream_index,
          pending->image.coded_width,
          pending->image.coded_height,
          pending->image.pixel_format_name,
          pending->presentation_timestamp,
      };
    }
    if (pending_decoded_frame_.has_value()) {
      auto pending = std::move(pending_decoded_frame_);
      pending_decoded_frame_.reset();
      return pending;
    }

    if (state_ == State::Finished) {
      return std::nullopt;
    }

    while (true) {
      if (auto frame = receive_pending_frame()) {
        return frame;
      }

      if (state_ == State::Finished) {
        return std::nullopt;
      }

      if (state_ == State::Draining) {
        continue;
      }

      send_or_read_next_packet();
    }
  }

  [[nodiscard]] std::optional<DecodedVisualVideoFrame> decode_next_visual() {
    if (pending_visual_frame_.has_value()) {
      auto pending = std::move(pending_visual_frame_);
      pending_visual_frame_.reset();
      return pending;
    }
    if (pending_decoded_frame_.has_value()) {
      throw MediaDecodeError{
          MediaDecodeErrorCode::DecodeFailed,
          "FFmpeg visual decode cannot consume a metadata-only pending presentation seek frame"};
    }

    if (state_ == State::Finished) {
      return std::nullopt;
    }

    while (true) {
      if (auto frame = receive_pending_visual_frame()) {
        return frame;
      }

      if (state_ == State::Finished) {
        return std::nullopt;
      }

      if (state_ == State::Draining) {
        continue;
      }

      send_or_read_next_packet();
    }
  }

  [[nodiscard]] int stream_index() const noexcept {
    return stream_index_;
  }

  [[nodiscard]] const std::string& codec_name() const noexcept {
    return codec_name_;
  }

  [[nodiscard]] std::optional<domain::MediaTime> stream_start_timestamp() const {
    return stream_start_timestamp_;
  }

  [[nodiscard]] VideoSeekResult seek_to_stream_timestamp(domain::MediaTime target) {
    const std::int64_t seek_timestamp =
        floor_media_time_to_stream_timestamp(target, stream_time_base_);

    reset_reusable_objects_before_seek();

    const int seek_result =
        av_seek_frame(context_.get(), stream_index_, seek_timestamp, AVSEEK_FLAG_BACKWARD);
    if (seek_result == AVERROR(ENOSYS) || seek_result == AVERROR(EPIPE)) {
      mark_after_failed_seek();
      throw MediaDecodeError{MediaDecodeErrorCode::SeekUnsupported,
                             "FFmpeg demuxer does not support seeking for this stream"};
    }
    if (seek_result < 0) {
      mark_after_failed_seek();
      throw MediaDecodeError{MediaDecodeErrorCode::SeekFailed,
                             "FFmpeg failed while seeking video stream: " +
                                 ffmpeg_error_text(seek_result)};
    }

    avcodec_flush_buffers(codec_context_.get());
    reset_logical_decode_state();

    std::size_t decoded_after_seek = 0;
    while (auto frame = decode_next()) {
      ++decoded_after_seek;
      if (!frame->presentation_timestamp.has_value()) {
        continue;
      }
      if (*frame->presentation_timestamp >= target) {
        return VideoSeekResult{target, std::move(frame), decoded_after_seek};
      }
    }

    return VideoSeekResult{target, std::nullopt, decoded_after_seek};
  }

  [[nodiscard]] VideoPresentationSeekResult seek_to_presentation_timestamp(
      domain::MediaTime target) {
    const std::int64_t seek_timestamp =
        floor_media_time_to_stream_timestamp(target, stream_time_base_);

    reset_reusable_objects_before_seek();

    const int seek_result =
        av_seek_frame(context_.get(), stream_index_, seek_timestamp, AVSEEK_FLAG_BACKWARD);
    if (seek_result == AVERROR(ENOSYS) || seek_result == AVERROR(EPIPE)) {
      mark_after_failed_seek();
      throw MediaDecodeError{MediaDecodeErrorCode::SeekUnsupported,
                             "FFmpeg demuxer does not support seeking for this stream"};
    }
    if (seek_result < 0) {
      mark_after_failed_seek();
      throw MediaDecodeError{MediaDecodeErrorCode::SeekFailed,
                             "FFmpeg failed while seeking video stream: " +
                                 ffmpeg_error_text(seek_result)};
    }

    avcodec_flush_buffers(codec_context_.get());
    reset_logical_decode_state();
    pending_decoded_frame_.reset();

    std::size_t decoded_after_seek = 0;
    std::optional<DecodedVideoFrameInfo> candidate;
    while (auto frame = decode_next()) {
      ++decoded_after_seek;
      if (!frame->presentation_timestamp.has_value()) {
        continue;
      }

      if (*frame->presentation_timestamp <= target) {
        candidate = std::move(frame);
        continue;
      }

      if (candidate.has_value()) {
        pending_decoded_frame_ = std::move(frame);
        return VideoPresentationSeekResult{target, std::move(candidate), pending_decoded_frame_,
                                           decoded_after_seek};
      }

      pending_decoded_frame_ = std::move(frame);
      return VideoPresentationSeekResult{target, std::nullopt, pending_decoded_frame_,
                                         decoded_after_seek};
    }

    return VideoPresentationSeekResult{target, std::move(candidate), std::nullopt,
                                       decoded_after_seek};
  }

  [[nodiscard]] VideoPresentationVisualSeekResult seek_to_presentation_timestamp_visual(
      domain::MediaTime target) {
    const std::int64_t seek_timestamp =
        floor_media_time_to_stream_timestamp(target, stream_time_base_);

    reset_reusable_objects_before_seek();

    const int seek_result =
        av_seek_frame(context_.get(), stream_index_, seek_timestamp, AVSEEK_FLAG_BACKWARD);
    if (seek_result == AVERROR(ENOSYS) || seek_result == AVERROR(EPIPE)) {
      mark_after_failed_seek();
      throw MediaDecodeError{MediaDecodeErrorCode::SeekUnsupported,
                             "FFmpeg demuxer does not support seeking for this stream"};
    }
    if (seek_result < 0) {
      mark_after_failed_seek();
      throw MediaDecodeError{MediaDecodeErrorCode::SeekFailed,
                             "FFmpeg failed while seeking video stream: " +
                                 ffmpeg_error_text(seek_result)};
    }

    avcodec_flush_buffers(codec_context_.get());
    reset_logical_decode_state();
    pending_decoded_frame_.reset();
    pending_visual_frame_.reset();

    std::size_t decoded_after_seek = 0;
    std::optional<DecodedVisualVideoFrame> candidate;
    while (auto frame = decode_next_visual()) {
      ++decoded_after_seek;
      if (!frame->presentation_timestamp.has_value()) {
        continue;
      }

      if (*frame->presentation_timestamp <= target) {
        candidate = std::move(frame);
        continue;
      }

      pending_visual_frame_ = std::move(frame);
      if (candidate.has_value()) {
        return VideoPresentationVisualSeekResult{target, std::move(candidate), pending_visual_frame_,
                                                decoded_after_seek};
      }

      return VideoPresentationVisualSeekResult{target, std::nullopt, pending_visual_frame_,
                                              decoded_after_seek};
    }

    return VideoPresentationVisualSeekResult{target, std::move(candidate), std::nullopt,
                                            decoded_after_seek};
  }

private:
  enum class State {
    Reading,
    Draining,
    Finished,
  };

  [[nodiscard]] std::optional<DecodedVideoFrameInfo> receive_pending_frame() {
    const int receive_result = avcodec_receive_frame(codec_context_.get(), frame_.get());
    if (receive_result == 0) {
      pending_send_was_blocked_ = false;
      DecodedVideoFrameInfo info{
          stream_index_,
          frame_->width,
          frame_->height,
          pixel_format_name(frame_->format),
          internal::convert_timestamp(frame_->best_effort_timestamp, stream_time_base_),
      };
      av_frame_unref(frame_.get());
      return info;
    }

    if (receive_result == AVERROR(EAGAIN)) {
      if (pending_send_was_blocked_) {
        throw MediaDecodeError{
            MediaDecodeErrorCode::DecodeFailed,
            "FFmpeg video decoder could neither receive a frame nor accept the pending packet"};
      }
      return std::nullopt;
    }

    if (receive_result == AVERROR_EOF) {
      state_ = State::Finished;
      return std::nullopt;
    }

    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "FFmpeg video decoder failed while receiving frame: " +
                               ffmpeg_error_text(receive_result)};
  }

  [[nodiscard]] std::optional<DecodedVisualVideoFrame> receive_pending_visual_frame() {
    const int receive_result = avcodec_receive_frame(codec_context_.get(), frame_.get());
    if (receive_result == 0) {
      pending_send_was_blocked_ = false;
      AVFrame* cloned_frame = av_frame_clone(frame_.get());
      if (cloned_frame == nullptr) {
        av_frame_unref(frame_.get());
        throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                               "FFmpeg could not create a refcounted video frame lease"};
      }

      DecodedVisualVideoFrame visual_frame{
          next_visual_frame_id_++,
          stream_index_,
          image_descriptor(*frame_),
          internal::convert_timestamp(frame_->best_effort_timestamp, stream_time_base_),
          FFmpegVideoFrameLease{
              std::make_shared<const FFmpegVideoFrameLease::Impl>(FramePtr{cloned_frame})},
      };
      av_frame_unref(frame_.get());
      return visual_frame;
    }

    if (receive_result == AVERROR(EAGAIN)) {
      if (pending_send_was_blocked_) {
        throw MediaDecodeError{
            MediaDecodeErrorCode::DecodeFailed,
            "FFmpeg video decoder could neither receive a frame nor accept the pending packet"};
      }
      return std::nullopt;
    }

    if (receive_result == AVERROR_EOF) {
      state_ = State::Finished;
      return std::nullopt;
    }

    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "FFmpeg video decoder failed while receiving visual frame: " +
                               ffmpeg_error_text(receive_result)};
  }

  void send_or_read_next_packet() {
    if (has_pending_packet_) {
      (void)send_pending_packet();
      return;
    }

    while (true) {
      const int read_result = av_read_frame(context_.get(), packet_.get());
      if (read_result == AVERROR_EOF) {
        send_drain_signal();
        return;
      }
      if (read_result < 0) {
        throw MediaDecodeError{MediaDecodeErrorCode::DemuxReadFailed,
                               "FFmpeg failed while reading media packet: " +
                                   ffmpeg_error_text(read_result)};
      }

      if (packet_->stream_index != stream_index_) {
        av_packet_unref(packet_.get());
        continue;
      }

      has_pending_packet_ = true;
      (void)send_pending_packet();
      return;
    }
  }

  [[nodiscard]] bool send_pending_packet() {
    const int send_result = avcodec_send_packet(codec_context_.get(), packet_.get());
    if (send_result == AVERROR(EAGAIN)) {
      pending_send_was_blocked_ = true;
      return false;
    }
    if (send_result < 0) {
      av_packet_unref(packet_.get());
      has_pending_packet_ = false;
      pending_send_was_blocked_ = false;
      throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                             "FFmpeg video decoder failed while sending packet: " +
                                 ffmpeg_error_text(send_result)};
    }

    av_packet_unref(packet_.get());
    has_pending_packet_ = false;
    pending_send_was_blocked_ = false;
    return true;
  }

  void send_drain_signal() {
    const int send_result = avcodec_send_packet(codec_context_.get(), nullptr);
    if (send_result < 0 && send_result != AVERROR_EOF) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                             "FFmpeg video decoder failed while entering drain mode: " +
                                 ffmpeg_error_text(send_result)};
    }

    state_ = State::Draining;
  }

  void reset_reusable_objects_before_seek() noexcept {
    if (has_pending_packet_) {
      av_packet_unref(packet_.get());
    }
    pending_decoded_frame_.reset();
    pending_visual_frame_.reset();
    av_frame_unref(frame_.get());
  }

  void reset_logical_decode_state() noexcept {
    state_ = State::Reading;
    has_pending_packet_ = false;
    pending_send_was_blocked_ = false;
  }

  void mark_after_failed_seek() noexcept {
    avcodec_flush_buffers(codec_context_.get());
    has_pending_packet_ = false;
    pending_send_was_blocked_ = false;
    pending_decoded_frame_.reset();
    pending_visual_frame_.reset();
    state_ = State::Finished;
  }

  internal::FormatContextPtr context_;
  CodecContextPtr codec_context_;
  PacketPtr packet_;
  FramePtr frame_;
  int stream_index_;
  domain::Rational stream_time_base_{1, 1};
  std::optional<domain::MediaTime> stream_start_timestamp_;
  std::string codec_name_{"unknown"};
  std::optional<DecodedVideoFrameInfo> pending_decoded_frame_;
  std::optional<DecodedVisualVideoFrame> pending_visual_frame_;
  // Monotonic physical visual-frame identity for this session lifetime.
  // Assigned once per successful avcodec_receive_frame(); never reset by
  // seeks, which flush codec state but do not restart the namespace.
  std::uint64_t next_visual_frame_id_{1};
  State state_{State::Reading};
  bool has_pending_packet_{false};
  bool pending_send_was_blocked_{false};
};

FFmpegVideoDecodeSession::FFmpegVideoDecodeSession(const std::filesystem::path& path,
                                                   int video_stream_index)
    : impl_{std::make_unique<Impl>(path, video_stream_index)} {}

FFmpegVideoDecodeSession::~FFmpegVideoDecodeSession() = default;

FFmpegVideoDecodeSession::FFmpegVideoDecodeSession(FFmpegVideoDecodeSession&&) noexcept = default;

FFmpegVideoDecodeSession& FFmpegVideoDecodeSession::operator=(
    FFmpegVideoDecodeSession&&) noexcept = default;

std::optional<DecodedVideoFrameInfo> FFmpegVideoDecodeSession::decode_next() {
  return impl_->decode_next();
}

std::optional<DecodedVisualVideoFrame> FFmpegVideoDecodeSession::decode_next_visual() {
  return impl_->decode_next_visual();
}

VideoSeekResult FFmpegVideoDecodeSession::seek_to_stream_timestamp(domain::MediaTime target) {
  return impl_->seek_to_stream_timestamp(target);
}

VideoPresentationSeekResult FFmpegVideoDecodeSession::seek_to_presentation_timestamp(
    domain::MediaTime target) {
  return impl_->seek_to_presentation_timestamp(target);
}

VideoPresentationVisualSeekResult FFmpegVideoDecodeSession::seek_to_presentation_timestamp_visual(
    domain::MediaTime target) {
  return impl_->seek_to_presentation_timestamp_visual(target);
}

int FFmpegVideoDecodeSession::stream_index() const noexcept {
  return impl_->stream_index();
}

const std::string& FFmpegVideoDecodeSession::codec_name() const noexcept {
  return impl_->codec_name();
}

std::optional<domain::MediaTime> FFmpegVideoDecodeSession::stream_start_timestamp() const {
  return impl_->stream_start_timestamp();
}

} // namespace pulsar::media::ffmpeg
