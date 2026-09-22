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
  // Gateable lifecycle backend calls (race/overlap regressions). A gated
  // call signals entered_<op>, then blocks until release_gate().
  bool gate_query{false};
  bool gate_open{false};
  bool gate_start{false};
  bool gate_stop{false};
  bool gate_reset{false};
  bool gate_close{false};
  bool gate_write{false};
  bool entered_query{false};
  bool entered_open{false};
  bool entered_start{false};
  bool entered_stop{false};
  bool entered_reset{false};
  bool entered_close{false};
  std::optional<std::string> injected_error;
  std::size_t frame_bytes_{4};
  int query_count{0};
  int open_count{0};
  int close_count{0};
  int start_count{0};
  int stop_count{0};
  int reset_count{0};
  int lifecycle_active{0};
  int lifecycle_max_concurrent{0};
  AudioDeviceFormat opened_format{0, 0, AudioSampleFormat::Float32};

  void release_gate() {
    {
      std::lock_guard lock{gate_mutex_};
      gate_query = gate_open = gate_start = gate_stop = gate_reset = gate_close = gate_write =
          false;
    }
    gate_cv_.notify_all();
  }

  bool wait_entered(bool FakeBackend::*flag, int timeout_seconds = 5) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
    std::unique_lock lock{gate_mutex_};
    return gate_cv_.wait_until(lock, deadline,
                               [this, flag] { return this->*flag; });
  }

  std::optional<AudioDeviceFormat> query_device_format(const AudioSourceFormat&) override {
    LifecycleScope scope{*this};
    gate_point(gate_query, entered_query);
    ++query_count;
    return offered;
  }
  bool open_device(const AudioDeviceFormat& format) override {
    LifecycleScope scope{*this};
    gate_point(gate_open, entered_open);
    if (!open_ok) {
      return false;
    }
    opened_format = format;
    frame_bytes_ = static_cast<std::size_t>(format.channel_count) *
                   application::device_sample_bytes(format.sample_format);
    ++open_count;
    return true;
  }
  bool fail_close{false};

  bool close_device() override {
    LifecycleScope scope{*this};
    gate_point(gate_close, entered_close);
    if (fail_close) {
      return false;
    }
    ++close_count;
    return true;
  }
  bool start_device() override {
    LifecycleScope scope{*this};
    gate_point(gate_start, entered_start);
    if (!start_ok) {
      return false;
    }
    ++start_count;
    return true;
  }
  bool fail_stop{false};
  bool fail_reset{false};

  bool stop_device() override {
    LifecycleScope scope{*this};
    gate_point(gate_stop, entered_stop);
    if (fail_stop) {
      return false;
    }
    ++stop_count;
    return true;
  }
  bool reset_device() override {
    LifecycleScope scope{*this};
    gate_point(gate_reset, entered_reset);
    if (fail_reset) {
      return false;
    }
    ++reset_count;
    pending = 0;
    processed = written;
    return true;
  }
  std::size_t writable_frames() override { return writable_cap; }
  std::size_t write_frames(const void* frames, std::size_t frame_count) override {
    gate_point(gate_write, entered_write);
    // Optional per-call script: each entry caps frames taken that call.
    std::size_t allowed = frame_count;
    if (!write_plan.empty()) {
      allowed = write_plan.front() > frame_count ? frame_count : write_plan.front();
      write_plan.erase(write_plan.begin());
    }
    const std::size_t taken = allowed > writable_cap ? writable_cap : allowed;
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

  std::vector<std::size_t> write_plan;
  bool entered_write{false};

private:
  struct LifecycleScope {
    explicit LifecycleScope(FakeBackend& owner) : owner_{owner} {
      std::lock_guard lock{owner_.track_mutex_};
      ++owner_.lifecycle_active;
      if (owner_.lifecycle_active > owner_.lifecycle_max_concurrent) {
        owner_.lifecycle_max_concurrent = owner_.lifecycle_active;
      }
    }
    ~LifecycleScope() {
      std::lock_guard lock{owner_.track_mutex_};
      --owner_.lifecycle_active;
    }
    FakeBackend& owner_;
  };

  void gate_point(bool& gate_flag, bool& entered_flag) {
    std::unique_lock lock{gate_mutex_};
    if (gate_flag) {
      entered_flag = true;
      gate_cv_.notify_all();
      gate_cv_.wait(lock, [&] { return !gate_flag; });
    }
  }

  std::mutex gate_mutex_;
  std::condition_variable gate_cv_;
  std::mutex track_mutex_;
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
  assert(engine.close());
  assert(engine.state() == AudioOutputState::Closed);
  assert(!engine.error().has_value());
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  assert(engine.open({44100, 2}));
  assert(engine.state() == AudioOutputState::Opened);
  assert(engine.device_format().has_value());
  assert(engine.device_format()->sample_rate == 44100);
  assert(!engine.open({44100, 2}));
  assert(engine.state() == AudioOutputState::Opened);
  assert(engine.close());
  assert(engine.state() == AudioOutputState::Closed);
  assert(!engine.device_format().has_value());
  assert(!engine.error().has_value());
  assert(engine.open({44100, 2}));
  assert(backend.open_count == 2);
  assert(engine.close());
}

void open_rejects_bad_source_and_layouts() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{48000, 2, AudioSampleFormat::Int16};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(!engine.open({0, 2}));
  assert(engine.state() == AudioOutputState::Error);
  assert(engine.error()->code == application::AudioOutputError::Code::UnsupportedConversion);
  assert(engine.close());
  backend.offered = AudioDeviceFormat{48000, 2, AudioSampleFormat::Int16};
  assert(!engine.open({44100, 5}));
  assert(engine.state() == AudioOutputState::Error);
  assert(engine.error()->code == application::AudioOutputError::Code::UnsupportedConversion);
  assert(engine.close());
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
  assert(engine.close());
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
  assert(engine.close());
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
  assert(engine.close());
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
  assert(engine.close());
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
  assert(engine.close());
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
  assert(engine.close());
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
  assert(engine.close());
}

void backend_buffer_does_not_count_as_underrun() {
  FakeBackend backend;
  backend.auto_process = false;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 2}));
  assert(engine.enqueue(make_block(44100, 2, 64)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.transfer_to_backend());
  backend.pending = 64;
  // The adapter queue is empty, but the simulated device still holds audio.
  assert(engine.transfer_to_backend());
  assert(engine.metrics().underrun_count == 0);
  backend.consume(64);
  assert(engine.transfer_to_backend());
  assert(engine.metrics().underrun_count == 1);
  assert(engine.close());
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
  assert(engine.close());
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
  assert(engine.close());
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
  assert(engine.close());
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
  assert(engine.close());
  assert(engine.open({44100, 2}));
  assert(engine.metrics().device_frames_processed == 0);
  assert(engine.metrics().underrun_count == 0);
  assert(engine.close());
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
  assert(engine.close());
  assert(engine.state() == AudioOutputState::Closed);
  assert(!engine.error().has_value());
  backend.injected_error.reset();
  assert(engine.open({44100, 2}));
  assert(engine.state() == AudioOutputState::Opened);
  assert(engine.close());
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
  assert(engine.close());
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
  assert(engine.close());
}

// B6: while a transfer blocks inside the backend and a foreign stop waits
// in its own backend call, the engine mutex must stay free: concurrent
// state queries proceed, and stop completes once the gate opens. Watchdogs
// bound every wait so a locking regression fails instead of hanging CI.
void foreign_lifecycle_during_blocked_transfer() {
  FakeBackend backend;
  backend.gate_write = true;
  backend.gate_stop = true;
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
  assert(engine.close());
}

// W1: a zero backend write changes nothing; later acceptance byte-matches.
void zero_write_preserves_queue() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  const AudioBlock block = make_block(44100, 1, 64, 0.5F);
  assert(engine.enqueue(block) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  backend.write_plan = {0};
  assert(engine.transfer_to_backend());
  assert(engine.metrics().buffered_device_frames == 63);
  assert(engine.metrics().device_frames_submitted == 0);
  assert(backend.captured.empty());
  assert(engine.transfer_to_backend());
  assert(engine.metrics().device_frames_submitted == 63);
  assert(engine.metrics().buffered_device_frames == 0);

  StreamingAudioConverter reference{44100, 1, AudioDeviceFormat{44100, 1,
                                                                AudioSampleFormat::Float32}};
  const auto expected = reference.convert(block);
  assert(backend.captured.size() == expected.frames.size());
  assert(backend.captured == expected.frames);
  assert(engine.close());
}

// W2: scripted short writes (3/0/7/1/rest) emit the exact byte stream.
void short_writes_preserve_exact_bytes() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  const AudioBlock block = make_block(44100, 1, 64, 0.25F);
  assert(engine.enqueue(block) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  backend.write_plan = {3, 0, 7, 1};
  for (int i = 0; i < 5; ++i) {
    assert(engine.transfer_to_backend());
  }
  assert(engine.metrics().device_frames_submitted == 63);
  assert(engine.metrics().buffered_device_frames == 0);

  StreamingAudioConverter reference{44100, 1, AudioDeviceFormat{44100, 1,
                                                                AudioSampleFormat::Float32}};
  const auto expected = reference.convert(block);
  assert(backend.captured == expected.frames);
  assert(engine.close());
}

// W3: partial consumption across head/reset cycles preserves exact data.
void partial_writes_across_reset_boundary() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  const AudioBlock first = make_block(44100, 1, 100, 0.1F);
  const AudioBlock second = make_block(44100, 1, 100, 0.2F);
  assert(engine.enqueue(first) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  backend.writable_cap = 30;
  assert(engine.transfer_to_backend());
  assert(engine.metrics().buffered_device_frames == 69);
  // Head sits mid-vector; new data appends behind it.
  assert(engine.enqueue(second) == AudioEnqueueResult::Accepted);
  backend.writable_cap = 100000;
  assert(engine.transfer_to_backend());
  assert(engine.transfer_to_backend());
  assert(engine.metrics().buffered_device_frames == 0);
  assert(engine.request_drain());
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Completed);
  assert(backend.written == 200);

  StreamingAudioConverter reference{44100, 1, AudioDeviceFormat{44100, 1,
                                                                AudioSampleFormat::Float32}};
  std::vector<std::uint8_t> expected;
  for (const AudioBlock* part : {&first, &second}) {
    const auto out = reference.convert(*part);
    expected.insert(expected.end(), out.frames.begin(), out.frames.end());
  }
  const auto tail = reference.flush_tail();
  expected.insert(expected.end(), tail.frames.begin(), tail.frames.end());
  assert(backend.captured == expected);
  assert(engine.close());
}

// B2: a short valid stream completes with processed == 0 once the backend
// is exhausted. Metrics never gate completion.
void short_stream_completes_with_zero_processed() {
  FakeBackend backend;
  backend.auto_process = false;
  backend.offered = AudioDeviceFormat{8000, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({8000, 1}));
  assert(engine.enqueue(make_block(8000, 1, 5)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.request_drain());
  assert(engine.transfer_to_backend());
  assert(engine.metrics().device_frames_submitted == 5);
  // Backend consumed everything but its progress counter rounds to zero.
  backend.pending = 0;
  backend.processed = 0;
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Completed);
  assert(engine.metrics().underrun_count == 0);
  assert(engine.close());
}

// B3: enqueue during a destructive transition is rejected, never accepted
// into a session being destroyed. Gated per op: flush, stop, close.
void enqueue_rejected_during_flush_transition() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  assert(engine.enqueue(make_block(44100, 1, 32)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  backend.gate_reset = true;
  bool flush_result = false;
  std::thread worker([&] { flush_result = engine.flush(); });
  assert(backend.wait_entered(&FakeBackend::entered_reset));
  assert(engine.enqueue(make_block(44100, 1, 8)) != AudioEnqueueResult::Accepted);
  assert(engine.metrics().source_frames_accepted == 32);
  backend.release_gate();
  worker.join();
  assert(flush_result);
  assert(engine.state() == AudioOutputState::Running);
  assert(engine.enqueue(make_block(44100, 1, 8)) == AudioEnqueueResult::Accepted);
  assert(engine.close());
}

void enqueue_rejected_during_stop_transition() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  assert(engine.enqueue(make_block(44100, 1, 32)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  backend.gate_stop = true;
  bool stop_result = false;
  std::thread worker([&] { stop_result = engine.stop(); });
  assert(backend.wait_entered(&FakeBackend::entered_stop));
  assert(engine.enqueue(make_block(44100, 1, 8)) != AudioEnqueueResult::Accepted);
  backend.release_gate();
  worker.join();
  assert(stop_result);
  assert(engine.state() == AudioOutputState::Opened);
  assert(engine.enqueue(make_block(44100, 1, 8)) == AudioEnqueueResult::Accepted);
  assert(engine.close());
}

void enqueue_rejected_during_close_transition() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  assert(engine.enqueue(make_block(44100, 1, 32)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  backend.gate_close = true;
  std::thread worker([&] { assert(engine.close()); });
  assert(backend.wait_entered(&FakeBackend::entered_close));
  assert(engine.enqueue(make_block(44100, 1, 8)) != AudioEnqueueResult::Accepted);
  backend.release_gate();
  worker.join();
  assert(engine.state() == AudioOutputState::Closed);
  assert(engine.enqueue(make_block(44100, 1, 8)) == AudioEnqueueResult::InvalidState);
}

// B4: overlapping lifecycle calls never enter the backend concurrently.
// L1: second open fails fast while the first is inside the backend.
void overlapping_open_rejected() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  backend.gate_query = true;
  bool first_result = false;
  std::thread first([&] { first_result = engine.open({44100, 1}); });
  assert(backend.wait_entered(&FakeBackend::entered_query));
  auto second =
      std::async(std::launch::async, [&] { return engine.open({44100, 1}); });
  assert(second.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
  assert(!second.get());
  backend.release_gate();
  first.join();
  assert(first_result);
  assert(backend.query_count == 1);
  assert(engine.state() == AudioOutputState::Opened);
  assert(backend.lifecycle_max_concurrent == 1);
  assert(engine.close());
}

// L2: stop during a gated flush fails fast without touching the backend.
void overlapping_flush_stop_rejected() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  assert(engine.enqueue(make_block(44100, 1, 32)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  backend.gate_reset = true;
  bool flush_result = false;
  std::thread flushing([&] { flush_result = engine.flush(); });
  assert(backend.wait_entered(&FakeBackend::entered_reset));
  auto stopping = std::async(std::launch::async, [&] { return engine.stop(); });
  assert(stopping.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
  assert(!stopping.get());
  assert(backend.stop_count == 0);
  backend.release_gate();
  flushing.join();
  assert(flush_result);
  assert(engine.state() == AudioOutputState::Running);
  assert(backend.lifecycle_max_concurrent == 1);
  assert(engine.close());
}

// Owner cycle: a foreign open blocks inside the backend until close() has
// returned (models backend work gated on owner execution). Fail-fast close
// returns Busy immediately so the owner stays free; a waiting close would
// deadlock here and hit the watchdog instead.
void close_during_blocked_open_returns_busy() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  backend.gate_query = true;
  bool open_result = false;
  std::thread foreign([&] { open_result = engine.open({44100, 1}); });
  assert(backend.wait_entered(&FakeBackend::entered_query));
  auto closing = std::async(std::launch::async, [&] { return engine.close(); });
  assert(closing.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
  assert(!closing.get());
  assert(engine.state() == AudioOutputState::Closed);
  backend.release_gate();
  foreign.join();
  assert(open_result);
  assert(engine.state() == AudioOutputState::Opened);
  assert(engine.close());
  assert(engine.state() == AudioOutputState::Closed);
  assert(backend.lifecycle_max_concurrent == 1);
}

// L3: close fails fast (Busy) while a gated start is in flight; backend
// calls never overlap; after release both complete coherently.
void overlapping_start_close_serialized() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  assert(engine.enqueue(make_block(44100, 1, 32)) == AudioEnqueueResult::Accepted);
  backend.gate_start = true;
  bool start_result = false;
  std::thread starting([&] { start_result = engine.start(); });
  assert(backend.wait_entered(&FakeBackend::entered_start));
  auto closing = std::async(std::launch::async, [&] { return engine.close(); });
  assert(closing.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
  assert(!closing.get());
  assert(backend.close_count == 0);
  assert(engine.state() == AudioOutputState::Opened);
  backend.release_gate();
  starting.join();
  assert(start_result);
  assert(engine.state() == AudioOutputState::Running);
  assert(engine.close());
  assert(engine.state() == AudioOutputState::Closed);
  assert(backend.close_count == 1);
  assert(backend.lifecycle_max_concurrent == 1);
}

// B4: a failed backend reset drives Error, never Running-with-dead-backend.
void reset_failure_drives_error() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  assert(engine.enqueue(make_block(44100, 1, 32)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  backend.fail_reset = true;
  assert(!engine.flush());
  assert(engine.state() == AudioOutputState::Error);
  assert(engine.error()->code == application::AudioOutputError::Code::BackendFailure);
  assert(engine.enqueue(make_block(44100, 1, 8)) == AudioEnqueueResult::InvalidState);
  // Recovery requires close/reopen.
  assert(engine.close());
  assert(engine.state() == AudioOutputState::Closed);
  assert(engine.open({44100, 1}));
  assert(engine.state() == AudioOutputState::Opened);
  assert(engine.close());
}

void stop_failure_drives_error() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  assert(engine.enqueue(make_block(44100, 1, 32)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  backend.fail_stop = true;
  assert(!engine.stop());
  assert(engine.state() == AudioOutputState::Error);
  assert(engine.error()->code == application::AudioOutputError::Code::BackendFailure);
  assert(engine.close());
  assert(engine.state() == AudioOutputState::Closed);
}

void close_failure_never_pretends_closed() {
  FakeBackend backend;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  backend.fail_close = true;
  assert(!engine.close());
  assert(engine.state() == AudioOutputState::Error);
  assert(engine.error()->code == application::AudioOutputError::Code::BackendFailure);
  backend.fail_close = false;
  assert(engine.close());
  assert(engine.state() == AudioOutputState::Closed);
}

// B1: enqueue while a transfer is blocked inside the backend must neither
// invalidate backend write memory nor corrupt the stream. Runs under
// ASan/UBSan: a queue view handed out unlocked is heap-use-after-free once
// the append reallocates.
void concurrent_enqueue_during_blocked_write() {
  FakeBackend backend;
  backend.gate_write = true;
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{65536}};
  assert(engine.open({44100, 2}));
  const AudioBlock first = make_block(44100, 2, 32, 0.1F);
  const AudioBlock second = make_block(44100, 2, 4096, 0.2F);
  assert(engine.enqueue(first) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  std::thread transfer([&] { engine.transfer_to_backend(); });
  assert(backend.wait_entered(&FakeBackend::entered_write));
  // Large append forces queue reallocation while the backend is blocked.
  assert(engine.enqueue(second) == AudioEnqueueResult::Accepted);
  backend.release_gate();
  transfer.join();
  assert(engine.transfer_to_backend());
  assert(engine.transfer_to_backend());

  StreamingAudioConverter reference{44100, 2, AudioDeviceFormat{44100, 2,
                                                                AudioSampleFormat::Float32}};
  std::vector<std::uint8_t> expected;
  for (const AudioBlock* part : {&first, &second}) {
    const auto out = reference.convert(*part);
    expected.insert(expected.end(), out.frames.begin(), out.frames.end());
  }
  assert(backend.captured == expected);
  assert(engine.metrics().device_frames_submitted == 31 + 4096);
  assert(engine.close());
}

// B1: a destructive transition during a blocked transfer must not corrupt
// the replacement queue; the stale commit is dropped by epoch.
void destructive_transition_during_blocked_write() {
  FakeBackend backend;
  backend.gate_write = true;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  const AudioBlock first = make_block(44100, 1, 64, 0.1F);
  assert(engine.enqueue(first) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  std::thread transfer([&] { engine.transfer_to_backend(); });
  assert(backend.wait_entered(&FakeBackend::entered_write));
  assert(engine.flush());
  assert(engine.state() == AudioOutputState::Running);
  backend.release_gate();
  transfer.join();
  // Stale attempt committed nothing: the session restarts clean.
  assert(engine.metrics().device_frames_submitted == 0);
  const AudioBlock second = make_block(44100, 1, 32, 0.7F);
  assert(engine.enqueue(second) == AudioEnqueueResult::Accepted);
  assert(engine.transfer_to_backend());

  StreamingAudioConverter reference{44100, 1, AudioDeviceFormat{44100, 1,
                                                                AudioSampleFormat::Float32}};
  const auto expected = reference.convert(second);
  assert(backend.captured.size() >= expected.frames.size());
  std::vector<std::uint8_t> tail(backend.captured.end() -
                                     static_cast<std::ptrdiff_t>(expected.frames.size()),
                                 backend.captured.end());
  assert(tail == expected.frames);
  assert(engine.metrics().device_frames_submitted == 31);
  assert(engine.close());
}

// The transfer snapshot must retain the old device frame size while a close
// and reopen replace the session during the backend write. The old completion
// is stale and must not account against the reopened queue; the replacement
// format must still transfer normally afterwards.
void format_snapshot_survives_replacement_during_write() {
  FakeBackend backend;
  backend.gate_write = true;
  backend.offered = AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32};
  AudioOutputEngine engine{backend, AudioEngineLimits{4096}};
  assert(engine.open({44100, 1}));
  assert(engine.enqueue(make_block(44100, 1, 64, 0.1F)) == AudioEnqueueResult::Accepted);
  assert(engine.start());

  std::thread transfer([&] { assert(engine.transfer_to_backend()); });
  assert(backend.wait_entered(&FakeBackend::entered_write));

  assert(engine.close());
  backend.offered = AudioDeviceFormat{44100, 2, AudioSampleFormat::Int16};
  assert(engine.open({44100, 1}));
  assert(engine.metrics().device_frames_submitted == 0);

  backend.release_gate();
  transfer.join();
  assert(engine.state() == AudioOutputState::Opened);
  assert(engine.metrics().device_frames_submitted == 0);

  assert(engine.enqueue(make_block(44100, 1, 32, 0.7F)) == AudioEnqueueResult::Accepted);
  assert(engine.start());
  assert(engine.transfer_to_backend());
  assert(engine.metrics().device_frames_submitted == 31);
  assert(engine.close());
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
  backend_buffer_does_not_count_as_underrun();
  completion_waits_for_backend();
  flush_discards_and_keeps_session();
  stop_lands_reusable_opened();
  close_opens_fresh_epoch();
  backend_failure_latches_and_recovers();
  processed_progress_is_monotonic_and_bounded();
  incremental_consumption_after_atomic_accept();
  foreign_lifecycle_during_blocked_transfer();
  zero_write_preserves_queue();
  short_writes_preserve_exact_bytes();
  partial_writes_across_reset_boundary();
  short_stream_completes_with_zero_processed();
  enqueue_rejected_during_flush_transition();
  enqueue_rejected_during_stop_transition();
  enqueue_rejected_during_close_transition();
  overlapping_open_rejected();
  overlapping_flush_stop_rejected();
  overlapping_start_close_serialized();
  close_during_blocked_open_returns_busy();
  reset_failure_drives_error();
  stop_failure_drives_error();
  close_failure_never_pretends_closed();
  concurrent_enqueue_during_blocked_write();
  destructive_transition_during_blocked_write();
  format_snapshot_survives_replacement_during_write();
}
