#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <stdexcept>

namespace pulsar::domain {

enum class TransitionType { CrossDissolve };

// A transition belongs to one adjacent edit relationship, never to media.
struct Transition {
  TrackId track_id;
  ClipId outgoing_clip_id;
  ClipId incoming_clip_id;
  TransitionType type{TransitionType::CrossDissolve};
  MediaTime duration{0, {1, 1}};

  friend bool operator==(const Transition&, const Transition&) = default;
};

inline void validate_transition_value(const Transition& transition) {
  if (transition.outgoing_clip_id == transition.incoming_clip_id ||
      transition.duration <= MediaTime::zero()) {
    throw std::invalid_argument{"invalid transition value"};
  }
}

} // namespace pulsar::domain
