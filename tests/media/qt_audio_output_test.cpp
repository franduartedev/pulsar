// A2 Qt adapter tests. Deterministic policy lives in the engine tests;
// these prove the real QAudioSink backend wiring. Tests needing a device
// skip cleanly when none exists; the bogus-device test always runs.
#include <pulsar/application/audio/audio_output.hpp>
#include <pulsar/application/media/audio_block.hpp>
#include <pulsar/media/qt_audio/qt_audio_output.hpp>

#include <QAudioDevice>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMediaDevices>

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace {
using namespace pulsar;
using application::AudioBlock;
using application::AudioEnqueueResult;
using application::AudioOutputState;
using domain::Rational;
using media::qt_audio::QtAudioOutput;

constexpr double kPi = 3.141592653589793;

AudioBlock sine_block(int sample_rate, int channels, std::size_t frames, double frequency) {
  std::vector<float> samples(frames * static_cast<std::size_t>(channels));
  for (std::size_t i = 0; i < frames; ++i) {
    const float value = static_cast<float>(0.5 * std::sin(2.0 * kPi * frequency *
                                                          static_cast<double>(i) /
                                                          static_cast<double>(sample_rate)));
    for (int c = 0; c < channels; ++c) {
      samples[i * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c)] = value;
    }
  }
  return AudioBlock{sample_rate, channels, frames, std::move(samples),
                    Rational{1, sample_rate}, std::nullopt};
}

bool poll_until(const std::function<bool()>& condition, int timeout_ms) {
  QElapsedTimer timer;
  timer.start();
  while (!condition()) {
    if (timer.elapsed() > timeout_ms) {
      return false;
    }
    QCoreApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return true;
}

bool have_device() {
  return !QMediaDevices::audioOutputs().empty();
}

// Always runs: unknown device id fails deterministically into Error.
void bogus_device_open_fails() {
  QtAudioOutput output{QtAudioOutput::Options{{4096}, "no-such-pulsar-device"}};
  assert(output.state() == AudioOutputState::Closed);
  assert(!output.open({44100, 2}));
  assert(output.state() == AudioOutputState::Error);
  assert(output.error().has_value());
  assert(output.error()->code == application::AudioOutputError::Code::NoDevice);
  output.close();
  assert(output.state() == AudioOutputState::Closed);
  assert(!output.error().has_value());
}

void drain_lifecycle_with_worker_producer() {
  constexpr int kRate = 44100;
  constexpr std::size_t kBlockFrames = 4410;
  constexpr int kBlocks = 10;
  QtAudioOutput output{QtAudioOutput::Options{{192000}, {}}};
  assert(output.open({kRate, 1}));
  assert(output.device_format().has_value());
  const auto device = *output.device_format();

  std::thread producer([&] {
    for (int i = 0; i < kBlocks; ++i) {
      const auto block = sine_block(kRate, 1, kBlockFrames, 440.0);
      while (output.enqueue(block) == AudioEnqueueResult::Backpressure) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }
  });
  producer.join();
  assert(output.metrics().source_frames_accepted == kBlocks * kBlockFrames);

  assert(output.start());
  assert(output.request_drain());
  assert(poll_until([&] { return output.state() == AudioOutputState::Completed; }, 20000));
  const auto metrics = output.metrics();
  const std::uint64_t expected_device_frames =
      static_cast<std::uint64_t>(kBlocks) *
      ((static_cast<std::uint64_t>(kBlockFrames) *
        static_cast<std::uint64_t>(device.sample_rate)) /
       static_cast<std::uint64_t>(kRate));
  assert(metrics.device_frames_submitted == expected_device_frames);
  assert(metrics.device_frames_processed == expected_device_frames);
  assert(metrics.buffered_device_frames == 0);
  assert(metrics.underrun_count == 0);
  output.close();
  assert(output.state() == AudioOutputState::Closed);
}

void start_empty_stop_flush_reopen() {
  QtAudioOutput output{QtAudioOutput::Options{{65536}, {}}};
  assert(output.open({44100, 1}));
  assert(!output.start());
  assert(output.state() == AudioOutputState::Opened);
  assert(output.metrics().underrun_count == 0);
  assert(output.enqueue(sine_block(44100, 1, 4410, 440.0)) == AudioEnqueueResult::Accepted);
  assert(output.start());
  assert(poll_until([&] { return output.metrics().device_frames_submitted > 0; }, 10000));
  assert(output.stop());
  assert(output.state() == AudioOutputState::Opened);
  assert(output.metrics().buffered_device_frames == 0);
  assert(output.enqueue(sine_block(44100, 1, 4410, 440.0)) == AudioEnqueueResult::Accepted);
  assert(output.start());
  assert(output.flush());
  assert(output.state() == AudioOutputState::Running);
  assert(output.metrics().buffered_device_frames == 0);
  assert(output.request_drain());
  assert(poll_until([&] { return output.state() == AudioOutputState::Completed; }, 20000));
  output.close();
  assert(output.open({44100, 1}));
  assert(output.state() == AudioOutputState::Opened);
  output.close();
}

void starvation_surfaces_underrun() {
  QtAudioOutput output{QtAudioOutput::Options{{65536}, {}}};
  assert(output.open({44100, 1}));
  assert(output.enqueue(sine_block(44100, 1, 441, 440.0)) == AudioEnqueueResult::Accepted);
  assert(output.start());
  assert(poll_until([&] { return output.metrics().underrun_count >= 1; }, 10000));
  assert(output.state() == AudioOutputState::Running);
  output.close();
}

void multichannel_policy_without_guessing() {
  QtAudioOutput output{QtAudioOutput::Options{{4096}, {}}};
  if (output.open({44100, 6})) {
    // Exact passthrough only: the device took 6 channels as-is, no downmix.
    assert(output.device_format()->channel_count == 6);
    output.close();
    return;
  }
  assert(output.state() == AudioOutputState::Error);
  assert(output.error()->code == application::AudioOutputError::Code::UnsupportedConversion);
  output.close();
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app{argc, argv};
  bogus_device_open_fails();
  if (!have_device()) {
    return 0;
  }
  drain_lifecycle_with_worker_producer();
  start_empty_stop_flush_reopen();
  starvation_surfaces_underrun();
  multichannel_policy_without_guessing();
}
