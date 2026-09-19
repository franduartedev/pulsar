#include <pulsar/media/ffmpeg/ffmpeg_audio_decode_session.hpp>

#include <pulsar/media/ffmpeg/media_decode_error.hpp>

#include "internal/ffmpeg_common.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace pulsar::media::ffmpeg {
namespace {

using application::AudioBlock;
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

struct ChannelLayoutDeleter {
  void operator()(AVChannelLayout* layout) const noexcept {
    if (layout != nullptr) {
      av_channel_layout_uninit(layout);
    }
  }
};

struct SwrContextDeleter {
  void operator()(SwrContext* context) const noexcept {
    if (context != nullptr) {
      swr_free(&context);
    }
  }
};

using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

[[nodiscard]] CodecContextPtr allocate_decoder_context(const AVCodec& decoder,
                                                       const AVCodecParameters& parameters) {
  AVCodecContext* raw_context = avcodec_alloc_context3(&decoder);
  if (raw_context == nullptr) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                           "FFmpeg could not allocate audio decoder context"};
  }

  CodecContextPtr context{raw_context};
  const int parameters_result = avcodec_parameters_to_context(context.get(), &parameters);
  if (parameters_result < 0) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                           "FFmpeg could not copy audio decoder parameters: " +
                               ffmpeg_error_text(parameters_result)};
  }

  const int open_result = avcodec_open2(context.get(), &decoder, nullptr);
  if (open_result < 0) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                           "FFmpeg could not open audio decoder: " +
                               ffmpeg_error_text(open_result)};
  }

  return context;
}

[[nodiscard]] PacketPtr allocate_packet() {
  AVPacket* packet = av_packet_alloc();
  if (packet == nullptr) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "FFmpeg could not allocate audio decode packet"};
  }

  return PacketPtr{packet};
}

[[nodiscard]] FramePtr allocate_frame() {
  AVFrame* frame = av_frame_alloc();
  if (frame == nullptr) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "FFmpeg could not allocate audio decode frame"};
  }

  return FramePtr{frame};
}

// A1 conversion policy: keep the source channel order and sample rate, only
// normalize the sample format (planar/packed integer/float) to packed
// interleaved float32. No resampling, no rematrixing, no speed change.
[[nodiscard]] SwrContextPtr allocate_resampler(const AVChannelLayout& channel_layout,
                                               int sample_rate, AVSampleFormat input_format) {
  SwrContext* raw_context = nullptr;
  const int alloc_result = swr_alloc_set_opts2(&raw_context, &channel_layout, AV_SAMPLE_FMT_FLT,
                                               sample_rate, &channel_layout, input_format,
                                               sample_rate, 0, nullptr);
  if (alloc_result < 0 || raw_context == nullptr) {
    if (raw_context != nullptr) {
      swr_free(&raw_context);
    }
    throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                           "libswresample could not configure float conversion: " +
                               ffmpeg_error_text(alloc_result)};
  }

  SwrContextPtr context{raw_context};
  const int init_result = swr_init(context.get());
  if (init_result < 0) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                           "libswresample could not initialize float conversion: " +
                               ffmpeg_error_text(init_result)};
  }

  return context;
}

[[nodiscard]] std::unique_ptr<AVChannelLayout, ChannelLayoutDeleter> make_channel_layout(
    const AVChannelLayout& reported, int channel_count) {
  auto layout = std::unique_ptr<AVChannelLayout, ChannelLayoutDeleter>(new AVChannelLayout{});
  const bool reported_usable = channel_count > 0 && reported.nb_channels == channel_count &&
                               av_channel_layout_check(&reported) != 0 &&
                               av_channel_layout_copy(layout.get(), &reported) == 0;
  if (reported_usable) {
    return layout;
  }
  av_channel_layout_uninit(layout.get());
  bool fallback_usable = false;
  if (channel_count > 0) {
    av_channel_layout_default(layout.get(), channel_count);
    fallback_usable = av_channel_layout_check(layout.get()) != 0;
  }
  if (!fallback_usable) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                           "FFmpeg reported an unusable audio channel layout"};
  }
  return layout;
}

[[nodiscard]] std::size_t checked_sample_capacity(int frame_capacity, int channel_count) {
  if (frame_capacity < 0 || channel_count <= 0) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "audio conversion received invalid frame capacity"};
  }
  const auto capacity = static_cast<std::uint64_t>(frame_capacity) *
                        static_cast<std::uint64_t>(channel_count);
  if (capacity > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "audio conversion buffer size overflowed"};
  }
  return static_cast<std::size_t>(capacity);
}

} // namespace

class FFmpegAudioDecodeSession::Impl {
public:
  Impl(const std::filesystem::path& path, int audio_stream_index)
      : context_{internal::open_input_with_stream_info(
            internal::canonical_local_regular_file(path))},
        stream_index_{audio_stream_index} {
    if (stream_index_ < 0 ||
        static_cast<unsigned int>(stream_index_) >= context_->nb_streams) {
      throw MediaDecodeError{MediaDecodeErrorCode::InvalidStreamIndex,
                             "selected audio stream index is out of range"};
    }

    AVStream& stream = *context_->streams[static_cast<unsigned int>(stream_index_)];
    if (stream.codecpar == nullptr) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                             "selected audio stream has no codec parameters"};
    }
    if (stream.codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
      throw MediaDecodeError{MediaDecodeErrorCode::SelectedStreamIsNotAudio,
                             "selected stream is not an audio stream"};
    }
    if (stream.codecpar->sample_rate <= 0) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                             "selected audio stream has no positive sample rate"};
    }
    if (stream.codecpar->ch_layout.nb_channels <= 0) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                             "selected audio stream has no positive channel count"};
    }

    const AVCodec* decoder = avcodec_find_decoder(stream.codecpar->codec_id);
    if (decoder == nullptr) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecoderUnavailable,
                             "no FFmpeg decoder is available for selected audio stream"};
    }

    codec_name_ = internal::codec_name(*stream.codecpar);
    stream_time_base_ffmpeg_ = stream.time_base;
    stream_time_base_ = internal::convert_required_time_base(stream.time_base);
    stream_start_timestamp_ =
        internal::convert_timestamp(stream.start_time, stream_time_base_);
    codec_context_ = allocate_decoder_context(*decoder, *stream.codecpar);
    sample_rate_ = codec_context_->sample_rate;
    channel_count_ = codec_context_->ch_layout.nb_channels;
    if (sample_rate_ <= 0 || channel_count_ <= 0) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecoderInitializationFailed,
                             "opened audio decoder has invalid sample rate or channel count"};
    }
    channel_layout_ = make_channel_layout(codec_context_->ch_layout, channel_count_);
    input_sample_format_ = codec_context_->sample_fmt;
    resampler_ = allocate_resampler(*channel_layout_, sample_rate_, input_sample_format_);
    packet_ = allocate_packet();
    frame_ = allocate_frame();
  }

  [[nodiscard]] std::optional<AudioBlock> decode_next() {
    if (pending_tail_.has_value()) {
      auto pending = std::move(pending_tail_);
      pending_tail_.reset();
      return pending;
    }

    if (state_ == State::Finished) {
      return std::nullopt;
    }

    while (true) {
      if (auto block = receive_pending_block()) {
        return block;
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

  [[nodiscard]] bool seek_to(domain::MediaTime timestamp) {
    const auto source_base = timestamp.time_base();
    const auto in_range = [](std::int64_t value) {
      return value >= std::numeric_limits<int>::min() &&
             value <= std::numeric_limits<int>::max();
    };
    if (!in_range(source_base.numerator()) || !in_range(source_base.denominator()) ||
        source_base.denominator() <= 0) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                             "audio seek timestamp is outside FFmpeg range"};
    }
    const AVRational source_time_base{static_cast<int>(source_base.numerator()),
                                      static_cast<int>(source_base.denominator())};
    const auto target = av_rescale_q(timestamp.value(), source_time_base,
                                     stream_time_base_ffmpeg_);
    const int result = avformat_seek_file(context_.get(), stream_index_,
                                           std::numeric_limits<std::int64_t>::min(), target,
                                           std::numeric_limits<std::int64_t>::max(),
                                           AVSEEK_FLAG_BACKWARD);
    if (result < 0) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                             "FFmpeg could not seek audio stream: " + ffmpeg_error_text(result)};
    }
    avcodec_flush_buffers(codec_context_.get());
    av_packet_unref(packet_.get());
    av_frame_unref(frame_.get());
    swr_close(resampler_.get());
    const int init_result = swr_init(resampler_.get());
    if (init_result < 0) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                             "libswresample could not reset after audio seek: " +
                                 ffmpeg_error_text(init_result)};
    }
    pending_tail_.reset();
    state_ = State::Reading;
    has_pending_packet_ = false;
    pending_send_was_blocked_ = false;
    return true;
  }

  [[nodiscard]] int stream_index() const noexcept { return stream_index_; }
  [[nodiscard]] int sample_rate() const noexcept { return sample_rate_; }
  [[nodiscard]] int channel_count() const noexcept { return channel_count_; }
  [[nodiscard]] const std::string& codec_name() const noexcept { return codec_name_; }
  [[nodiscard]] const domain::Rational& stream_time_base() const noexcept {
    return stream_time_base_;
  }
  [[nodiscard]] std::optional<domain::MediaTime> stream_start_timestamp() const {
    return stream_start_timestamp_;
  }

private:
  enum class State {
    Reading,
    Draining,
    Finished,
  };

  [[nodiscard]] std::optional<AudioBlock> convert_frame(const AVFrame& frame) {
    if (frame.nb_samples <= 0) {
      return std::nullopt;
    }
    const int out_capacity = swr_get_out_samples(resampler_.get(), frame.nb_samples);
    if (out_capacity <= 0) {
      return std::nullopt;
    }
    std::vector<float> samples(checked_sample_capacity(out_capacity, channel_count_));
    std::uint8_t* output = reinterpret_cast<std::uint8_t*>(samples.data());
    // Plane pointers scale with the decoded channel count: planar audio
    // with more than AV_NUM_DATA_POINTERS channels exposes the extra planes
    // through AVFrame::extended_data only.
    const int plane_count =
        av_sample_fmt_is_planar(input_sample_format_) != 0 ? channel_count_ : 1;
    std::vector<const std::uint8_t*> input_planes(static_cast<std::size_t>(plane_count));
    for (int plane = 0; plane < plane_count; ++plane) {
      const std::uint8_t* data = frame.extended_data[static_cast<std::size_t>(plane)];
      if (data == nullptr) {
        throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                               "FFmpeg audio frame is missing a channel plane"};
      }
      input_planes[static_cast<std::size_t>(plane)] = data;
    }
    const int converted = swr_convert(resampler_.get(), &output, out_capacity,
                                      input_planes.data(), frame.nb_samples);
    if (converted < 0) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                             "libswresample could not convert audio samples: " +
                                 ffmpeg_error_text(converted)};
    }
    if (converted == 0) {
      return std::nullopt;
    }
    samples.resize(checked_sample_capacity(converted, channel_count_));
    // Block timing is the decoded frame's own source presentation timestamp
    // in the audio stream time base. No timeline mapping is performed.
    return AudioBlock{sample_rate_, channel_count_, static_cast<std::size_t>(converted),
                      std::move(samples), stream_time_base_,
                      internal::convert_timestamp(frame.best_effort_timestamp, stream_time_base_)};
  }

  [[nodiscard]] std::optional<AudioBlock> flush_converter() {
    state_ = State::Finished;
    const int buffered = swr_get_out_samples(resampler_.get(), 0);
    if (buffered <= 0) {
      return std::nullopt;
    }
    std::vector<float> samples(checked_sample_capacity(buffered, channel_count_));
    std::uint8_t* output = reinterpret_cast<std::uint8_t*>(samples.data());
    const int converted = swr_convert(resampler_.get(), &output, buffered, nullptr, 0);
    if (converted < 0) {
      throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                             "libswresample could not flush converted audio samples: " +
                                 ffmpeg_error_text(converted)};
    }
    if (converted == 0) {
      return std::nullopt;
    }
    samples.resize(checked_sample_capacity(converted, channel_count_));
    // Flushed tail samples have no single source frame timestamp.
    return AudioBlock{sample_rate_, channel_count_, static_cast<std::size_t>(converted),
                      std::move(samples), stream_time_base_, std::nullopt};
  }

  [[nodiscard]] std::optional<AudioBlock> receive_pending_block() {
    const int receive_result = avcodec_receive_frame(codec_context_.get(), frame_.get());
    if (receive_result == 0) {
      pending_send_was_blocked_ = false;
      auto block = convert_frame(*frame_);
      av_frame_unref(frame_.get());
      return block;
    }

    if (receive_result == AVERROR(EAGAIN)) {
      if (pending_send_was_blocked_) {
        throw MediaDecodeError{
            MediaDecodeErrorCode::DecodeFailed,
            "FFmpeg audio decoder could neither receive a frame nor accept the pending packet"};
      }
      return std::nullopt;
    }

    if (receive_result == AVERROR_EOF) {
      return flush_converter();
    }

    throw MediaDecodeError{MediaDecodeErrorCode::DecodeFailed,
                           "FFmpeg audio decoder failed while receiving frame: " +
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
                             "FFmpeg audio decoder failed while sending packet: " +
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
                             "FFmpeg audio decoder failed while entering drain mode: " +
                                 ffmpeg_error_text(send_result)};
    }

    state_ = State::Draining;
  }

  internal::FormatContextPtr context_;
  CodecContextPtr codec_context_;
  std::unique_ptr<AVChannelLayout, ChannelLayoutDeleter> channel_layout_;
  SwrContextPtr resampler_;
  PacketPtr packet_;
  FramePtr frame_;
  int stream_index_;
  int sample_rate_{0};
  int channel_count_{0};
  AVSampleFormat input_sample_format_{AV_SAMPLE_FMT_NONE};
  AVRational stream_time_base_ffmpeg_{0, 1};
  domain::Rational stream_time_base_{1, 1};
  std::string codec_name_{"unknown"};
  std::optional<AudioBlock> pending_tail_;
  std::optional<domain::MediaTime> stream_start_timestamp_;
  State state_{State::Reading};
  bool has_pending_packet_{false};
  bool pending_send_was_blocked_{false};
};

FFmpegAudioDecodeSession::FFmpegAudioDecodeSession(const std::filesystem::path& path,
                                                   int audio_stream_index)
    : impl_{std::make_unique<Impl>(path, audio_stream_index)} {}

FFmpegAudioDecodeSession::~FFmpegAudioDecodeSession() = default;

FFmpegAudioDecodeSession::FFmpegAudioDecodeSession(FFmpegAudioDecodeSession&&) noexcept = default;

FFmpegAudioDecodeSession& FFmpegAudioDecodeSession::operator=(
    FFmpegAudioDecodeSession&&) noexcept = default;

std::optional<application::AudioBlock> FFmpegAudioDecodeSession::decode_next() {
  return impl_->decode_next();
}

bool FFmpegAudioDecodeSession::seek_to(domain::MediaTime timestamp) {
  return impl_->seek_to(timestamp);
}

int FFmpegAudioDecodeSession::stream_index() const noexcept {
  return impl_->stream_index();
}

int FFmpegAudioDecodeSession::sample_rate() const noexcept {
  return impl_->sample_rate();
}

int FFmpegAudioDecodeSession::channel_count() const noexcept {
  return impl_->channel_count();
}

const std::string& FFmpegAudioDecodeSession::codec_name() const noexcept {
  return impl_->codec_name();
}

const domain::Rational& FFmpegAudioDecodeSession::stream_time_base() const noexcept {
  return impl_->stream_time_base();
}

std::optional<domain::MediaTime> FFmpegAudioDecodeSession::stream_start_timestamp() const {
  return impl_->stream_start_timestamp();
}

} // namespace pulsar::media::ffmpeg
