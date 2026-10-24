#include <pulsar/media/ffmpeg/ffmpeg_export_media_writer.hpp>
#include <pulsar/media/ffmpeg/detail/ffmpeg_export_audio_policy.hpp>
#include <pulsar/media/ffmpeg/detail/ffmpeg_export_media_writer_test_hook.hpp>

#include "internal/ffmpeg_common.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace pulsar::media::ffmpeg {
namespace detail {
ExportWriteHeaderHook& export_write_header_hook() noexcept {
  static ExportWriteHeaderHook hook = nullptr;
  return hook;
}
} // namespace detail

namespace {

using application::exporting::ExportMediaWriterError;
using application::exporting::ExportMediaWriterErrorCode;
using application::exporting::ExportMediaWriterMetrics;
using application::exporting::ExportPlan;
using application::exporting::OfflineAudioBlock;
using application::exporting::OfflinePixelFormat;
using application::exporting::OfflineVideoFrame;
using internal::ffmpeg_error_text;

constexpr std::size_t kMaxAudioFifoFrames = 4096;

[[noreturn]] void fail(ExportMediaWriterErrorCode code, std::string message) {
  throw ExportMediaWriterError{code, std::move(message)};
}

[[nodiscard]] int checked_int(std::int64_t value, ExportMediaWriterErrorCode code,
                              const char* what) {
  if (value <= 0 || value > std::numeric_limits<int>::max()) fail(code, what);
  return static_cast<int>(value);
}

struct FormatDeleter {
  void operator()(AVFormatContext* context) const noexcept {
    if (context == nullptr) return;
    // avformat_free_context() does not close AVIOContext. This deleter also
    // runs when Impl construction throws after avio_open().
    if (context->pb != nullptr && context->oformat != nullptr &&
        (context->oformat->flags & AVFMT_NOFILE) == 0) {
      avio_closep(&context->pb);
    }
    avformat_free_context(context);
  }
};

struct CodecDeleter {
  void operator()(AVCodecContext* context) const noexcept {
    if (context != nullptr) avcodec_free_context(&context);
  }
};

struct FrameDeleter {
  void operator()(AVFrame* frame) const noexcept {
    if (frame != nullptr) av_frame_free(&frame);
  }
};

struct PacketDeleter {
  void operator()(AVPacket* packet) const noexcept {
    if (packet != nullptr) av_packet_free(&packet);
  }
};

struct SwsDeleter {
  void operator()(SwsContext* context) const noexcept {
    if (context != nullptr) sws_freeContext(context);
  }
};

using FormatPtr = std::unique_ptr<AVFormatContext, FormatDeleter>;
using CodecPtr = std::unique_ptr<AVCodecContext, CodecDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using SwsPtr = std::unique_ptr<SwsContext, SwsDeleter>;

// FFmpeg 7.1 added avcodec_get_supported_config(); FFmpeg 8 removed the public
// AVCodec::sample_fmts and AVCodec::pix_fmts arrays. Keep both worlds working.
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
[[nodiscard]] bool supports_sample_format(const AVCodec& codec, AVSampleFormat format) {
  const void* configs = nullptr;
  int count = 0;
  if (avcodec_get_supported_config(nullptr, &codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &configs,
                                   &count) < 0 ||
      configs == nullptr) {
    return false;
  }
  const auto* formats = static_cast<const AVSampleFormat*>(configs);
  for (int index = 0; index < count; ++index) {
    if (formats[index] == format) return true;
  }
  return false;
}

[[nodiscard]] bool advertises_pixel_format(const AVCodec& codec, AVPixelFormat format) {
  const void* configs = nullptr;
  int count = 0;
  if (avcodec_get_supported_config(nullptr, &codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs,
                                   &count) < 0 ||
      configs == nullptr) {
    return true;
  }
  const auto* formats = static_cast<const AVPixelFormat*>(configs);
  for (int index = 0; index < count; ++index) {
    if (formats[index] == format) return true;
  }
  return false;
}
#else
[[nodiscard]] bool supports_sample_format(const AVCodec& codec, AVSampleFormat format) {
  if (codec.sample_fmts == nullptr) return false;
  for (const auto* current = codec.sample_fmts; *current != AV_SAMPLE_FMT_NONE; ++current) {
    if (*current == format) return true;
  }
  return false;
}

[[nodiscard]] bool advertises_pixel_format(const AVCodec& codec, AVPixelFormat format) {
  if (codec.pix_fmts == nullptr) return true;
  for (const auto* current = codec.pix_fmts; *current != AV_PIX_FMT_NONE; ++current) {
    if (*current == format) return true;
  }
  return false;
}
#endif

} // namespace

class FFmpegExportMediaWriter::Impl {
public:
  Impl(const ExportPlan& plan, const std::filesystem::path& output_path)
      : plan{plan}, output_path{output_path} {
    open_output();
  }

  ~Impl() noexcept { close_output(); }

  void write_video(const OfflineVideoFrame& frame);
  void write_audio(const OfflineAudioBlock& block);
  void finish();
  void abort() noexcept;

  [[nodiscard]] const ExportMediaWriterMetrics& get_metrics() const noexcept { return metrics; }

private:
  void open_output();
  void close_output() noexcept;
  void setup_video();
  void setup_audio();
  void write_packet(AVPacket* packet, AVStream* stream, bool video);
  void receive_video_packets();
  void receive_audio_packets();
  void submit_video_frame(AVFrame* frame);
  void submit_audio_frame(AVFrame* frame);
  void submit_video_flush();
  void submit_audio_flush();
  void send_audio_frame(int frame_samples, const float* interleaved);
  void flush_video();
  void flush_audio();
  void set_audio_frame_samples(const float* interleaved, int frame_samples);

  const ExportPlan& plan;
  std::filesystem::path output_path;
  FormatPtr format;
  CodecPtr video_codec;
  CodecPtr audio_codec;
  AVStream* video_stream{nullptr};
  AVStream* audio_stream{nullptr};
  SwsPtr sws;
  FramePtr video_frame;
  FramePtr audio_frame;
  PacketPtr packet{av_packet_alloc()};
  bool header_written{false};
  bool finished{false};
  std::int64_t next_video_frame{0};
  std::int64_t next_audio_sample{0};
  std::int64_t encoded_audio_sample{0};
  int audio_frame_size{0};
  AVSampleFormat audio_sample_format{AV_SAMPLE_FMT_NONE};
  std::vector<float> audio_fifo;
  ExportMediaWriterMetrics metrics;
};

void FFmpegExportMediaWriter::Impl::open_output() {
  if (!packet) fail(ExportMediaWriterErrorCode::InternalInvariant,
                    "FFmpeg could not allocate an output packet");
  AVFormatContext* raw_format = nullptr;
#if defined(_WIN32)
  const auto output_path_utf8 = output_path.generic_u8string();
  std::string output_path_text{reinterpret_cast<const char*>(output_path_utf8.data()),
                               output_path_utf8.size()};
  if (output_path_text.starts_with("//?/")) output_path_text.erase(0, 4);
  if (output_path_text.size() >= 2 && output_path_text[1] == ':') {
    output_path_text = "file:" + output_path_text;
  } else if (output_path_text.starts_with("//")) {
    output_path_text = "file:" + output_path_text;
  }
#else
  const std::string output_path_text = output_path.generic_string();
#endif
  const auto result = avformat_alloc_output_context2(
      &raw_format, nullptr, "mp4", output_path_text.c_str());
  if (result < 0 || raw_format == nullptr) {
    fail(ExportMediaWriterErrorCode::MuxerUnavailable,
         "MP4 muxer is unavailable: " + ffmpeg_error_text(result));
  }
  format.reset(raw_format);

  setup_video();
  setup_audio();
  video_stream->duration = av_rescale_q(plan.frame_count(), video_codec->time_base,
                                        video_stream->time_base);
  audio_stream->duration = plan.audio_sample_count();
  if ((format->oformat->flags & AVFMT_NOFILE) == 0 &&
      avio_open(&format->pb, output_path_text.c_str(), AVIO_FLAG_WRITE) < 0) {
    fail(ExportMediaWriterErrorCode::OutputOpenFailed,
         "could not open MP4 output path for writing");
  }
  const auto header_result = detail::export_write_header_hook() != nullptr
                                 ? detail::export_write_header_hook()(format.get())
                                 : avformat_write_header(format.get(), nullptr);
  if (header_result < 0) {
    fail(ExportMediaWriterErrorCode::MuxWriteFailed,
         "MP4 header write failed: " + ffmpeg_error_text(header_result));
  }
  header_written = true;
}

void FFmpegExportMediaWriter::Impl::setup_video() {
  const auto* encoder = avcodec_find_encoder_by_name("libx264");
  if (encoder == nullptr) {
    fail(ExportMediaWriterErrorCode::EncoderUnavailable,
         "required H.264 encoder libx264 is unavailable");
  }
  const auto width = checked_int(plan.output_width(), ExportMediaWriterErrorCode::InvalidInputFrame,
                                 "export width is invalid");
  const auto height = checked_int(plan.output_height(), ExportMediaWriterErrorCode::InvalidInputFrame,
                                  "export height is invalid");
  if ((width & 1) != 0 || (height & 1) != 0) {
    fail(ExportMediaWriterErrorCode::InvalidInputFrame,
         "H.264 yuv420p output dimensions must be even");
  }
  const auto rate = plan.frame_rate().rational();
  const int numerator = checked_int(rate.numerator(), ExportMediaWriterErrorCode::InternalInvariant,
                                    "video frame rate numerator is invalid");
  const int denominator = checked_int(rate.denominator(), ExportMediaWriterErrorCode::InternalInvariant,
                                      "video frame rate denominator is invalid");
  if (!advertises_pixel_format(*encoder, AV_PIX_FMT_YUV420P)) {
    fail(ExportMediaWriterErrorCode::EncoderUnavailable, "libx264 does not advertise yuv420p");
  }
  video_stream = avformat_new_stream(format.get(), nullptr);
  if (video_stream == nullptr) fail(ExportMediaWriterErrorCode::InternalInvariant,
                                    "could not create MP4 video stream");
  video_codec.reset(avcodec_alloc_context3(encoder));
  if (!video_codec) fail(ExportMediaWriterErrorCode::EncoderUnavailable,
                         "could not allocate libx264 context");
  video_codec->codec_type = AVMEDIA_TYPE_VIDEO;
  video_codec->codec_id = AV_CODEC_ID_H264;
  video_codec->width = width;
  video_codec->height = height;
  video_codec->pix_fmt = AV_PIX_FMT_YUV420P;
  video_codec->time_base = AVRational{denominator, numerator};
  video_codec->framerate = AVRational{numerator, denominator};
  video_codec->gop_size = std::max(1, (numerator * 2) / denominator);
  // Beta deliberately disables B-frames. It keeps packet DTS/PTS ordering
  // transparent at the mux boundary and makes CFR duration accounting
  // directly auditable; the application contract does not depend on them.
  video_codec->max_b_frames = 0;
  if ((format->oformat->flags & AVFMT_GLOBALHEADER) != 0) video_codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  av_opt_set(video_codec.get(), "preset", "medium", 0);
  av_opt_set(video_codec.get(), "crf", "23", 0);
  const auto result = avcodec_open2(video_codec.get(), encoder, nullptr);
  if (result < 0) fail(ExportMediaWriterErrorCode::EncoderUnavailable,
                       "could not open libx264: " + ffmpeg_error_text(result));
  const auto parameters_result = avcodec_parameters_from_context(video_stream->codecpar,
                                                                  video_codec.get());
  if (parameters_result < 0) fail(ExportMediaWriterErrorCode::InternalInvariant,
                                  "could not copy H.264 parameters");
  video_stream->time_base = video_codec->time_base;
  video_frame.reset(av_frame_alloc());
  if (!video_frame) fail(ExportMediaWriterErrorCode::InternalInvariant,
                         "could not allocate reusable YUV frame");
  video_frame->format = video_codec->pix_fmt;
  video_frame->width = width;
  video_frame->height = height;
  if (av_frame_get_buffer(video_frame.get(), 32) < 0) {
    fail(ExportMediaWriterErrorCode::InternalInvariant, "could not allocate YUV frame buffer");
  }
  sws.reset(sws_getContext(width, height, AV_PIX_FMT_RGBA, width, height,
                           AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr));
  if (!sws) fail(ExportMediaWriterErrorCode::VideoConversionFailed,
                 "could not create reusable RGBA to YUV420P converter");
}

void FFmpegExportMediaWriter::Impl::setup_audio() {
  const auto* encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
  if (encoder == nullptr) {
    fail(ExportMediaWriterErrorCode::EncoderUnavailable, "AAC encoder is unavailable");
  }
  if (!supports_sample_format(*encoder, AV_SAMPLE_FMT_FLTP) &&
      !supports_sample_format(*encoder, AV_SAMPLE_FMT_FLT)) {
    fail(ExportMediaWriterErrorCode::EncoderUnavailable,
         "AAC encoder has no supported float input sample format");
  }
  audio_stream = avformat_new_stream(format.get(), nullptr);
  if (audio_stream == nullptr) fail(ExportMediaWriterErrorCode::InternalInvariant,
                                    "could not create MP4 audio stream");
  audio_codec.reset(avcodec_alloc_context3(encoder));
  if (!audio_codec) fail(ExportMediaWriterErrorCode::EncoderUnavailable,
                         "could not allocate AAC context");
  audio_sample_format = supports_sample_format(*encoder, AV_SAMPLE_FMT_FLTP)
                            ? AV_SAMPLE_FMT_FLTP
                            : AV_SAMPLE_FMT_FLT;
  audio_codec->codec_type = AVMEDIA_TYPE_AUDIO;
  audio_codec->codec_id = AV_CODEC_ID_AAC;
  audio_codec->sample_fmt = audio_sample_format;
  audio_codec->sample_rate = plan.audio_sample_rate();
  audio_codec->bit_rate = plan.profile().audio_bitrate();
  audio_codec->profile = AV_PROFILE_AAC_LOW;
  av_channel_layout_default(&audio_codec->ch_layout, plan.audio_channels());
  audio_codec->time_base = AVRational{1, plan.audio_sample_rate()};
  if ((format->oformat->flags & AVFMT_GLOBALHEADER) != 0) audio_codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  const auto result = avcodec_open2(audio_codec.get(), encoder, nullptr);
  if (result < 0) fail(ExportMediaWriterErrorCode::EncoderUnavailable,
                       "could not open AAC encoder: " + ffmpeg_error_text(result));
  if (audio_codec->sample_rate != 48'000 || audio_codec->ch_layout.nb_channels != 2) {
    fail(ExportMediaWriterErrorCode::EncoderUnavailable,
         "AAC encoder did not accept the required 48 kHz stereo profile");
  }
  if (audio_codec->profile != AV_PROFILE_AAC_LOW) {
    fail(ExportMediaWriterErrorCode::EncoderUnavailable,
         "AAC encoder did not accept the required AAC-LC profile");
  }
  const auto parameters_result = avcodec_parameters_from_context(audio_stream->codecpar,
                                                                  audio_codec.get());
  if (parameters_result < 0) fail(ExportMediaWriterErrorCode::InternalInvariant,
                                  "could not copy AAC parameters");
  audio_stream->time_base = audio_codec->time_base;
  if (audio_stream->codecpar->profile != AV_PROFILE_AAC_LOW) {
    fail(ExportMediaWriterErrorCode::EncoderUnavailable,
         "AAC stream parameters are not AAC-LC");
  }
  audio_codec->initial_padding = 0;
  audio_codec->trailing_padding = 0;
  audio_stream->codecpar->initial_padding = 0;
  audio_stream->codecpar->trailing_padding = 0;
  audio_frame_size = audio_codec->frame_size > 0 ? audio_codec->frame_size : 1024;
  if (audio_frame_size > static_cast<int>(kMaxAudioFifoFrames)) {
    fail(ExportMediaWriterErrorCode::InternalInvariant,
         "AAC frame size exceeds the bounded E4 audio FIFO");
  }
  audio_frame.reset(av_frame_alloc());
  if (!audio_frame) fail(ExportMediaWriterErrorCode::InternalInvariant,
                         "could not allocate reusable AAC frame");
  audio_frame->format = audio_sample_format;
  audio_frame->sample_rate = audio_codec->sample_rate;
  audio_frame->ch_layout = audio_codec->ch_layout;
  audio_frame->nb_samples = audio_frame_size;
  if (av_frame_get_buffer(audio_frame.get(), 0) < 0) {
    fail(ExportMediaWriterErrorCode::InternalInvariant, "could not allocate AAC frame buffer");
  }
}

void FFmpegExportMediaWriter::Impl::write_packet(AVPacket* current, AVStream* stream, bool video) {
  av_packet_rescale_ts(current, video ? video_codec->time_base : audio_codec->time_base,
                       stream->time_base);
  if (video) {
    // Some muxers otherwise derive the track duration from the last DTS and
    // omit the final CFR interval when the stream has no following packet.
    current->duration = av_rescale_q(1, video_codec->time_base, stream->time_base);
  }
  current->stream_index = stream->index;
  const auto result = av_interleaved_write_frame(format.get(), current);
  if (result < 0) fail(ExportMediaWriterErrorCode::MuxWriteFailed,
                       "MP4 packet write failed: " + ffmpeg_error_text(result));
  ++metrics.total_mux_packets;
  if (video) ++metrics.video_packets_written;
  else ++metrics.audio_packets_written;
}

void FFmpegExportMediaWriter::Impl::receive_video_packets() {
  while (true) {
    const auto result = avcodec_receive_packet(video_codec.get(), packet.get());
    if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) return;
    if (result < 0) fail(ExportMediaWriterErrorCode::VideoEncodeFailed,
                         "H.264 packet receive failed: " + ffmpeg_error_text(result));
    write_packet(packet.get(), video_stream, true);
    av_packet_unref(packet.get());
  }
}

void FFmpegExportMediaWriter::Impl::receive_audio_packets() {
  while (true) {
    const auto result = avcodec_receive_packet(audio_codec.get(), packet.get());
    if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) return;
    if (result < 0) fail(ExportMediaWriterErrorCode::AudioEncodeFailed,
                         "AAC packet receive failed: " + ffmpeg_error_text(result));
    write_packet(packet.get(), audio_stream, false);
    av_packet_unref(packet.get());
  }
}

void FFmpegExportMediaWriter::Impl::submit_video_frame(AVFrame* frame) {
  while (true) {
    const auto result = avcodec_send_frame(video_codec.get(), frame);
    if (result == AVERROR(EAGAIN)) {
      // The normal path drains immediately. Keep the retry explicit for
      // encoders whose internal queue can still be full at send time.
      receive_video_packets();
      continue;
    }
    if (result < 0) fail(ExportMediaWriterErrorCode::VideoEncodeFailed,
                         "H.264 frame submission failed: " + ffmpeg_error_text(result));
    receive_video_packets();
    return;
  }
}

void FFmpegExportMediaWriter::Impl::submit_audio_frame(AVFrame* frame) {
  while (true) {
    const auto result = avcodec_send_frame(audio_codec.get(), frame);
    if (result == AVERROR(EAGAIN)) {
      receive_audio_packets();
      continue;
    }
    if (result < 0) fail(ExportMediaWriterErrorCode::AudioEncodeFailed,
                         "AAC frame submission failed: " + ffmpeg_error_text(result));
    receive_audio_packets();
    return;
  }
}

void FFmpegExportMediaWriter::Impl::submit_video_flush() {
  while (true) {
    const auto result = avcodec_send_frame(video_codec.get(), nullptr);
    if (result == AVERROR(EAGAIN)) {
      receive_video_packets();
      continue;
    }
    if (result < 0 && result != AVERROR_EOF) {
      fail(ExportMediaWriterErrorCode::VideoEncodeFailed,
           "H.264 flush submission failed: " + ffmpeg_error_text(result));
    }
    receive_video_packets();
    return;
  }
}

void FFmpegExportMediaWriter::Impl::submit_audio_flush() {
  while (true) {
    const auto result = avcodec_send_frame(audio_codec.get(), nullptr);
    if (result == AVERROR(EAGAIN)) {
      receive_audio_packets();
      continue;
    }
    if (result < 0 && result != AVERROR_EOF) {
      fail(ExportMediaWriterErrorCode::AudioEncodeFailed,
           "AAC flush submission failed: " + ffmpeg_error_text(result));
    }
    receive_audio_packets();
    return;
  }
}

void FFmpegExportMediaWriter::Impl::write_video(const OfflineVideoFrame& frame) {
  if (finished || !header_written || frame.frame_index.value() != next_video_frame ||
      frame.frame_index.value() < 0 || frame.frame_index.value() >= plan.frame_count() ||
      frame.pixel_format != OfflinePixelFormat::Rgba8888 || frame.width != video_codec->width ||
      frame.height != video_codec->height || frame.stride_bytes < frame.width * 4 ||
      frame.pixels.size() < static_cast<std::size_t>(frame.stride_bytes) * frame.height) {
    fail(ExportMediaWriterErrorCode::InvalidInputFrame,
         "video frame does not match the frozen export contract");
  }
  if (av_frame_make_writable(video_frame.get()) < 0) {
    fail(ExportMediaWriterErrorCode::VideoConversionFailed,
         "H.264 frame is not writable");
  }
  const auto* input_pixels = frame.pixels.data();
  const int input_stride = frame.stride_bytes;
  const auto converted = sws_scale(sws.get(), &input_pixels, &input_stride, 0,
                                   frame.height, video_frame->data, video_frame->linesize);
  if (converted != frame.height) {
    fail(ExportMediaWriterErrorCode::VideoConversionFailed,
         "RGBA to YUV420P conversion returned an incomplete frame");
  }
  video_frame->pts = next_video_frame;
  submit_video_frame(video_frame.get());
  ++next_video_frame;
  ++metrics.video_frames_submitted;
}

void FFmpegExportMediaWriter::Impl::set_audio_frame_samples(const float* interleaved,
                                                            int frame_samples) {
  if (av_frame_make_writable(audio_frame.get()) < 0) {
    fail(ExportMediaWriterErrorCode::AudioConversionFailed,
         "AAC frame is not writable");
  }
  audio_frame->nb_samples = frame_samples;
  if (audio_sample_format == AV_SAMPLE_FMT_FLTP) {
    auto* left = reinterpret_cast<float*>(audio_frame->data[0]);
    auto* right = reinterpret_cast<float*>(audio_frame->data[1]);
    for (int index = 0; index < frame_samples; ++index) {
      left[index] = interleaved[index * 2];
      right[index] = interleaved[index * 2 + 1];
    }
  } else if (audio_sample_format == AV_SAMPLE_FMT_FLT) {
    auto* packed = reinterpret_cast<float*>(audio_frame->data[0]);
    std::copy_n(interleaved, static_cast<std::size_t>(frame_samples) * 2, packed);
  } else {
    fail(ExportMediaWriterErrorCode::AudioConversionFailed,
         "unsupported AAC input sample format");
  }
}

void FFmpegExportMediaWriter::Impl::send_audio_frame(int frame_samples, const float* interleaved) {
  set_audio_frame_samples(interleaved, frame_samples);
  audio_frame->pts = encoded_audio_sample;
  submit_audio_frame(audio_frame.get());
  encoded_audio_sample += frame_samples;
  ++metrics.audio_encoder_frames;
}

void FFmpegExportMediaWriter::Impl::write_audio(const OfflineAudioBlock& block) {
  if (block.frame_count > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
    fail(ExportMediaWriterErrorCode::TimestampInvariant, "audio block frame count overflows index");
  }
  const auto block_frames = static_cast<std::int64_t>(block.frame_count);
  if (block.frame_count > std::numeric_limits<std::size_t>::max() / 2) {
    fail(ExportMediaWriterErrorCode::InvalidInputFrame,
         "audio block sample vector size overflows");
  }
  if (finished || !header_written || block.first_sample_index != next_audio_sample ||
      block.sample_rate != 48'000 || block.channel_count != 2 || block.frame_count == 0 ||
      block.samples.size() != block.frame_count * 2 || block_frames > plan.audio_sample_count() ||
      next_audio_sample > plan.audio_sample_count() - block_frames) {
    fail(block.first_sample_index != next_audio_sample
             ? ExportMediaWriterErrorCode::TimestampInvariant
             : ExportMediaWriterErrorCode::InvalidInputFrame,
         "audio block does not match the frozen sample-grid contract");
  }
  if (block_frames > std::numeric_limits<std::int64_t>::max() - next_audio_sample) {
    fail(ExportMediaWriterErrorCode::TimestampInvariant, "audio sample index overflow");
  }
  for (const auto sample : block.samples) {
    if (!std::isfinite(sample)) {
      fail(ExportMediaWriterErrorCode::InvalidInputFrame, "audio block contains NaN or Inf");
    }
  }
  next_audio_sample += block_frames;
  metrics.audio_samples_submitted += block.frame_count;
  // Keep the transient FIFO bounded even if a caller supplies one giant
  // OfflineAudioBlock. The residual is at most 4096 frames; erase(begin) is
  // therefore intentionally acceptable and its cost is bounded by this cap.
  audio_fifo.reserve(kMaxAudioFifoFrames * 2);
  for (std::size_t offset = 0; offset < block.samples.size();) {
    const auto available = kMaxAudioFifoFrames * 2 - audio_fifo.size();
    const auto count = std::min(available, block.samples.size() - offset);
    for (std::size_t index = 0; index < count; ++index) {
      // E4's explicit pre-encoder policy: deterministic hard clipping only.
      audio_fifo.push_back(detail::hard_clip_audio_sample(block.samples[offset + index]));
    }
    offset += count;
    metrics.peak_audio_fifo_frames = std::max<std::uint64_t>(
        metrics.peak_audio_fifo_frames, audio_fifo.size() / 2);
    while (audio_fifo.size() / 2 >= static_cast<std::size_t>(audio_frame_size)) {
      send_audio_frame(audio_frame_size, audio_fifo.data());
      const auto consumed = static_cast<std::size_t>(audio_frame_size) * 2;
      audio_fifo.erase(audio_fifo.begin(),
                       audio_fifo.begin() + static_cast<std::ptrdiff_t>(consumed));
    }
  }
}

void FFmpegExportMediaWriter::Impl::flush_video() {
  submit_video_flush();
}

void FFmpegExportMediaWriter::Impl::flush_audio() {
  if (!audio_fifo.empty()) {
    send_audio_frame(static_cast<int>(audio_fifo.size() / 2), audio_fifo.data());
    audio_fifo.clear();
  }
  submit_audio_flush();
}

void FFmpegExportMediaWriter::Impl::finish() {
  if (finished) return;
  if (next_video_frame != plan.frame_count() || next_audio_sample != plan.audio_sample_count()) {
    fail(ExportMediaWriterErrorCode::TimestampInvariant,
         "finish called before all planned video frames and audio samples were written");
  }
  try {
    flush_video();
    flush_audio();
    // Reassert logical durations after packet interleaving. The MP4 muxer may
    // otherwise derive video duration from the last DTS and omit the final
    // CFR frame interval when no subsequent packet exists.
    video_stream->duration = av_rescale_q(plan.frame_count(), video_codec->time_base,
                                          video_stream->time_base);
    audio_stream->duration = plan.audio_sample_count();
    const auto result = av_write_trailer(format.get());
    if (result < 0) fail(ExportMediaWriterErrorCode::FinalizeFailed,
                         "MP4 trailer write failed: " + ffmpeg_error_text(result));
    close_output();
    finished = true;
  } catch (...) {
    abort();
    throw;
  }
}

void FFmpegExportMediaWriter::Impl::close_output() noexcept {
  if (format && format->pb != nullptr && (format->oformat->flags & AVFMT_NOFILE) == 0) {
    avio_closep(&format->pb);
  }
}

void FFmpegExportMediaWriter::Impl::abort() noexcept {
  close_output();
  header_written = false;
  finished = true;
}

FFmpegExportMediaWriter::FFmpegExportMediaWriter(const ExportPlan& plan,
                                                 const std::filesystem::path& output_path)
    : impl_{std::make_unique<Impl>(plan, output_path)} {}

FFmpegExportMediaWriter::~FFmpegExportMediaWriter() = default;

void FFmpegExportMediaWriter::write_video(const application::exporting::OfflineVideoFrame& frame) {
  impl_->write_video(frame);
}

void FFmpegExportMediaWriter::write_audio(const application::exporting::OfflineAudioBlock& block) {
  impl_->write_audio(block);
}

void FFmpegExportMediaWriter::finish() { impl_->finish(); }

void FFmpegExportMediaWriter::abort() noexcept { impl_->abort(); }

const application::exporting::ExportMediaWriterMetrics&
FFmpegExportMediaWriter::metrics() const noexcept {
  return impl_->get_metrics();
}

} // namespace pulsar::media::ffmpeg
