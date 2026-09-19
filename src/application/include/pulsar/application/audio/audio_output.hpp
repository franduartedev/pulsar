#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace pulsar::application {

// A2 standalone audio-device output contract. PULSAR-owned types only: no Qt,
// FFmpeg, or platform audio API may appear here. Implementations feed owned
// PCM to a real device; this interface knows nothing about timelines,
// clocks, or synchronization (A3 owns those decisions).

enum class AudioOutputState {
  Closed,    // No session. open() may configure a new one.
  Opened,    // Device configured. enqueue() accepted; start() may run.
  Running,   // Backend is consuming queued audio.
  Draining,  // No more input expected; queue + backend buffer emptying.
  Completed, // Drain finished normally. Terminal for this session.
  Error,     // Terminal device failure. close() then open() to recover.
};

enum class AudioSampleFormat {
  Float32,
  Int16,
  UInt8,
};

// A1 source PCM: always interleaved float32. Rate/channels are native.
struct AudioSourceFormat {
  int sample_rate;
  int channel_count;
};

// Device PCM negotiated at open(). Queryable after a successful open().
struct AudioDeviceFormat {
  int sample_rate;
  int channel_count;
  AudioSampleFormat sample_format;
};

struct AudioOutputMetrics {
  // Source frames accepted via enqueue() since the session point.
  std::uint64_t source_frames_accepted{0};
  // Device frames handed to the backend since the session point.
  std::uint64_t device_frames_submitted{0};
  // Backend-processed device frames. Monotonic within one open/start
  // session; backend progress only, never a speaker timestamp, never
  // mapped to timeline time. Always <= device_frames_submitted.
  std::uint64_t device_frames_processed{0};
  // Device frames currently held in the adapter queue.
  std::uint64_t buffered_device_frames{0};
  // Starvation episodes while Running with more input expected.
  // Cumulative since open(); never counted pre-start or during drain.
  std::uint64_t underrun_count{0};
};

struct AudioOutputError {
  enum class Code {
    NoDevice,
    UnsupportedConversion,
    BackendFailure,
  };

  Code code{Code::BackendFailure};
  std::string message;
};

enum class AudioEnqueueResult {
  Accepted,
  // Queue capacity insufficient; nothing was copied, retry later.
  Backpressure,
  // The single converted block exceeds total queue capacity.
  BlockTooLarge,
  // enqueue() in Closed/Draining/Completed/Error, or before open().
  InvalidState,
  // Block rate/channels differ from the open source format.
  FormatMismatch,
  // Channel mapping outside A2 scope (only equal counts or mono/stereo).
  UnsupportedChannels,
};

class AudioOutput {
public:
  virtual ~AudioOutput() = default;

  // Lifecycle operations are exclusive and fail-fast: if another
  // lifecycle operation is already running its backend phase, the second
  // call returns false immediately (Busy) instead of waiting or
  // overlapping backend mutations. No lifecycle call ever waits for
  // another lifecycle call, so owner-thread callers cannot deadlock.
  //
  // Negotiates the device format for source and enters Opened. Valid only
  // from Closed. Returns false and latches error() on failure.
  virtual bool open(const AudioSourceFormat& source) = 0;
  // Stops/resets the device, discards buffers, clears the latched error,
  // returns to Closed. Returns false (Busy) without changing state while
  // another lifecycle operation is active; call again once it finishes.
  // Idempotent once Closed (returns true).
  //
  // Destruction contract: destroy only with no in-flight lifecycle
  // operation (prefer an explicit close() that returned true, or a never
  // opened instance). Destructors never block waiting for transitions.
  virtual bool close() = 0;
  // Copies the block into the device-format queue atomically: accepted
  // fully or rejected completely, never partial, never drops accepted
  // frames. Nonblocking; the caller retains its block.
  [[nodiscard]] virtual AudioEnqueueResult enqueue(const class AudioBlock& block) = 0;
  // Begins playback. Requires Opened with at least one queued device
  // frame; starting empty fails deterministically without counting an
  // underrun.
  virtual bool start() = 0;
  // Declares no more input is expected. Valid only from Running. Further
  // enqueue() calls are rejected; completion follows once queued and
  // backend-buffered audio are exhausted (NOT an underrun).
  virtual bool request_drain() = 0;
  // Immediately terminates playback and discards pending audio. Lands in
  // Opened (reusable) unless Closed/Error, where it fails.
  virtual bool stop() = 0;
  // Immediately discards adapter-queue and backend-buffered audio while
  // preserving the session: state is unchanged, accounting restarts from
  // a new session point. Valid in Opened/Running/Draining.
  virtual bool flush() = 0;

  [[nodiscard]] virtual AudioOutputState state() const = 0;
  [[nodiscard]] virtual std::optional<AudioDeviceFormat> device_format() const = 0;
  [[nodiscard]] virtual AudioOutputMetrics metrics() const = 0;
  // Latched terminal failure; has_value() exactly when state() == Error.
  [[nodiscard]] virtual std::optional<AudioOutputError> error() const = 0;
};

} // namespace pulsar::application
