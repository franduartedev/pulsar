#pragma once

#include <pulsar/application/playback/monotonic_clock.hpp>
#include <pulsar/application/playback/playback_time.hpp>

namespace pulsar::application {

enum class PlaybackTransportState {
  Paused,
  Playing,
};

class PlaybackTransport {
public:
  explicit PlaybackTransport(const MonotonicClock& clock);

  [[nodiscard]] PlaybackTransportState state() const noexcept {
    return state_;
  }

  [[nodiscard]] bool is_playing() const noexcept {
    return state_ == PlaybackTransportState::Playing;
  }

  [[nodiscard]] PlaybackPosition position() const;

  void play();
  void pause();
  void seek(PlaybackPosition position);

private:
  [[nodiscard]] PlaybackPosition position_at(MonotonicInstant now) const;

  const MonotonicClock& clock_;
  PlaybackTransportState state_{PlaybackTransportState::Paused};
  PlaybackPosition anchor_position_{PlaybackPosition::zero()};
  MonotonicInstant anchor_clock_{MonotonicInstant::zero()};
};

} // namespace pulsar::application
