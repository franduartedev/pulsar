// A1 audio decode tests.
//
// Fixture: tests/fixtures/audio_decode_fixture.mp4 (18244 bytes), generated
// once with the system ffmpeg 6.1.1 CLI and committed. No network access and
// no ffmpeg executable are required at test runtime:
//
//   ffmpeg -y -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=0.5"
//     -f lavfi -i "sine=frequency=660:sample_rate=44100:duration=0.5"
//     -f lavfi -i "sine=frequency=880:sample_rate=22050:duration=0.5"
//     -f lavfi -i "testsrc=size=64x64:rate=10:duration=0.5"
//     -filter_complex "[0:a][1:a]amerge=inputs=2[stereo]"
//     -map 3:v -map "[stereo]" -map 2:a
//     -c:v mpeg4 -pix_fmt yuv420p -c:a aac -shortest audio_decode_fixture.mp4
//
// Layout: stream 0 = mpeg4 video, stream 1 = AAC stereo 44100 Hz
// (440 Hz left, 660 Hz right), stream 2 = AAC mono 22050 Hz (880 Hz).
// AAC decodes natively to planar float (fltp), so the fixture exercises the
// libswresample planar-to-interleaved normalization path.
//
// Multichannel fixture: tests/fixtures/audio_multichannel_s16p.nut,
// generated once with the system ffmpeg 6.1.1 CLI and committed.
// DC constant (k+1)*0.05 on each of 16 channels, 0.1 s at 8000 Hz,
// encoded as planar s16le. Generation:
//
//   ffmpeg -y -f lavfi -i "aevalsrc=0.05:d=0.1:s=8000[a0];...[a15];
//     [a0]...[a15]amerge=inputs=16" -c:a pcm_s16le_planar
//     audio_multichannel_s16p.nut
//
// (One aevalsrc source per channel, values 0.05, 0.10, ..., 0.80.)
// Stream 0 decodes natively to 16-plane s16p, which exceeds the 8 inline
// AVFrame data pointers and must be collected via extended_data.
#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/media/audio_block.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_audio_decode_session.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_audio_playback_source.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/media_decode_error.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {
using namespace pulsar;
using application::AudioBlock;
using application::CommandProcessor;
using application::ProjectDocument;
using domain::MediaTime;
using domain::Rational;
using media::ffmpeg::FFmpegAudioDecodeSession;
using media::ffmpeg::FFmpegMediaProbe;
using media::ffmpeg::MediaDecodeError;
using media::ffmpeg::MediaDecodeErrorCode;

std::filesystem::path fixture_path() {
  return std::filesystem::path{PULSAR_SOURCE_DIR} / "tests" / "fixtures" /
         "audio_decode_fixture.mp4";
}

struct DecodedAudio {
  int sample_rate{0};
  int channel_count{0};
  std::size_t block_count{0};
  std::size_t frame_count{0};
  double max_abs{0.0};
  bool all_finite{true};
  std::optional<MediaTime> last_pts;
  std::size_t last_block_frames{0};
  std::vector<MediaTime> first_pts_per_block;
};

DecodedAudio decode_all(int stream_index) {
  FFmpegAudioDecodeSession session{fixture_path(), stream_index};
  DecodedAudio result;
  result.sample_rate = session.sample_rate();
  result.channel_count = session.channel_count();
  while (auto block = session.decode_next()) {
    assert(block->sample_rate() == session.sample_rate());
    assert(block->channel_count() == session.channel_count());
    assert(block->frame_count() > 0);
    assert(block->samples().size() ==
           block->frame_count() * static_cast<std::size_t>(session.channel_count()));
    ++result.block_count;
    result.frame_count += block->frame_count();
    for (float sample : block->samples()) {
      if (!std::isfinite(sample)) {
        result.all_finite = false;
      }
      const double magnitude = std::fabs(sample);
      if (magnitude > result.max_abs) {
        result.max_abs = magnitude;
      }
    }
    if (block->presentation_timestamp().has_value()) {
      if (result.last_pts.has_value()) {
        // Sequential decode of a fixed stream advances: every returned
        // block is fresh data, never a re-served previous buffer.
        assert(*block->presentation_timestamp() > *result.last_pts);
      }
      result.last_pts = *block->presentation_timestamp();
      result.last_block_frames = block->frame_count();
      result.first_pts_per_block.push_back(*block->presentation_timestamp());
    }
  }
  // EOF is stable: repeated decode after EOF resurrects nothing.
  for (int i = 0; i < 3; ++i) {
    assert(!session.decode_next().has_value());
  }
  return result;
}

// T1: the exact requested FFmpeg stream index is opened, not front() again.
void exact_stream_selection() {
  FFmpegAudioDecodeSession session{fixture_path(), 2};
  assert(session.stream_index() == 2);
  assert(session.sample_rate() == 22050);
  assert(session.channel_count() == 1);
  assert(session.codec_name() == "aac");
  assert((session.stream_time_base() == Rational{1, 22050}));
}

// T2: PCM shape — native rate/channels, consistent layout, finite non-silent signal.
void pcm_shape() {
  const auto decoded = decode_all(2);
  assert(decoded.block_count > 0);
  assert(decoded.frame_count > 0);
  assert(decoded.sample_rate == 22050);
  assert(decoded.channel_count == 1);
  assert(decoded.all_finite);
  assert(decoded.max_abs > 1e-3);

  const auto stereo = decode_all(1);
  assert(stereo.sample_rate == 44100);
  assert(stereo.channel_count == 2);
  assert(stereo.frame_count > 0);
  assert(stereo.all_finite);
  assert(stereo.max_abs > 1e-3);
}

// T3: the source decoder emits planar samples; the caller still receives
// packed interleaved float32 with distinct stereo channels.
void planar_normalization() {
  // Independent raw-FFmpeg observation (test only): the fixture's native
  // decoded format is planar float, so conversion is actually required.
  AVFormatContext* raw_context = nullptr;
  assert(avformat_open_input(&raw_context, fixture_path().c_str(), nullptr, nullptr) == 0);
  assert(avformat_find_stream_info(raw_context, nullptr) >= 0);
  AVStream* stream = raw_context->streams[1];
  const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
  assert(decoder != nullptr);
  AVCodecContext* codec_context = avcodec_alloc_context3(decoder);
  assert(codec_context != nullptr);
  assert(avcodec_parameters_to_context(codec_context, stream->codecpar) >= 0);
  assert(avcodec_open2(codec_context, decoder, nullptr) >= 0);
  AVPacket* packet = av_packet_alloc();
  AVFrame* frame = av_frame_alloc();
  bool saw_planar_frame = false;
  while (av_read_frame(raw_context, packet) >= 0 && !saw_planar_frame) {
    if (packet->stream_index != 1) {
      av_packet_unref(packet);
      continue;
    }
    if (avcodec_send_packet(codec_context, packet) == 0) {
      while (avcodec_receive_frame(codec_context, frame) == 0) {
        assert(av_sample_fmt_is_planar(
                   static_cast<AVSampleFormat>(frame->format)) != 0);
        saw_planar_frame = true;
        av_frame_unref(frame);
        break;
      }
    }
    av_packet_unref(packet);
  }
  av_frame_free(&frame);
  av_packet_free(&packet);
  avcodec_free_context(&codec_context);
  avformat_close_input(&raw_context);
  assert(saw_planar_frame);

  // Session output for the same stream is interleaved stereo float: left and
  // right channels carry different tones, so the even/odd subsequences differ.
  FFmpegAudioDecodeSession session{fixture_path(), 1};
  auto block = session.decode_next();
  assert(block.has_value());
  assert(block->channel_count() == 2);
  bool channels_differ = false;
  const auto& samples = block->samples();
  for (std::size_t i = 0; i + 1 < samples.size(); i += 2) {
    if (samples[i] != samples[i + 1]) {
      channels_differ = true;
      break;
    }
  }
  assert(channels_differ);
}

// T4: returned blocks own their PCM; later decodes cannot corrupt them.
void block_ownership() {
  FFmpegAudioDecodeSession session{fixture_path(), 1};
  auto first = session.decode_next();
  assert(first.has_value());
  const std::vector<float> snapshot = first->samples();
  const float* first_buffer = first->samples().data();
  const auto first_pts = first->presentation_timestamp();
  std::size_t later_blocks = 0;
  while (auto block = session.decode_next()) {
    ++later_blocks;
    assert(block->samples().data() != first_buffer);
    // Fresh owned data: consecutive blocks differ in signal or timing.
    const bool signal_differs = block->samples() != snapshot;
    const bool timing_differs = block->presentation_timestamp() != first_pts;
    assert(signal_differs || timing_differs);
  }
  assert(later_blocks > 0);
  assert(first->samples() == snapshot);
}

// T5: EOF flushes delayed samples; the decoded tail reaches the nominal end.
void eof_flushing() {
  FFmpegMediaProbe probe;
  const auto probed = probe.probe(fixture_path());
  int audio_index = -1;
  std::optional<MediaTime> audio_duration;
  for (const auto& audio : probed.audio_streams) {
    if (audio.stream_index == 1) {
      audio_index = audio.stream_index;
      audio_duration = audio.duration;
    }
  }
  assert(audio_index == 1);
  assert(audio_duration.has_value());

  const auto decoded = decode_all(1);
  // No delayed samples lost: decoded coverage reaches the probed duration.
  assert((MediaTime{static_cast<std::int64_t>(decoded.frame_count), Rational{1, 44100}} >=
          *audio_duration));
  assert(decoded.last_pts.has_value());
  const MediaTime tail_end{decoded.last_pts->value() +
                               static_cast<std::int64_t>(decoded.last_block_frames),
                           Rational{1, 44100}};
  assert(tail_end >= *audio_duration);
}

// T6: invalid and non-audio stream indices fail deterministically.
void invalid_stream_selection() {
  auto expect_code = [](int stream_index, MediaDecodeErrorCode code) {
    try {
      FFmpegAudioDecodeSession session{fixture_path(), stream_index};
      (void)session.decode_next();
    } catch (const MediaDecodeError& error) {
      assert(error.code() == code);
      return;
    }
    assert(false);
  };
  expect_code(99, MediaDecodeErrorCode::InvalidStreamIndex);
  expect_code(-1, MediaDecodeErrorCode::InvalidStreamIndex);
  expect_code(0, MediaDecodeErrorCode::SelectedStreamIsNotAudio);
}

void wide_media_time_seek_uses_exact_stream_rescale() {
  FFmpegAudioDecodeSession session{fixture_path(), 1};

  // The exact PULSAR timestamp has a denominator outside AVRational's int
  // fields, but rounds deterministically to the first stream tick.
  assert(session.seek_to(MediaTime{1, Rational{1, 3000000000}}));

  bool rejected = false;
  try {
    (void)session.seek_to(MediaTime{std::numeric_limits<std::int64_t>::max(), Rational{1, 1}});
  } catch (const MediaDecodeError& error) {
    rejected = error.code() == MediaDecodeErrorCode::InvalidSeekTarget;
  }
  assert(rejected);
}

// T7: A0 identity (MediaAsset::audio stream index) opens the A1 decoder.
void a0_identity_integration() {
  ProjectDocument document;
  CommandProcessor processor{document};
  FFmpegMediaProbe probe;
  const auto media = processor.import_asset(fixture_path(), probe);
  const auto asset = document.find_asset(media);
  assert(asset.has_value());
  assert(asset->audio().has_value());
  assert(asset->audio()->stream_index == 1);

  FFmpegAudioDecodeSession session{fixture_path(), asset->audio()->stream_index};
  assert(session.sample_rate() == asset->audio()->sample_rate);
  assert(session.channel_count() == asset->audio()->channel_count);
  std::size_t blocks = 0;
  while (auto block = session.decode_next()) {
    ++blocks;
  }
  assert(blocks > 0);
}

// Regression for the fixed input-plane array: planar audio with more
// channels than AV_NUM_DATA_POINTERS inline pointers must keep every
// channel. Each channel carries a distinct DC level ((k+1)*0.05).
void multichannel_planar_beyond_inline_planes() {
  const std::filesystem::path path{std::filesystem::path{PULSAR_SOURCE_DIR} / "tests" /
                                   "fixtures" / "audio_multichannel_s16p.nut"};
  FFmpegAudioDecodeSession session{path, 0};
  assert(session.stream_index() == 0);
  assert(session.sample_rate() == 8000);
  assert(session.channel_count() == 16);
  assert(session.channel_count() > AV_NUM_DATA_POINTERS);

  std::size_t total_frames = 0;
  std::vector<double> channel_sums(16, 0.0);
  std::size_t blocks = 0;
  while (auto block = session.decode_next()) {
    assert(block->channel_count() == 16);
    assert(block->frame_count() > 0);
    assert(block->samples().size() ==
           block->frame_count() * static_cast<std::size_t>(16));
    ++blocks;
    total_frames += block->frame_count();
    const auto& samples = block->samples();
    for (std::size_t i = 0; i < samples.size(); ++i) {
      assert(std::isfinite(samples[i]));
      channel_sums[i % 16] += samples[i];
    }
  }
  assert(blocks > 0);
  // Exact PCM length: 0.1 s at 8000 Hz, no codec priming or padding.
  assert(total_frames == 800);
  for (int channel = 0; channel < 16; ++channel) {
    const double mean =
        channel_sums[static_cast<std::size_t>(channel)] / static_cast<double>(total_frames);
    const double expected = 0.05 * static_cast<double>(channel + 1);
    // s16 quantization step is ~3e-5; tolerance stays far below the 0.05
    // spacing between adjacent channels, so every channel is distinguished.
    assert(std::fabs(mean - expected) < 2e-3);
  }
  // High-index channels (beyond the old inline limit) carry their own
  // signal: channel 15 is ~0.8, not zero, null, or a copy of a low channel.
  const double high_mean = channel_sums[15] / static_cast<double>(total_frames);
  assert(high_mean > 0.79);
  for (int channel = 0; channel + 1 < 16; ++channel) {
    const double mean = channel_sums[static_cast<std::size_t>(channel)] /
                        static_cast<double>(total_frames);
    const double next = channel_sums[static_cast<std::size_t>(channel + 1)] /
                        static_cast<double>(total_frames);
    assert(next > mean);
  }
  for (int i = 0; i < 3; ++i) {
    assert(!session.decode_next().has_value());
  }
}

// Timestamp contract: block timing uses the audio stream time base,
// is nondecreasing, and is never compared against video timing.
void timestamp_contract() {
  const auto decoded = decode_all(1);
  assert(!decoded.first_pts_per_block.empty());
  for (const auto& pts : decoded.first_pts_per_block) {
    assert((pts.time_base() == Rational{1, 44100}));
  }
  // AudioBlock rejects timestamps expressed in any other time base.
  bool rejected = false;
  try {
    (void)AudioBlock{44100, 2, 1, std::vector<float>{0.0F, 0.0F}, Rational{1, 44100},
                     MediaTime{0, Rational{1, 90000}}};
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void seek_resets_decoder_state() {
  FFmpegAudioDecodeSession session{fixture_path(), 1};
  assert(session.decode_next().has_value());
  assert(session.seek_to(MediaTime{8820, Rational{1, 44100}}));
  auto block = session.decode_next();
  assert(block.has_value());
  assert(block->frame_count() > 0);
  assert(block->presentation_timestamp().has_value());
  assert((*block->presentation_timestamp() < MediaTime{8820, Rational{1, 44100}}));
}

void builtin_sound_playback_source_is_non_silent() {
  const auto path = std::filesystem::path{PULSAR_SOURCE_DIR} / "assets" / "sounds" /
                    "impact_heavy.wav";
  media::ffmpeg::FFmpegAudioPlaybackSource source{path, 0};
  assert(source.sample_rate() == 48000);
  assert(source.channel_count() == 1);
  double peak = 0.0;
  double sum_squares = 0.0;
  std::size_t count = 0;
  while (auto block = source.decode_next()) {
    for (const float sample : block->samples()) {
      assert(std::isfinite(sample));
      peak = std::max(peak, std::fabs(static_cast<double>(sample)));
      sum_squares += static_cast<double>(sample) * sample;
      ++count;
    }
  }
  assert(count > 0);
  assert(peak > 1e-3);
  assert(std::sqrt(sum_squares / static_cast<double>(count)) > 1e-4);
}

}  // namespace

int main() {
  exact_stream_selection();
  pcm_shape();
  planar_normalization();
  block_ownership();
  eof_flushing();
  invalid_stream_selection();
  wide_media_time_seek_uses_exact_stream_rescale();
  a0_identity_integration();
  timestamp_contract();
  multichannel_planar_beyond_inline_planes();
  seek_resets_decoder_state();
  builtin_sound_playback_source_is_non_silent();
}
