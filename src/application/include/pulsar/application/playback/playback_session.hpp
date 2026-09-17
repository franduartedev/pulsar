#pragma once

#include <pulsar/application/playback/monotonic_clock.hpp>
#include <pulsar/application/playback/playback_transport.hpp>
#include <pulsar/application/playback/playback_worker.hpp>
#include <pulsar/application/playback/presentation_scheduler.hpp>

#include <memory>
#include <optional>
#include <string>

namespace pulsar::application {

enum class PlaybackIntent {
  Paused,
  Playing,
};

enum class PlaybackAvailability {
  Empty,
  Preparing,
  Ready,
  Seeking,
  Ended,
  Error,
};

struct PlaybackSessionStats {
  std::size_t stale_event_count{0};
  std::size_t control_accept_count{0};
};

class PlaybackSession {
public:
  PlaybackSession(const MonotonicClock& clock, std::unique_ptr<PlaybackMediaWorker> worker);
  ~PlaybackSession();

  PlaybackSession(const PlaybackSession&) = delete;
  PlaybackSession& operator=(const PlaybackSession&) = delete;

  void open();
  void play();
  void pause();
  void seek(PlaybackPosition position);
  [[nodiscard]] PresentationDecision update();

  [[nodiscard]] PlaybackIntent intent() const noexcept {
    return intent_;
  }

  [[nodiscard]] PlaybackAvailability availability() const noexcept {
    return availability_;
  }

  [[nodiscard]] PlaybackPosition position() const {
    return transport_.position();
  }

  [[nodiscard]] std::optional<std::string> error_message() const {
    return error_message_;
  }

  [[nodiscard]] PlaybackSessionStats stats() const noexcept {
    return stats_;
  }

private:
  void poll_worker_events();
  void handle_event(PlaybackWorkerEvent event);
  void handle_seek_complete(PlaybackWorkerSeekComplete complete);
  void request_read_if_needed(const PresentationDecision& decision);
  void request_seek_for_position(PlaybackPosition position, PlaybackAvailability availability);
  void set_error(std::string message);
  [[nodiscard]] StreamTimestamp stream_target_for(PlaybackPosition position) const;

  std::unique_ptr<PlaybackMediaWorker> worker_;
  PlaybackTransport transport_;
  VideoSourceDescriptor descriptor_;
  SingleStreamPlaybackClockMap time_map_;
  PresentationScheduler scheduler_;
  PlaybackIntent intent_{PlaybackIntent::Paused};
  PlaybackAvailability availability_{PlaybackAvailability::Empty};
  PlaybackGeneration generation_{0};
  bool read_in_flight_{false};
  bool source_exhausted_{false};
  std::optional<PlaybackPosition> pending_seek_position_;
  std::optional<std::string> error_message_;
  PlaybackSessionStats stats_;
};

} // namespace pulsar::application
