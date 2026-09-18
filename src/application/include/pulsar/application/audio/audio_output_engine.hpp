#pragma once

#include <pulsar/application/audio/audio_output.hpp>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace pulsar::application {

// Narrow owner-thread device seam for the output engine. All methods are
// called by the engine; implementations that wrap a threaded device backend
// (e.g. QAudioSink) must marshal to the owning thread internally. The engine
// itself is thread-safe and never retains caller-owned sample pointers.
class AudioDeviceBackend {
public:
  virtual ~AudioDeviceBackend() = default;

  // Returns the device format the backend will use for requested, or
  // std::nullopt when no usable output device exists.
  [[nodiscard]] virtual std::optional<AudioDeviceFormat>
  query_device_format(const AudioSourceFormat& requested) = 0;
  [[nodiscard]] virtual bool open_device(const AudioDeviceFormat& format) = 0;
  virtual void close_device() = 0;
  // Acquires the device stream for consumption. Returns false on failure.
  [[nodiscard]] virtual bool start_device() = 0;
  // Halts the device and discards backend-buffered audio.
  virtual void stop_device() = 0;
  // Drops backend-buffered audio while remaining started.
  virtual void reset_device() = 0;
  // Complete device frames the backend can take right now (nonblocking).
  [[nodiscard]] virtual std::size_t writable_frames() = 0;
  // Writes up to frame_count complete frames; returns frames taken.
  // Never splits a sample frame.
  virtual std::size_t write_frames(const void* frames, std::size_t frame_count) = 0;
  // Backend-processed device frames. Implementations clamp monotonically.
  [[nodiscard]] virtual std::uint64_t processed_frames() = 0;
  // Latched backend failure, if any.
  [[nodiscard]] virtual std::optional<std::string> backend_error() = 0;
};

struct AudioEngineLimits {
  // Total device frames the adapter queue may hold. Explicit and observable.
  std::size_t queue_frame_capacity;
};

// Concrete AudioOutput: bounded device-format queue, lifecycle state
// machine, metrics, and private PCM conversion. Device I/O happens only
// through AudioDeviceBackend, driven by transfer_to_backend().
class AudioOutputEngine final : public AudioOutput {
public:
  explicit AudioOutputEngine(AudioDeviceBackend& backend, AudioEngineLimits limits);

  AudioOutputEngine(const AudioOutputEngine&) = delete;
  AudioOutputEngine& operator=(const AudioOutputEngine&) = delete;

  bool open(const AudioSourceFormat& source) override;
  void close() override;
  [[nodiscard]] AudioEnqueueResult enqueue(const AudioBlock& block) override;
  bool start() override;
  bool request_drain() override;
  bool stop() override;
  bool flush() override;

  // Owner-thread pump step: moves queued device frames to the backend,
  // updates metrics, detects underrun vs drain completion. Safe to call
  // from tests without any device thread.
  bool transfer_to_backend();

  [[nodiscard]] AudioOutputState state() const override;
  [[nodiscard]] std::optional<AudioDeviceFormat> device_format() const override;
  [[nodiscard]] AudioOutputMetrics metrics() const override;
  [[nodiscard]] std::optional<AudioOutputError> error() const override;
  [[nodiscard]] std::size_t queue_frame_capacity() const noexcept { return limits_.queue_frame_capacity; }

private:
  void enter_error(AudioOutputError::Code code, std::string message);
  void reset_session_counters() noexcept;
  [[nodiscard]] std::size_t frame_size_bytes() const noexcept;
  [[nodiscard]] std::size_t buffered_frames() const noexcept;

  AudioDeviceBackend& backend_;
  AudioEngineLimits limits_;
  mutable std::mutex mutex_;
  AudioOutputState state_{AudioOutputState::Closed};
  AudioSourceFormat source_format_{0, 0};
  std::optional<AudioDeviceFormat> device_format_;
  std::optional<AudioOutputError> latched_error_;
  std::vector<std::uint8_t> queue_;
  std::size_t queue_head_{0};
  AudioOutputMetrics metrics_;
  bool drain_requested_{false};
  bool in_underrun_{false};
};

} // namespace pulsar::application
