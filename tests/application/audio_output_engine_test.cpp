// A2 deterministic engine tests: lifecycle, queue, streaming conversion,
// metrics. No Qt, no threads except the cross-thread regression (bounded).
// A FakeBackend stands in for QAudioSink.
#include <pulsar/application/audio/audio_converter.hpp>
#include <pulsar/application/audio/audio_output_engine.hpp>
#include <pulsar/application/media/audio_block.hpp>

#include <cassert>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <future>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace pulsar;
using application::AudioBlock;
using application::AudioConversionError;
using application::AudioDeviceBackend;
using application::AudioDeviceFormat;
using application::AudioEnqueueResult;
using application::AudioEngineLimits;
using application::AudioOutputEngine;
using application::AudioOutputState;
using application::AudioSampleFormat;
using application::AudioSourceFormat;
using application::ConvertedAudioBlock;
using application::StreamingAudioConverter;
using domain::Rational;

AudioBlock make_block(int sample_rate, int channels, std::size_t frames, float fill = 0.25F) {
  return AudioBlock{sample_rate, channels, frames,
                    std::vector<float>(frames * static_cast<std::size_t>(channels), fill),
                    Rational{1, sample_rate}, std::nullopt};
}

class FakeBackend final : public AudioDeviceBackend {
public:
  std::optional<AudioDeviceFormat> offered;
  bool open_ok{true};
  bool start_ok{true};
  std::size_t writable_cap{std::numeric_limits<std::size_t>::max()};
  std::uint64_t written{0};
  std::uint64_t processed{0};
  std::uint64_t pending{0};
  std::vector<std::uint8_t> captured;
  bool auto_process{true};
  // When set, the marked backend calls block until release_gate().
  // Proves the engine never holds its mutex across backend calls (B6).
  bool block_writes{false};
  bool block_stops{false};
  std::optional<std::string> injected_error;
  std::size_t frame_bytes_{4};
  int open_count{0};
  int close_count{0};
  int start_count{0};
  int stop_count{0};
  int reset_count{0};
  AudioDeviceFormat opened_format{0, 0, AudioSampleFormat::Float32};

  void release_gate() {
    {
      std::lock_guard lock{gate_mutex_};
      block_writes = false;
      block_stops = false;
    }
    gate_cv_.notify_all();
  }

  std::optional<AudioDeviceFormat> query_device_format(const AudioSourceFormat&) override {
    return offered;
  }
  bool open_device(const AudioDeviceFormat& format) override {
    if (!open_ok) {
      return false;
    }
    opened_format = format;
    frame_bytes_ = static_cast<std::size_t>(format.channel_count) *
                   application::device_sample_bytes(format.sample_format);
    ++open_count;
    return true;
  }
  void close_device() override { ++close_count; }
  bool start_device() override {
    if (!start_ok) {
      return false;
    }
    ++start_count;
    return true;
  }
  void stop_device() override {
    if (block_stops) {
      std::unique_lock lock{gate_mutex_};
      gate_cv_.wait(lock, [this] { return !block_stops; });
    }
    ++stop_count;
  }
  void reset_device() override {
    ++reset_count;
    pending = 0;
    processed = written;
  }
  std::size_t writable_frames() override { return writable_cap; }
  std::size_t write_frames(const void* frames, std::size_t frame_count) override {
    if (block_writes) {
      std::unique_lock lock{gate_mutex_};
      gate_cv_.wait(lock, [this] { return !block_writes; });
    }
    const std::size_t taken = frame_count > writable_cap ? writable_cap : frame_count;
    const auto* bytes = static_cast<const std::uint8_t*>(frames);
    captured.insert(captured.end(), bytes, bytes + taken * frame_bytes_);
    written += taken;
    pending += taken;
    if (auto_process) {
      consume(taken);
    }
    return taken;
  }
  std::uint64_t processed_frames() override { return processed; }
  bool is_playback_exhausted() override { return pending == 0; }
  std::optional<std::string> backend_error() override { return injected_error; }

  void consume(std::uint64_t frames) {
    const std::uint64_t take = frames > pending ? pending : frames;
    pending -= take;
    processed += take;
  }

private:
  std::mutex gate_mutex_;
  std::condition_variable gate_cv_;
};

void converter_passthrough_and_quantization() {
  StreamingAudioConverter passthrough{44100, 2, AudioDeviceFormat{44100, 2,
                                                                  AudioSampleFormat::Float32}};
  const AudioBlock stereo = make_block(44100, 2, 4, 0.5F);
  const ConvertedAudioBlock same = passthrough.convert(stereo);
  // Equal rates still hold the final source frame for the next block.
  assert(same.frame_count == 3);
  assert(same.frames.size() == 3 * 2 * 4);
  assert((reinterpret_cast<const float*>(same.frames.data())[0]) == 0.5F);
  const ConvertedAudioBlock tail = passthrough.flush_tail();
  assert(tail.frame_count == 1);
  assert((reinterpret_cast<const float*>(tail.frames.data())[0]) == 0.5F);

  StreamingAudioConverter to_int16{44100, 2, AudioDeviceFormat{44100, 2, AudioSampleFormat::Int16}};
  const ConvertedAudioBlock as_int16 = to_int16.convert(make_block(44100, 2, 4, 0.5F));
  const auto* words = reinterpret_cast<const std::int16_t*>(as_int16.frames.data());
  assert(words[0] == 16384);

  StreamingAudioConverter clipper{44100, 1, AudioDeviceFormat{44100, 1, AudioSampleFormat::Int16}};
  AudioBlock clip{44100, 1, 3, std::vector<float>{2.0F, -2.0F, 0.0F}, Rational{1, 44100},
                  std::nullopt};
  const ConvertedAudioBlock clipped = clipper.convert(clip);
  const auto* clipped_words = reinterpret_cast<const std::int16_t*>(clipped.frames.data());
  assert(clipped_words[0] == 32767);
  assert(clipped_words[1] == -32768);

  StreamingAudioConverter to_u8{44100, 2, AudioDeviceFormat{44100, 2, AudioSampleFormat::UInt8}};
  const ConvertedAudioBlock as_u8 = to_u8.convert(make_block(44100, 2, 4, 0.5F));
  assert(as_u8.frames[0] == 191);

  StreamingAudioConverter widen{44100, 1, AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32}};
  AudioBlock mono{44100, 1, 2, std::vector<float>{0.25F, -0.5F}, Rational{1, 44100}, std::nullopt};
  const ConvertedAudioBlock wide = widen.convert(mono);
  assert(wide.frame_count == 1);
  const auto* wide_floats = reinterpret_cast<const float*>(wide.frames.data());
  assert(wide_floats[0] == 0.25F && wide_floats[1] == 0.25F);

  StreamingAudioConverter narrow{44100, 2, AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32}};
  AudioBlock pair{44100, 2, 2, std::vector<float>{1.0F, -1.0F, 1.0F, -1.0F}, Rational{1, 44100},
                  std::nullopt};
  const ConvertedAudioBlock narrowed = narrow.convert(pair);
  assert(narrowed.frame_count == 1);
  assert((reinterpret_cast<const float*>(narrowed.frames.data())[0]) == 0.0F);
}

// One contiguous buffer vs the same PCM split into irregular blocks:
// identical totals, equivalent waveform, no per-block phase reset.
void streaming_split_matches_contiguous() {
  constexpr int kSourceRate = 44100;
  constexpr int kDeviceRate = 48000;
  constexpr std::size_t kTotal = 4000;
  std::vector<float> pcm(kTotal);
  for (std::size_t i = 0; i < kTotal; ++i) {
    pcm[i] = static_cast<float>(0.5 * std::sin(2.0 * 3.141592653589793 *
                                               440.0 * static_cast<double>(i) / 44100.0));
  }
  const AudioDeviceFormat device{kDeviceRate, 1, AudioSampleFormat::Float32};

  StreamingAudioConverter whole{kSourceRate, 1, device};
  AudioBlock contiguous{kSourceRate, 1, kTotal, pcm, Rational{1, kSourceRate}, std::nullopt};
  ConvertedAudioBlock reference = whole.convert(contiguous);
  const ConvertedAudioBlock whole_tail = whole.flush_tail();
  std::vector<float> reference_floats(reference.frame_count + whole_tail.frame_count);
  std::memcpy(reference_floats.data(), reference.frames.data(), reference.frames.size());
  std::memcpy(reference_floats.data() + reference.frame_count, whole_tail.frames.data(),
              whole_tail.frames.size());
  const std::size_t reference_count = reference_floats.size();

  const std::vector<std::size_t> splits{13, 71, 2, 511, 37, 1024, 3, 256, 789, 17, 5, 900, 372};
  std::size_t check_sum = 0;
  for (auto size : splits) {
    check_sum += size;
  }
  assert(check_sum == kTotal);

  StreamingAudioConverter piece{kSourceRate, 1, device};
  std::vector<float> split_floats;
  std::size_t offset = 0;
  for (auto size : splits) {
    AudioBlock part{kSourceRate, 1, size,
                    std::vector<float>(pcm.begin() + static_cast<std::ptrdiff_t>(offset),
                                       pcm.begin() + static_cast<std::ptrdiff_t>(offset + size)),
                    Rational{1, kSourceRate}, std::nullopt};
    const ConvertedAudioBlock out = piece.convert(part);
    const auto* floats = reinterpret_cast<const float*>(out.frames.data());
    split_floats.insert(split_floats.end(), floats, floats + out.frame_count);
    offset += size;
  }
  const ConvertedAudioBlock split_tail = piece.flush_tail();
  const auto* tail_floats = reinterpret_cast<const float*>(split_tail.frames.data());
  split_floats.insert(split_floats.end(), tail_floats, tail_floats + split_tail.frame_count);

  // Same lattice: exact total, equivalent waveform (float rounding only).
  assert(split_floats.size() == reference_count);
  double worst = 0.0;
  for (std::size_t i = 0; i < reference_count; ++i) {
    const double delta = std::fabs(static_cast<double>(split_floats[i]) - reference_floats[i]);
    if (delta > worst) {
      worst = delta;
    }
  }
  assert(worst < 1e-5);
  // No boundary discontinuity beyond the sine slope (~0.029/frame at 48 kHz).
  for (std::size_t i = 1; i < split_floats.size(); ++i) {
    assert(std::fabs(static_cast<double>(split_floats[i]) - split_floats[i - 1]) < 0.05);
  }
}

void converter_rejects_unguessable_layouts() {
  bool threw = false;
  try {
    StreamingAudioConverter bad{44100, 3, AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32}};
    (void)bad;
  } catch (const AudioConversionError&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    StreamingAudioConverter bad{44100, 2, AudioDeviceFormat{44100, 6, AudioSampleFormat::Float32}};
    (void)bad;
  } catch (const AudioConversionError&) {
    threw = true;
  }
  assert(threw);
}

void lifecycle_open_close_reopen() {
  FakeBackend backend;
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.state() == AudioOutputState::Closed);
  assert(!engine.device_format().has_value());
  // No device offered: open fails into Error, close recovers to Closed.
  assert(!engine.open({44100, 2}));
  assert(engine.state() == AudioOutputState::Error);
  assert(engine.error()->code == application::AudioOutputError::Code::NoDevice);
  engine.close();
  assert(engine.state() == AudioOutputState::Closed);
  assert(!engine.error().has_value());
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  assert(engine.open({44100, 2}));
  assert(engine.state() == AudioOutputState::Opened);
  assert(engine.device_format().has_value());
  assert(engine.device_format()->sample_rate == 44100);
  assert(!engine.open({44100, 2}));
  assert(engine.state() == AudioOutputState::Opened);
  engine.close();
  assert(engine.state() == AudioOutputState::Closed);
  assert(!engine.device_format().has_value());
  assert(!engine.error().has_value());
  assert(engine.open({44100, 2}));
  assert(backend.open_count == 2);
  engine.close();
}

void open_rejects_bad_source_and_layouts() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{48000, 2, AudioSampleFormat::Int16};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(!engine.open({0, 2}));
  assert(engine.state() == AudioOutputState::Error);
  assert(engine.error()->code == application::AudioOutputError::Code::UnsupportedConversion);
  engine.close();
  backend.offered = AudioDeviceFormat{48000, 2, AudioSampleFormat::Int16};
  assert(!engine.open({44100, 5}));
  assert(engine.state() == AudioOutputState::Error);
  assert(engine.error()->code == application::AudioOutputError::Code::UnsupportedConversion);
  engine.close();
  assert(!engine.error().has_value());
}

void enqueue_contract_atomicity() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{100}};
  assert(engine.enqueue(make_block(44100, 2, 8)) == AudioEnqueueResult::InvalidState);
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(48000, 2, 8)) == AudioEnqueueResult::FormatMismatch);
  assert(engine.enqueue(make_block(44100, 1, 8)) == AudioEnqueueResult::FormatMismatch);
  assert(engine.enqueue(make_block(44100, 2, 60)) == AudioEnqueueResult::Accepted);
  assert(engine.metrics().source_frames_accepted == 60);
  // Equal rates convert 60 source frames into 59 device frames; the 60th
  // waits for a successor sample.
  assert(engine.metrics().buffered_device_frames == 59);
  // Backpressure accepts nothing: buffered count is unchanged.
  assert(engine.enqueue(make_block(44100, 2, 60)) == AudioEnqueueResult::Backpressure);
  assert(engine.metrics().buffered_device_frames == 59);
  assert(engine.metrics().source_frames_accepted == 60);
  // A single block larger than total capacity is rejected outright.
  assert(engine.enqueue(make_block(44100, 2, 102)) == AudioEnqueueResult::BlockTooLarge);
  assert(engine.metrics().buffered_device_frames == 59);
  assert(engine.enqueue(make_block(44100, 2, 40)) == AudioEnqueueResult::Accepted);
  assert(engine.metrics().buffered_device_frames == 99);
  engine.close();
}

// Equal rates hold the final source frame back for the next block; totals
// still reconcile because the drain flushes the converter tail.
void equal_rate_tail_reconciles_at_drain() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(44100, 2, 60)) == AudioEnqueueResult::Accepted);
  // 59 converted immediately; the 60th waits for a successor sample.
  assert(engine.metrics().buffered_device_frames == 59);
  assert(engine.start());
  assert(engine.request_drain());
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Completed);
  const auto metrics = engine.metrics();
  assert(metrics.source_frames_accepted == 60);
  assert(metrics.device_frames_submitted == 60);
  assert(metrics.device_frames_processed == 60);
  engine.close();
}

// Rejected enqueue leaves converter state untouched: feed A, force
// Backpressure on B, free capacity, retry B, continue with C. The captured
// device stream must equal a direct conversion where B was never rejected.
void backpressure_preserves_streaming_state() {
  constexpr int kRate = 48000;
  std::vector<float> a_pcm(200, 0.1F);
  std::vector<float> b_pcm(200, 0.2F);
  std::vector<float> c_pcm(200, 0.3F);
  const AudioDeviceFormat device{kRate, 1, AudioSampleFormat::Float32};

  StreamingAudioConverter reference{kRate, 1, device};
  AudioBlock ra{kRate, 1, 200, a_pcm, Rational{1, kRate}, std::nullopt};
  AudioBlock rb{kRate, 1, 200, b_pcm, Rational{1, kRate}, std::nullopt};
  AudioBlock rc{kRate, 1, 200, c_pcm, Rational{1, kRate}, std::nullopt};
  std::vector<std::uint8_t> expected;
  for (const AudioBlock* part : {&ra, &rb, &rc}) {
    const auto out = reference.convert(*part);
    expected.insert(expected.end(), out.frames.begin(), out.frames.end());
  }
  const auto ref_tail = reference.flush_tail();
  expected.insert(expected.end(), ref_tail.frames.begin(), ref_tail.frames.end());
  // 199 + 200 + 200 converted plus 1 tail frame for 600 source frames.
  assert(expected.size() == 600 * 4);

  FakeBackend backend;
  backend.offered = device;
  AudioOutputEngine engine{backend, AudioEngineLimits{250}};
  assert(engine.open({kRate, 1}));
  assert(engine.enqueue(ra) == AudioEnqueueResult::Accepted);
  assert(engine.metrics().buffered_device_frames == 199);
  // B converts to 200 frames (catching up the held sample): 51 free slots
  // reject it, and the rejection must not advance converter state.
  assert(engine.enqueue(rb) == AudioEnqueueResult::Backpressure);
  assert(engine.metrics().buffered_device_frames == 199);
  assert(engine.start());
  assert(engine.transfer_to_backend());
  assert(engine.enqueue(rb) == AudioEnqueueResult::Accepted);
  assert(engine.transfer_to_backend());
  assert(engine.enqueue(rc) == AudioEnqueueResult::Accepted);
  assert(engine.request_drain());
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Completed);
  assert(backend.written == 600);
  assert(backend.captured == expected);
  engine.close();
}

void start_requires_prefbuffer() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(!engine.start());
  assert(engine.open({44100, 2}));
  assert(!engine.start());
  assert(engine.state() == AudioOutputState::Opened);
  assert(engine.metrics().underrun_count == 0);
  assert(engine.enqueue(make_block(44100, 2, 8)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.state() == AudioOutputState::Running);
  assert(!engine.start());
  assert(backend.start_count == 1);
  engine.close();
}

void drain_completes_without_underrun() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(44100, 2, 100)) == AudioEnqueueResult::Accepted);
  assert(!engine.request_drain());
  assert(engine.start());
  assert(engine.request_drain());
  assert(engine.state() == AudioOutputState::Draining);
  assert(engine.enqueue(make_block(44100, 2, 8)) == AudioEnqueueResult::InvalidState);
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Completed);
  const auto metrics = engine.metrics();
  assert(metrics.source_frames_accepted == 100);
  assert(metrics.device_frames_submitted == 100);
  assert(metrics.device_frames_processed == 100);
  assert(metrics.buffered_device_frames == 0);
  assert(metrics.underrun_count == 0);
  assert(!engine.transfer_to_backend());
  engine.close();
}

// Rounding-short progress with audio still backend-buffered must NOT
// complete; only true exhaustion completes. No accepted tail is dropped.
void drain_requires_true_exhaustion() {
  FakeBackend backend;
  backend.auto_process = false;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(44100, 2, 100)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.request_drain());
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Draining);
  // One frame still backend-buffered: processed is a rounding delta short.
  backend.processed = 99;
  backend.pending = 1;
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Draining);
  backend.pending = 0;
  backend.processed = 100;
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Completed);
  assert(engine.metrics().device_frames_submitted == 100);
  assert(engine.metrics().device_frames_processed == 100);
  engine.close();
}

void starvation_counts_underrun_episodes() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(44100, 2, 8)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.transfer_to_backend());
  assert(engine.metrics().underrun_count == 0);
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Running);
  assert(engine.metrics().underrun_count == 1);
  assert(engine.transfer_to_backend());
  assert(engine.metrics().underrun_count == 1);
  assert(engine.enqueue(make_block(44100, 2, 8)) == AudioEnqueueResult::Accepted);
  assert(engine.transfer_to_backend());
  assert(engine.metrics().underrun_count == 1);
  assert(engine.transfer_to_backend());
  assert(engine.metrics().underrun_count == 2);
  engine.close();
}

void completion_waits_for_backend() {
  FakeBackend backend;
  backend.auto_process = false;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(44100, 2, 64)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.request_drain());
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Draining);
  backend.consume(64);
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Completed);
  engine.close();
}

void flush_discards_and_keeps_session() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(44100, 2, 64)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.transfer_to_backend());
  assert(engine.metrics().device_frames_processed == 63);
  assert(engine.flush());
  assert(engine.state() == AudioOutputState::Running);
  // New epoch: no progress counter survives the flush.
  assert(engine.metrics().buffered_device_frames == 0);
  assert(engine.metrics().source_frames_accepted == 0);
  assert(engine.metrics().device_frames_submitted == 0);
  assert(engine.metrics().device_frames_processed == 0);
  assert(backend.reset_count == 1);
  // New data after flush plays from a clean session point.
  assert(engine.enqueue(make_block(44100, 2, 16, 0.75F)) == AudioEnqueueResult::Accepted);
  assert(engine.transfer_to_backend());
  assert(engine.metrics().source_frames_accepted == 16);
  assert(engine.metrics().device_frames_submitted == 15);
  assert(engine.metrics().device_frames_processed == 15);
  engine.close();
  assert(!engine.flush());
}

void stop_lands_reusable_opened() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(!engine.stop());
  assert(engine.open({44100, 2}));
  assert(engine.stop());
  assert(engine.enqueue(make_block(44100, 2, 32)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.transfer_to_backend());
  assert(engine.metrics().device_frames_processed == 31);
  assert(engine.stop());
  assert(engine.state() == AudioOutputState::Opened);
  assert(engine.metrics().buffered_device_frames == 0);
  assert(engine.metrics().source_frames_accepted == 0);
  assert(engine.metrics().device_frames_processed == 0);
  // stop() from Opened, after transfer, completion auto-stop, from Completed.
  assert(backend.stop_count == 2);
  // Replay without close/open.
  assert(engine.enqueue(make_block(44100, 2, 32)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.request_drain());
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Completed);
  assert(engine.stop());
  assert(engine.state() == AudioOutputState::Opened);
  // Replay added the completion auto-stop plus the final stop from Completed.
  assert(backend.stop_count == 4);
  engine.close();
}

void close_opens_fresh_epoch() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(44100, 2, 32)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.transfer_to_backend());
  assert(engine.metrics().device_frames_processed == 31);
  engine.close();
  assert(engine.open({44100, 2}));
  assert(engine.metrics().device_frames_processed == 0);
  assert(engine.metrics().underrun_count == 0);
  engine.close();
}

void backend_failure_latches_and_recovers() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(44100, 2, 32)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  backend.injected_error = "device disappeared";
  assert(engine.transfer_to_backend() == false);
  assert(engine.state() == AudioOutputState::Error);
  assert(engine.error()->code == application::AudioOutputError::Code::BackendFailure);
  assert(engine.enqueue(make_block(44100, 2, 8)) == AudioEnqueueResult::InvalidState);
  assert(!engine.stop());
  // Recovery requires close/open.
  engine.close();
  assert(engine.state() == AudioOutputState::Closed);
  assert(!engine.error().has_value());
  backend.injected_error.reset();
  assert(engine.open({44100, 2}));
  assert(engine.state() == AudioOutputState::Opened);
  engine.close();
}

void processed_progress_is_monotonic_and_bounded() {
  FakeBackend backend;
  backend.auto_process = false;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(44100, 2, 100)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.transfer_to_backend());
  assert(engine.metrics().device_frames_submitted == 99);
  // Backend rounding moving backward must not regress the metric.
  backend.processed = 40;
  assert(engine.transfer_to_backend());
  assert(engine.metrics().device_frames_processed == 40);
  backend.processed = 10;
  assert(engine.transfer_to_backend());
  assert(engine.metrics().device_frames_processed == 40);
  // Backend claiming more than submitted is clamped, never trusted.
  backend.processed = 1000000;
  assert(engine.transfer_to_backend());
  assert(engine.metrics().device_frames_processed == 99);
  engine.close();
}

void incremental_consumption_after_atomic_accept() {
  FakeBackend backend;
  backend.writable_cap = 10;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  assert(engine.enqueue(make_block(44100, 1, 25, 0.5F)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.transfer_to_backend());
  assert(backend.written == 10);
  assert(engine.metrics().buffered_device_frames == 14);
  backend.writable_cap = 1000;
  assert(engine.transfer_to_backend());
  assert(backend.written == 24);
  assert(engine.metrics().buffered_device_frames == 0);
  engine.close();
}

// B6: while a transfer blocks inside the backend and a foreign stop waits
// in its own backend call, the engine mutex must stay free: concurrent
// state queries proceed, and stop completes once the gate opens. Watchdogs
// bound every wait so a locking regression fails instead of hanging CI.
void foreign_lifecycle_during_blocked_transfer() {
  FakeBackend backend;
  backend.block_writes = true;
  backend.block_stops = true;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(44100, 2, 64)) == AudioEnqueueResult::Accepted);
  assert(engine.start());

  std::thread transfer([&] { engine.transfer_to_backend(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  bool stop_result = false;
  std::thread worker([&] { stop_result = engine.stop(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // Both threads are parked inside backend calls; the mutex must be free.
  auto probe = std::async(std::launch::async, [&] { return engine.state(); });
  assert(probe.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
  assert(probe.get() == AudioOutputState::Running);
  backend.release_gate();
  transfer.join();
  worker.join();
  assert(stop_result);
  assert(engine.state() == AudioOutputState::Opened);
  engine.close();
}

}  // namespace

int main() {
  converter_passthrough_and_quantization();
  streaming_split_matches_contiguous();
  converter_rejects_unguessable_layouts();
  lifecycle_open_close_reopen();
  open_rejects_bad_source_and_layouts();
  enqueue_contract_atomicity();
  equal_rate_tail_reconciles_at_drain();
  backpressure_preserves_streaming_state();
  start_requires_prefbuffer();
  drain_completes_without_underrun();
  drain_requires_true_exhaustion();
  starvation_counts_underrun_episodes();
  completion_waits_for_backend();
  flush_discards_and_keeps_session();
  stop_lands_reusable_opened();
  close_opens_fresh_epoch();
  backend_failure_latches_and_recovers();
  processed_progress_is_monotonic_and_bounded();
  incremental_consumption_after_atomic_accept();
  foreign_lifecycle_during_blocked_transfer();
}
