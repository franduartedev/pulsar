#pragma once

#include <pulsar/application/playback/video_source.hpp>

#include <cstdint>
#include <optional>
#include <variant>

namespace pulsar::application {

using PlaybackGeneration = std::uint64_t;

struct PlaybackWorkerSample {
  PlaybackGeneration generation;
  VideoSample sample;
};

struct PlaybackWorkerSeekComplete {
  PlaybackGeneration generation;
  VideoSeekOutcome outcome;
};

struct PlaybackWorkerEndOfStream {
  PlaybackGeneration generation;
  VideoSourceEndOfStream end;
};

struct PlaybackWorkerCancelled {
  PlaybackGeneration generation;
};

struct PlaybackWorkerError {
  PlaybackGeneration generation;
  VideoSourceError error;
};

using PlaybackWorkerEvent = std::variant<PlaybackWorkerSample, PlaybackWorkerSeekComplete,
                                         PlaybackWorkerEndOfStream, PlaybackWorkerCancelled,
                                         PlaybackWorkerError>;

class PlaybackMediaWorker {
public:
  virtual ~PlaybackMediaWorker() = default;

  [[nodiscard]] virtual VideoSourceDescriptor describe() const = 0;
  virtual void request_read(PlaybackGeneration generation) = 0;
  virtual void request_seek_for_presentation(PlaybackGeneration generation,
                                             StreamTimestamp target) = 0;
  virtual void cancel_pending() = 0;
  [[nodiscard]] virtual std::optional<PlaybackWorkerEvent> poll_event() = 0;
};

} // namespace pulsar::application
