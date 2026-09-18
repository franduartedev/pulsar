// A2 deterministic engine tests: lifecycle, queue, conversion, metrics.
// No Qt, no threads, no device. A FakeBackend stands in for QAudioSink.
#include <pulsar/application/audio/audio_converter.hpp>
#include <pulsar/application/audio/audio_output_engine.hpp>
#include <pulsar/application/media/audio_block.hpp>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {
using namespace pulsar;
using application::AudioBlock;
using application::AudioDeviceBackend;
using application::AudioDeviceFormat;
using application::AudioEnqueueResult;
using application::AudioEngineLimits;
using application::AudioOutputEngine;
using application::AudioOutputState;
using application::AudioSampleFormat;
using application::AudioSourceFormat;
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
  bool auto_process{true};
  std::optional<std::string> injected_error;
  int open_count{0};
  int close_count{0};
  int start_count{0};
  int stop_count{0};
  int reset_count{0};
  AudioDeviceFormat opened_format{0, 0, AudioSampleFormat::Float32};

  std::optional<AudioDeviceFormat> query_device_format(const AudioSourceFormat&) override {
    return offered;
  }
  bool open_device(const AudioDeviceFormat& format) override {
    if (!open_ok) {
      return false;
    }
    opened_format = format;
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
  void stop_device() override { ++stop_count; }
  void reset_device() override {
    ++reset_count;
    processed = written;
  }
  std::size_t writable_frames() override { return writable_cap; }
  std::size_t write_frames(const void*, std::size_t frame_count) override {
    const std::size_t taken = frame_count > writable_cap ? writable_cap : frame_count;
    written += taken;
    if (auto_process) {
      processed = written;
    }
    return taken;
  }
  std::uint64_t processed_frames() override { return processed; }
  std::optional<std::string> backend_error() override { return injected_error; }
};

void converter_passthrough_and_quantization() {
  using application::convert_block_for_device;
  const AudioBlock stereo = make_block(44100, 2, 4, 0.5F);
  const auto same = convert_block_for_device(stereo, AudioDeviceFormat{44100, 2,
                                                                       AudioSampleFormat::Float32});
  assert(same.frame_count == 4);
  assert(same.frames.size() == 4 * 2 * 4);
  assert((reinterpret_cast<const float*>(same.frames.data())[0]) == 0.5F);

  const auto as_int16 =
      convert_block_for_device(stereo, AudioDeviceFormat{44100, 2, AudioSampleFormat::Int16});
  assert(as_int16.frames.size() == 4 * 2 * 2);
  const auto* words = reinterpret_cast<const std::int16_t*>(as_int16.frames.data());
  assert(words[0] == 16384);

  AudioBlock clip{44100, 1, 3, std::vector<float>{2.0F, -2.0F, 0.0F}, Rational{1, 44100},
                  std::nullopt};
  const auto clipped =
      convert_block_for_device(clip, AudioDeviceFormat{44100, 1, AudioSampleFormat::Int16});
  const auto* clipped_words = reinterpret_cast<const std::int16_t*>(clipped.frames.data());
  assert(clipped_words[0] == 32767);
  assert(clipped_words[1] == -32768);
  assert(clipped_words[2] == 0);

  const auto as_u8 =
      convert_block_for_device(stereo, AudioDeviceFormat{44100, 2, AudioSampleFormat::UInt8});
  assert(as_u8.frames.size() == 4 * 2);
  assert(as_u8.frames[0] == 191);

  AudioBlock mono{44100, 1, 2, std::vector<float>{0.25F, -0.5F}, Rational{1, 44100}, std::nullopt};
  const auto wide =
      convert_block_for_device(mono, AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32});
  assert(wide.frame_count == 2);
  const auto* wide_floats = reinterpret_cast<const float*>(wide.frames.data());
  assert(wide_floats[0] == 0.25F && wide_floats[1] == 0.25F);
  assert(wide_floats[2] == -0.5F && wide_floats[3] == -0.5F);

  AudioBlock pair{44100, 2, 1, std::vector<float>{1.0F, -1.0F}, Rational{1, 44100}, std::nullopt};
  const auto narrow =
      convert_block_for_device(pair, AudioDeviceFormat{44100, 1, AudioSampleFormat::Float32});
  assert(narrow.frame_count == 1);
  assert((reinterpret_cast<const float*>(narrow.frames.data())[0]) == 0.0F);
}

void converter_resampling() {
  using application::convert_block_for_device;
  std::vector<float> ramp(1024);
  for (std::size_t i = 0; i < ramp.size(); ++i) {
    ramp[i] = static_cast<float>(i) / 1024.0F;
  }
  AudioBlock source{44100, 1, 1024, ramp, Rational{1, 44100}, std::nullopt};
  const auto up =
      convert_block_for_device(source, AudioDeviceFormat{48000, 1, AudioSampleFormat::Float32});
  assert(up.frame_count == 1114);
  assert(up.frames.size() == 1114 * 4);
  const auto* out = reinterpret_cast<const float*>(up.frames.data());
  assert(out[0] == ramp[0]);
  for (std::size_t i = 1; i < 1114; ++i) {
    assert(out[i] >= out[i - 1]);
  }
  assert(out[1113] >= ramp[1021] && out[1113] <= ramp[1023]);
}

void converter_rejects_unguessable_layouts() {
  using application::AudioConversionError;
  using application::convert_block_for_device;
  const AudioBlock three = make_block(44100, 3, 8);
  bool threw = false;
  try {
    (void)convert_block_for_device(three, AudioDeviceFormat{44100, 2, AudioSampleFormat::Float32});
  } catch (const AudioConversionError&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    const AudioBlock stereo = make_block(44100, 2, 8);
    (void)convert_block_for_device(stereo, AudioDeviceFormat{44100, 6, AudioSampleFormat::Float32});
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
  assert(engine.metrics().buffered_device_frames == 60);
  // Backpressure accepts nothing: buffered count is unchanged.
  assert(engine.enqueue(make_block(44100, 2, 60)) == AudioEnqueueResult::Backpressure);
  assert(engine.metrics().buffered_device_frames == 60);
  assert(engine.metrics().source_frames_accepted == 60);
  // A single block larger than total capacity is rejected outright.
  assert(engine.enqueue(make_block(44100, 2, 101)) == AudioEnqueueResult::BlockTooLarge);
  assert(engine.metrics().buffered_device_frames == 60);
  assert(engine.enqueue(make_block(44100, 2, 40)) == AudioEnqueueResult::Accepted);
  assert(engine.metrics().buffered_device_frames == 100);
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
  backend.processed = 64;
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Completed);
  engine.close();
}

void drain_completion_tolerates_backend_rounding() {
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
  // Tolerance is 44100/1000 = 44 device frames: 55 short stays Draining.
  backend.processed = 100 - 44 - 1;
  assert(engine.transfer_to_backend());
  assert(engine.state() == AudioOutputState::Draining);
  // Exactly at the tolerance boundary the drain completes.
  backend.processed = 100 - 44;
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
  assert(engine.flush());
  assert(engine.state() == AudioOutputState::Running);
  assert(engine.metrics().buffered_device_frames == 0);
  assert(engine.metrics().source_frames_accepted == 0);
  assert(engine.metrics().device_frames_submitted == 0);
  assert(backend.reset_count == 1);
  // New data after flush plays from a clean session point.
  assert(engine.enqueue(make_block(44100, 2, 16, 0.75F)) == AudioEnqueueResult::Accepted);
  assert(engine.transfer_to_backend());
  assert(engine.metrics().source_frames_accepted == 16);
  assert(engine.metrics().device_frames_submitted == 16);
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
  assert(engine.stop());
  assert(engine.state() == AudioOutputState::Opened);
  assert(engine.metrics().buffered_device_frames == 0);
  assert(engine.metrics().source_frames_accepted == 0);
  // stop() from Opened, then after transfer.
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
  assert(engine.metrics().device_frames_submitted == 100);
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
  assert(engine.metrics().device_frames_processed == 100);
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
  assert(engine.metrics().buffered_device_frames == 15);
  backend.writable_cap = 1000;
  assert(engine.transfer_to_backend());
  assert(backend.written == 25);
  assert(engine.metrics().buffered_device_frames == 0);
  engine.close();
}

}  // namespace

int main() {
  converter_passthrough_and_quantization();
  converter_resampling();
  converter_rejects_unguessable_layouts();
  lifecycle_open_close_reopen();
  open_rejects_bad_source_and_layouts();
  enqueue_contract_atomicity();
  start_requires_prefbuffer();
  drain_completes_without_underrun();
  starvation_counts_underrun_episodes();
  completion_waits_for_backend();
  drain_completion_tolerates_backend_rounding();
  flush_discards_and_keeps_session();
  stop_lands_reusable_opened();
  backend_failure_latches_and_recovers();
  processed_progress_is_monotonic_and_bounded();
  incremental_consumption_after_atomic_accept();
}
