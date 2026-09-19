// A2 Qt adapter tests. Deterministic policy lives in the engine tests;
// these prove the real QAudioSink backend wiring when explicitly enabled.
// Device-dependent coverage skips before Qt backend enumeration by default.
#include <pulsar/application/audio/audio_output.hpp>
#include <pulsar/application/media/audio_block.hpp>
#include <pulsar/media/qt_audio/qt_audio_output.hpp>

#include <QAudioDevice>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMediaDevices>
#include <QProcess>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <future>
#include <string>
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

// On Linux, QMediaDevices can retain a PulseAudio endpoint after the server
// has disappeared; touching that endpoint can block for a long time. A short
// service probe lets the optional test classify that environment before Qt
// Multimedia enumeration. On platforms without pactl, Qt remains the source
// of truth.
bool host_audio_service_available() {
  QProcess process;
  process.start("pactl", {"info"});
  if (!process.waitForStarted(250)) {
    return true;
  }
  if (!process.waitForFinished(1000)) {
    process.kill();
    process.waitForFinished(250);
    return false;
  }
  return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

// A listed device is not a usable backend (e.g. dead PulseAudio server):
// prove open/close works before running device scenarios, otherwise SKIP.
bool usable_backend() {
  if (!have_device()) {
    return false;
  }
  QtAudioOutput probe{QtAudioOutput::Options{{4096}, {}}};
  if (!probe.open({44100, 1})) {
    return false;
  }
  return probe.close();
}

// Always runs: unknown device id fails deterministically into Error.
void bogus_device_open_fails() {
  QtAudioOutput output{QtAudioOutput::Options{{4096}, "no-such-pulsar-device"}};
  assert(output.state() == AudioOutputState::Closed);
  assert(!output.open({44100, 2}));
  assert(output.state() == AudioOutputState::Error);
  assert(output.error().has_value());
  assert(output.error()->code == application::AudioOutputError::Code::NoDevice);
  assert(output.close());
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
  // Backend progress never exceeds submissions; exact equality is backend
  // rounding luck, not a contract (completion uses exhaustion, not equality).
  assert(metrics.device_frames_processed <= expected_device_frames);
  assert(metrics.buffered_device_frames == 0);
  assert(metrics.underrun_count == 0);
  assert(output.close());
  assert(output.state() == AudioOutputState::Closed);
}

// B3: flush opens a new processed epoch. After a long first run, post-flush
// progress must restart near zero, never inherit the old baseline.
void flush_resets_processed_epoch() {
  QtAudioOutput output{QtAudioOutput::Options{{192000}, {}}};
  assert(output.open({44100, 1}));
  assert(output.enqueue(sine_block(44100, 1, 88200, 440.0)) == AudioEnqueueResult::Accepted);
  assert(output.start());
  assert(poll_until([&] { return output.metrics().device_frames_processed > 44000; }, 15000));
  assert(output.flush());
  assert(output.metrics().device_frames_processed == 0);
  assert(output.metrics().device_frames_submitted == 0);
  assert(output.enqueue(sine_block(44100, 1, 44100, 440.0)) == AudioEnqueueResult::Accepted);
  assert(poll_until([&] { return output.metrics().device_frames_submitted > 0; }, 10000));
  // A stale baseline would report the full new submission instantly; fresh
  // audio cannot have played 0.5 s within microseconds of submission.
  assert(output.metrics().device_frames_processed < 22050);
  assert(poll_until([&] { return output.metrics().device_frames_processed > 0; }, 10000));
  assert(output.request_drain());
  assert(poll_until([&] { return output.state() == AudioOutputState::Completed; }, 20000));
  assert(output.close());
}

// B5/B6-Qt: full lifecycle driven from a foreign thread while the owner
// pump is active. Any engine-lock/blocking-marshal cycle deadlocks here;
// the watchdog bounds the test instead of hanging CI.
void foreign_thread_lifecycle_no_deadlock() {
  QtAudioOutput output{QtAudioOutput::Options{{65536}, {}}};
  std::atomic<bool> worker_done{false};
  std::string worker_error;
  std::thread worker([&] {
    if (!output.open({44100, 1})) {
      worker_error = "open";
      return;
    }
    if (output.enqueue(sine_block(44100, 1, 4410, 440.0)) != AudioEnqueueResult::Accepted) {
      worker_error = "enqueue";
      return;
    }
    if (!output.start()) {
      worker_error = "start";
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (!output.stop()) {
      worker_error = "stop";
      return;
    }
    if (!output.flush()) {
      worker_error = "flush";
      return;
    }
    assert(output.close());
    worker_done = true;
  });
  const bool finished =
      poll_until([&] { return worker_done.load(); }, 15000);
  if (finished) {
    worker.join();
  } else {
    worker.detach();
  }
  assert(finished);
  assert(worker_error.empty());
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
  assert(output.close());
  assert(output.open({44100, 1}));
  assert(output.state() == AudioOutputState::Opened);
  assert(output.close());
}

void starvation_surfaces_underrun() {
  QtAudioOutput output{QtAudioOutput::Options{{65536}, {}}};
  assert(output.open({44100, 1}));
  assert(output.enqueue(sine_block(44100, 1, 441, 440.0)) == AudioEnqueueResult::Accepted);
  assert(output.start());
  assert(poll_until([&] { return output.metrics().underrun_count >= 1; }, 10000));
  assert(output.state() == AudioOutputState::Running);
  assert(output.close());
}

void multichannel_policy_without_guessing() {
  QtAudioOutput output{QtAudioOutput::Options{{4096}, {}}};
  if (output.open({44100, 6})) {
    // Exact passthrough only: the device took 6 channels as-is, no downmix.
    assert(output.device_format()->channel_count == 6);
    assert(output.close());
    return;
  }
  assert(output.state() == AudioOutputState::Error);
  assert(output.error()->code == application::AudioOutputError::Code::UnsupportedConversion);
  assert(output.close());
}

void owner_close_returns_during_foreign_open() {
  QtAudioOutput output{QtAudioOutput::Options{{65536}, {}}};
  bool open_result = false;
  std::thread worker([&] { open_result = output.open({44100, 1}); });
  // Close from this thread must return within bounds while the foreign
  // open is in flight (true before the claim, false during it): it must
  // never wait on the other operation. The owner keeps pumping so the
  // worker's marshaled backend calls can proceed.
  auto closing = std::async(std::launch::async, [&] { return output.close(); });
  bool close_returned = false;
  bool close_value = false;
  QElapsedTimer timer;
  timer.start();
  while (!close_returned && timer.elapsed() < 5000) {
    if (closing.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
      close_value = closing.get();
      close_returned = true;
    }
    QCoreApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  assert(close_returned);
  worker.join();
  if (close_value) {
    // Closed before the worker claimed: it may have opened afterwards.
    if (output.state() == AudioOutputState::Opened) {
      assert(output.close());
    }
  } else {
    assert(open_result);
    assert(output.state() == AudioOutputState::Opened);
    assert(output.close());
  }
  assert(output.state() == AudioOutputState::Closed);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app{argc, argv};
  // Device enumeration can expose a stale PulseAudio/PipeWire endpoint even
  // when no usable server is available. Keep CI deterministic and make real
  // device coverage an explicit local smoke choice; once enabled, failures
  // are real test failures rather than skips.
  const char* run_device_tests = std::getenv("PULSAR_RUN_AUDIO_DEVICE_TESTS");
  if (run_device_tests == nullptr || std::string{run_device_tests} != "1") {
    std::fprintf(stderr,
                 "SKIP: set PULSAR_RUN_AUDIO_DEVICE_TESTS=1 for real-device Qt audio tests\n");
    return 0;
  }
  if (!host_audio_service_available()) {
    std::fprintf(stderr, "SKIP: no usable host audio service\n");
    return 0;
  }
  bogus_device_open_fails();
  if (!usable_backend()) {
    std::fprintf(stderr, "SKIP: no usable Qt audio backend/device\n");
    return 0;
  }
  drain_lifecycle_with_worker_producer();
  start_empty_stop_flush_reopen();
  starvation_surfaces_underrun();
  multichannel_policy_without_guessing();
  flush_resets_processed_epoch();
  foreign_thread_lifecycle_no_deadlock();
  owner_close_returns_during_foreign_open();
}
