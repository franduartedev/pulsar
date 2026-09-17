#include <pulsar/application/playback/playback_transport.hpp>

namespace pulsar::application {

PlaybackTransport::PlaybackTransport(const MonotonicClock& clock) : clock_{clock} {}

PlaybackPosition PlaybackTransport::position() const {
  if (state_ == PlaybackTransportState::Paused) {
    return anchor_position_;
  }

  return position_at(clock_.now());
}

void PlaybackTransport::play() {
  if (state_ == PlaybackTransportState::Playing) {
    return;
  }

  anchor_clock_ = clock_.now();
  state_ = PlaybackTransportState::Playing;
}

void PlaybackTransport::pause() {
  if (state_ == PlaybackTransportState::Paused) {
    return;
  }

  const MonotonicInstant now = clock_.now();
  anchor_position_ = position_at(now);
  anchor_clock_ = now;
  state_ = PlaybackTransportState::Paused;
}

void PlaybackTransport::seek(PlaybackPosition position) {
  anchor_position_ = position;
  if (state_ == PlaybackTransportState::Playing) {
    anchor_clock_ = clock_.now();
  }
}

PlaybackPosition PlaybackTransport::position_at(MonotonicInstant now) const {
  const domain::MediaTime elapsed = now.elapsed_since(anchor_clock_);
  return PlaybackPosition{anchor_position_.value() + elapsed};
}

} // namespace pulsar::application
