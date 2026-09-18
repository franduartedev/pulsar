#pragma once

#include <pulsar/application/audio/audio_output.hpp>
#include <pulsar/application/audio/audio_output_engine.hpp>

#include <memory>
#include <string>

namespace pulsar::media::qt_audio {

// Standalone audio-device output backed by Qt6 Multimedia (QAudioSink, push
// mode). Implements application::AudioOutput without exposing any Qt type.
//
// Thread contract: construct on a Qt event-loop thread (the owner). All
// QAudioSink/QIODevice operations run on the owner internally; AudioOutput
// calls from any thread are safe (device calls marshal to the owner).
// Destroy on the owner thread when possible. No timeline, clock, or sync
// semantics: PCM is consumed sequentially only.
class QtAudioOutput : public application::AudioOutput {
public:
  struct Options {
    application::AudioEngineLimits limits;
    // System device id, or empty for the Qt default output device.
    std::string device_id;
  };

  explicit QtAudioOutput(Options options);
  ~QtAudioOutput() override;

  QtAudioOutput(const QtAudioOutput&) = delete;
  QtAudioOutput& operator=(const QtAudioOutput&) = delete;

  bool open(const application::AudioSourceFormat& source) override;
  void close() override;
  [[nodiscard]] application::AudioEnqueueResult
  enqueue(const application::AudioBlock& block) override;
  bool start() override;
  bool request_drain() override;
  bool stop() override;
  bool flush() override;

  [[nodiscard]] application::AudioOutputState state() const override;
  [[nodiscard]] std::optional<application::AudioDeviceFormat> device_format() const override;
  [[nodiscard]] application::AudioOutputMetrics metrics() const override;
  [[nodiscard]] std::optional<application::AudioOutputError> error() const override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace pulsar::media::qt_audio
