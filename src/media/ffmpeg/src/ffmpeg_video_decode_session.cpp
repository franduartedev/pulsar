#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>

#include <pulsar/media/ffmpeg/decoded_visual_video_frame.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_video_converter.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>

#include <pulsar/media/ffmpeg/media_decode_error.hpp>

#include <pulsar/application/media/still_media.hpp>
#include <pulsar/domain/support/wide_integer.hpp>

#include "internal/ffmpeg_common.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/display.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <chrono>
#include <cerrno>
#include <cmath>
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

using WideInt = domain::WideInteger;

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

// ---- Stills ---------------------------------------------------------------------------------
// A photograph or colour card is decoded once into an opaque, upright RGBA picture that is held
// for the whole clip. Transparent pixels are flattened over black (the project canvas is black),
// and the EXIF orientation that phones write is applied so portrait photos are not sideways.
constexpr std::int64_t max_still_pixels = 100'000'000;

[[nodiscard]] FramePtr new_rgba_frame(int width, int height) {
  FramePtr frame = allocate_frame();
  frame->format = AV_PIX_FMT_RGBA;
  frame->width = width;
  frame->height = height;
  if (av_frame_get_buffer(frame.get(), 32) < 0) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "FFmpeg could not allocate a still picture buffer"};
  }
  return frame;
}

[[nodiscard]] int clockwise_quarter_turns(const AVFrame& source) {
  const AVFrameSideData* side = av_frame_get_side_data(&source, AV_FRAME_DATA_DISPLAYMATRIX);
  if (side == nullptr || side->size < 9 * sizeof(std::int32_t)) return 0;
  const double counter_clockwise =
      av_display_rotation_get(reinterpret_cast<const std::int32_t*>(side->data));
  if (std::isnan(counter_clockwise)) return 0;
  const long turns = std::lround(-counter_clockwise / 90.0);
  return static_cast<int>(((turns % 4) + 4) % 4);
}

[[nodiscard]] FramePtr make_still_frame(const AVFrame& source) {
  const int width = source.width;
  const int height = source.height;
  if (width <= 0 || height <= 0 || source.format < 0 ||
      static_cast<std::int64_t>(width) * height > max_still_pixels) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "the image is empty or too large to use as a still"};
  }
  // JPEG decodes to the deprecated "yuvj" formats; use the plain format plus a full-range flag.
  auto format = static_cast<AVPixelFormat>(source.format);
  bool full_range = source.color_range == AVCOL_RANGE_JPEG;
  bool jpeg_range_yuv = false;
  switch (format) {
    case AV_PIX_FMT_YUVJ420P: format = AV_PIX_FMT_YUV420P; full_range = true; jpeg_range_yuv = true; break;
    case AV_PIX_FMT_YUVJ422P: format = AV_PIX_FMT_YUV422P; full_range = true; jpeg_range_yuv = true; break;
    case AV_PIX_FMT_YUVJ444P: format = AV_PIX_FMT_YUV444P; full_range = true; jpeg_range_yuv = true; break;
    case AV_PIX_FMT_YUVJ440P: format = AV_PIX_FMT_YUV440P; full_range = true; jpeg_range_yuv = true; break;
    default: break;
  }
  SwsContextPtr sws{sws_getContext(width, height, format, width, height, AV_PIX_FMT_RGBA,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr)};
  if (sws == nullptr) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "libswscale could not convert the still image"};
  }
  if (jpeg_range_yuv) {
    const int* coefficients = sws_getCoefficients(SWS_CS_DEFAULT);
    sws_setColorspaceDetails(sws.get(), coefficients, full_range ? 1 : 0, coefficients, 1, 0,
                             1 << 16, 1 << 16);
  }
  FramePtr flat = new_rgba_frame(width, height);
  if (sws_scale(sws.get(), source.data, source.linesize, 0, height, flat->data, flat->linesize) !=
      height) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "libswscale did not convert the complete still image"};
  }
  for (int y = 0; y < height; ++y) {
    std::uint8_t* row = flat->data[0] + static_cast<std::ptrdiff_t>(y) * flat->linesize[0];
    for (int x = 0; x < width; ++x) {
      std::uint8_t* px = row + x * 4;
      const unsigned alpha = px[3];
      if (alpha != 255) {
        px[0] = static_cast<std::uint8_t>((px[0] * alpha + 127) / 255);
        px[1] = static_cast<std::uint8_t>((px[1] * alpha + 127) / 255);
        px[2] = static_cast<std::uint8_t>((px[2] * alpha + 127) / 255);
        px[3] = 255;
      }
    }
  }
  const int turns = clockwise_quarter_turns(source);
  if (turns == 0) return flat;

  const int out_width = (turns % 2 == 0) ? width : height;
  const int out_height = (turns % 2 == 0) ? height : width;
  FramePtr upright = new_rgba_frame(out_width, out_height);
  for (int y = 0; y < height; ++y) {
    const std::uint8_t* from = flat->data[0] + static_cast<std::ptrdiff_t>(y) * flat->linesize[0];
    for (int x = 0; x < width; ++x) {
      int dx = x;
      int dy = y;
      if (turns == 1) { dx = height - 1 - y; dy = x; }
      else if (turns == 2) { dx = width - 1 - x; dy = height - 1 - y; }
      else { dx = y; dy = width - 1 - x; }
      std::copy_n(from + x * 4, 4,
                  upright->data[0] + static_cast<std::ptrdiff_t>(dy) * upright->linesize[0] + dx * 4);
    }
  }
  return upright;
}

[[nodiscard]] FramePtr make_color_card_frame(std::uint32_t rgb) {
  const int width = application::still_canvas_max_width;
  const int height = application::still_canvas_max_height;
  FramePtr frame = new_rgba_frame(width, height);
  const std::uint8_t pixel[4] = {static_cast<std::uint8_t>((rgb >> 16) & 0xFF),
                                 static_cast<std::uint8_t>((rgb >> 8) & 0xFF),
                                 static_cast<std::uint8_t>(rgb & 0xFF), 255};
  for (int y = 0; y < height; ++y) {
    std::uint8_t* row = frame->data[0] + static_cast<std::ptrdiff_t>(y) * frame->linesize[0];
    for (int x = 0; x < width; ++x) std::copy_n(pixel, 4, row + x * 4);
  }
  return frame;
}

[[nodiscard]] internal::FormatContextPtr open_unless_generated(const std::filesystem::path& path) {
  if (application::is_color_card_locator(path.generic_string())) return internal::FormatContextPtr{};
  return internal::open_input_with_stream_info(internal::canonical_local_regular_file(path));
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

class FFmpegOfflineVideoFrameConverter::Impl {
public:
  mutable SwsContextPtr sws;
};

FFmpegOfflineVideoFrameConverter::FFmpegOfflineVideoFrameConverter()
    : impl_{std::make_unique<Impl>()} {}

FFmpegOfflineVideoFrameConverter::~FFmpegOfflineVideoFrameConverter() = default;

application::exporting::OfflineVideoSourceFrame
FFmpegOfflineVideoFrameConverter::convert(const DecodedVisualVideoFrame& frame) const {
  if (!frame.frame.valid() || frame.frame.impl_ == nullptr) {
    throw application::exporting::OfflineVideoRenderError{
        application::exporting::OfflineVideoRenderErrorCode::UnsupportedFrame,
        "FFmpeg returned an invalid visual frame lease"};
  }

  const AVFrame& source = frame.frame.impl_->frame();
  if (source.width <= 0 || source.height <= 0 || source.format < 0 ||
      source.width > std::numeric_limits<int>::max() / 4) {
    throw application::exporting::OfflineVideoRenderError{
        application::exporting::OfflineVideoRenderErrorCode::UnsupportedFrame,
        "FFmpeg returned unsupported video frame metadata"};
  }

  impl_->sws.reset(sws_getCachedContext(
      impl_->sws.release(), source.width, source.height,
      static_cast<AVPixelFormat>(source.format), source.width, source.height,
      AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr));
  if (!impl_->sws) {
    throw application::exporting::OfflineVideoRenderError{
        application::exporting::OfflineVideoRenderErrorCode::ConversionFailed,
        "libswscale could not create the cached RGBA conversion context"};
  }

  const int stride = source.width * 4;
  const auto size = static_cast<std::size_t>(stride) * static_cast<std::size_t>(source.height);
  auto pixels = std::make_shared<application::exporting::OfflineVideoPixelBuffer>();
  pixels->width = source.width;
  pixels->height = source.height;
  pixels->stride_bytes = stride;
  pixels->pixels.resize(size);
  std::uint8_t* destination_data[4] = {pixels->pixels.data(), nullptr, nullptr, nullptr};
  int destination_linesize[4] = {stride, 0, 0, 0};
  const int converted_height = sws_scale(impl_->sws.get(), source.data, source.linesize, 0,
                                         source.height, destination_data, destination_linesize);
  if (converted_height != source.height) {
    throw application::exporting::OfflineVideoRenderError{
        application::exporting::OfflineVideoRenderErrorCode::ConversionFailed,
        "libswscale did not convert the complete video frame"};
  }

  return application::exporting::OfflineVideoSourceFrame{
      frame.presentation_timestamp, frame.presentation_duration, std::move(pixels)};
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
      : context_{open_unless_generated(path)}, stream_index_{video_stream_index} {
    if (context_ == nullptr) {
      // Generated colour card: no file, no demuxer, no decoder.
      const auto rgb = application::color_card_rgb(path.generic_string());
      if (!rgb.has_value()) {
        throw MediaDecodeError{MediaDecodeErrorCode::InvalidStreamIndex,
                               "the colour card locator is malformed"};
      }
      codec_name_ = "pulsar-color";
      stream_index_ = 0;
      stream_time_base_ = domain::Rational{1, 1000};
      stream_start_timestamp_ = domain::MediaTime{0, stream_time_base_};
      still_ = true;
      FramePtr card = make_color_card_frame(*rgb);
      still_frame_ = DecodedVisualVideoFrame{
          1, stream_index_, image_descriptor(*card), *stream_start_timestamp_,
          FFmpegVideoFrameLease{std::make_shared<const FFmpegVideoFrameLease::Impl>(std::move(card))}};
      return;
    }
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
    still_ = context_->iformat != nullptr && context_->iformat->name != nullptr &&
             application::is_still_image_container(context_->iformat->name);
  }

  [[nodiscard]] std::optional<DecodedVideoFrameInfo> decode_next() {
    if (still_) {
      auto frame = still_next();
      if (!frame.has_value()) return std::nullopt;
      return DecodedVideoFrameInfo{frame->stream_index, frame->image.coded_width,
                                   frame->image.coded_height, frame->image.pixel_format_name,
                                   frame->presentation_timestamp};
    }
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
    if (still_) return still_next();
    return decode_next_visual_raw();
  }

  [[nodiscard]] std::optional<DecodedVisualVideoFrame> decode_next_visual_raw() {
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

  [[nodiscard]] bool is_still() const noexcept { return still_; }

  [[nodiscard]] VideoSeekResult seek_to_stream_timestamp(domain::MediaTime target) {
    throw_if_still_metadata_seek();
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
    throw_if_still_metadata_seek();
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
    if (still_) return still_seek(target);
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

  void throw_if_still_metadata_seek() const {
    if (still_) {
      throw MediaDecodeError{MediaDecodeErrorCode::SeekUnsupported,
                             "still pictures only support presentation seeks"};
    }
  }

  // Decodes the picture once; every later request is answered from the held frame.
  void ensure_still_loaded() {
    if (still_frame_.has_value()) return;
    auto raw = decode_next_visual_raw();
    if (!raw.has_value() || !raw->frame.valid()) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                             "the image could not be decoded"};
    }
    FramePtr upright = make_still_frame(raw->frame.impl_->frame());
    const auto origin = stream_start_timestamp_.value_or(domain::MediaTime{0, stream_time_base_});
    still_frame_ = DecodedVisualVideoFrame{
        1, stream_index_, image_descriptor(*upright), origin,
        FFmpegVideoFrameLease{std::make_shared<const FFmpegVideoFrameLease::Impl>(std::move(upright))}};
  }

  // Sequential read: the picture once, then end of stream.
  [[nodiscard]] std::optional<DecodedVisualVideoFrame> still_next() {
    ensure_still_loaded();
    if (still_delivered_) return std::nullopt;
    still_delivered_ = true;
    return fresh_still();
  }

  // Same picture, new identity: the consumer's visual mailbox hands each frame out once, so
  // every delivery (a seek back into the held picture, for instance) must be a new "frame".
  [[nodiscard]] DecodedVisualVideoFrame fresh_still() {
    DecodedVisualVideoFrame frame = *still_frame_;
    frame.physical_frame_id = next_visual_frame_id_++;
    frame.presentation_duration = application::still_playable_duration();
    return frame;
  }

  // The picture is the frame at every instant from its start onwards.
  [[nodiscard]] VideoPresentationVisualSeekResult still_seek(domain::MediaTime target) {
    ensure_still_loaded();
    if (target < *still_frame_->presentation_timestamp) {
      still_delivered_ = false;
      return VideoPresentationVisualSeekResult{target, std::nullopt, fresh_still(), 1};
    }
    still_delivered_ = true;
    return VideoPresentationVisualSeekResult{target, fresh_still(), std::nullopt, 1};
  }

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
          frame_->duration > 0 ? std::optional{domain::MediaTime{frame_->duration, stream_time_base_}}
                               : std::nullopt,
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
  bool still_{false};
  bool still_delivered_{false};
  std::optional<DecodedVisualVideoFrame> still_frame_;
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

bool FFmpegVideoDecodeSession::is_still() const noexcept { return impl_->is_still(); }

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
