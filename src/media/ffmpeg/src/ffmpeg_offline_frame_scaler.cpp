#include <pulsar/media/ffmpeg/ffmpeg_offline_frame_scaler.hpp>

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
}

#include <stdexcept>

namespace pulsar::media::ffmpeg {

class FFmpegOfflineFrameScaler::Impl {
public:
  ~Impl() {
    if (context != nullptr) sws_freeContext(context);
  }
  SwsContext* context{nullptr};
};

FFmpegOfflineFrameScaler::FFmpegOfflineFrameScaler() : impl_{std::make_unique<Impl>()} {}
FFmpegOfflineFrameScaler::~FFmpegOfflineFrameScaler() = default;

application::exporting::OfflineVideoPixelBuffer FFmpegOfflineFrameScaler::scale(
    const application::exporting::OfflineVideoPixelBuffer& source, int target_width, int target_height) {
  if (source.width <= 0 || source.height <= 0 || target_width <= 0 || target_height <= 0 ||
      source.pixels.size() < static_cast<std::size_t>(source.stride_bytes) * static_cast<std::size_t>(source.height)) {
    throw std::invalid_argument{"invalid picture for scaling"};
  }
  impl_->context = sws_getCachedContext(impl_->context, source.width, source.height, AV_PIX_FMT_RGBA,
                                        target_width, target_height, AV_PIX_FMT_RGBA,
                                        SWS_LANCZOS | SWS_ACCURATE_RND, nullptr, nullptr, nullptr);
  if (impl_->context == nullptr) throw std::runtime_error{"cannot create the scaling context"};
  application::exporting::OfflineVideoPixelBuffer result;
  result.width = target_width;
  result.height = target_height;
  result.stride_bytes = target_width * 4;
  result.pixels.assign(static_cast<std::size_t>(result.stride_bytes) * static_cast<std::size_t>(target_height), 0);
  const std::uint8_t* input[4] = {source.pixels.data(), nullptr, nullptr, nullptr};
  const int input_stride[4] = {source.stride_bytes, 0, 0, 0};
  std::uint8_t* output[4] = {result.pixels.data(), nullptr, nullptr, nullptr};
  const int output_stride[4] = {result.stride_bytes, 0, 0, 0};
  const int rows = sws_scale(impl_->context, input, input_stride, 0, source.height, output, output_stride);
  if (rows != target_height) throw std::runtime_error{"scaler produced an incomplete picture"};
  return result;
}

} // namespace pulsar::media::ffmpeg
