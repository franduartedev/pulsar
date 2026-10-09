#include <pulsar/application/playback/playback_session.hpp>

#include <stdexcept>
#include <utility>

namespace pulsar::application {
namespace {

[[nodiscard]] StreamTimestamp descriptor_origin_or_zero(const VideoSourceDescriptor& descriptor) {
  if (descriptor.stream_origin.has_value()) {
    return *descriptor.stream_origin;
  }
  return StreamTimestamp{domain::MediaTime::zero()};
}

} // namespace

PlaybackSession::PlaybackSession(const MonotonicClock& clock,
                                 std::unique_ptr<PlaybackMediaWorker> worker)
    : worker_{std::move(worker)},
      transport_{clock},
      descriptor_{worker_ != nullptr ? worker_->describe()
                                     : throw std::invalid_argument{"playback worker is required"}},
      time_map_{descriptor_origin_or_zero(descriptor_)},
      scheduler_{time_map_} {}

PlaybackSession::~PlaybackSession() {
  if (worker_ != nullptr) {
    worker_->cancel_pending();
  }
}

void PlaybackSession::open() {
  ++stats_.control_accept_count;
  if (worker_terminal_) return;
  intent_ = PlaybackIntent::Paused;
  request_seek_for_position(PlaybackPosition::zero(), PlaybackAvailability::Preparing);
}

void PlaybackSession::play() {
  ++stats_.control_accept_count;
  intent_ = PlaybackIntent::Playing;
  if (availability_ == PlaybackAvailability::Ready) {
    transport_.play();
  }
}

void PlaybackSession::pause() {
  ++stats_.control_accept_count;
  intent_ = PlaybackIntent::Paused;
  transport_.pause();
}

void PlaybackSession::seek(PlaybackPosition position) {
  ++stats_.control_accept_count;
  if (worker_terminal_) return;
  request_seek_for_position(position, PlaybackAvailability::Seeking);
}

PresentationDecision PlaybackSession::update() {
  return update_at_impl(transport_.position(), true);
}

PresentationDecision PlaybackSession::update_at(PlaybackPosition source_position) {
  return update_at_impl(source_position, false);
}

PresentationDecision PlaybackSession::update_at_impl(PlaybackPosition source_position,
                                                     bool drive_transport) {
  poll_worker_events();
  if (drive_transport) {
    source_position = transport_.position();
  }

  if (availability_ == PlaybackAvailability::Error) {
    return PresentationTemporalError{error_message_.value_or("playback session error")};
  }
  if (availability_ == PlaybackAvailability::Empty || availability_ == PlaybackAvailability::Preparing ||
      availability_ == PlaybackAvailability::Seeking) {
    return PresentationStarved{};
  }

  auto decision = scheduler_.evaluate(source_position);
  if (std::holds_alternative<PresentationEnded>(decision)) {
    availability_ = PlaybackAvailability::Ended;
    if (drive_transport) {
      transport_.pause();
    }
  } else if (std::holds_alternative<PresentationTemporalError>(decision)) {
    const auto& error = std::get<PresentationTemporalError>(decision);
    set_error(error.message);
  } else {
    request_read_if_needed(decision);
  }

  return decision;
}

void PlaybackSession::poll_worker_events() {
  while (auto event = worker_->poll_event()) {
    handle_event(std::move(*event));
  }
}

void PlaybackSession::handle_event(PlaybackWorkerEvent event) {
  std::visit(
      [this](auto&& value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, PlaybackWorkerFatalError>) {
          worker_terminal_ = true;
          read_in_flight_ = false;
          pending_seek_position_.reset();
          set_error(value.error.message);
        } else if constexpr (std::is_same_v<Value, PlaybackWorkerSeekComplete>) {
          if (value.generation != generation_) {
            ++stats_.stale_event_count;
            return;
          }
          handle_seek_complete(std::move(value));
        } else if constexpr (std::is_same_v<Value, PlaybackWorkerSample>) {
          if (value.generation != generation_) {
            ++stats_.stale_event_count;
            return;
          }
          read_in_flight_ = false;
          scheduler_.ingest(std::move(value.sample));
        } else if constexpr (std::is_same_v<Value, PlaybackWorkerEndOfStream>) {
          if (value.generation != generation_) {
            ++stats_.stale_event_count;
            return;
          }
          read_in_flight_ = false;
          source_exhausted_ = true;
          scheduler_.ingest(value.end);
        } else if constexpr (std::is_same_v<Value, PlaybackWorkerCancelled>) {
          if (value.generation != generation_) {
            ++stats_.stale_event_count;
            return;
          }
          read_in_flight_ = false;
        } else if constexpr (std::is_same_v<Value, PlaybackWorkerError>) {
          if (value.generation != generation_) {
            ++stats_.stale_event_count;
            return;
          }
          set_error(value.error.message);
        }
      },
      std::move(event));
}

void PlaybackSession::handle_seek_complete(PlaybackWorkerSeekComplete complete) {
  read_in_flight_ = false;
  source_exhausted_ = false;
  scheduler_.reset();

  std::visit(
      [this](auto&& outcome) {
        using Outcome = std::decay_t<decltype(outcome)>;
        if constexpr (std::is_same_v<Outcome, VideoSeekHit>) {
          scheduler_.ingest(outcome.sample);
          if (outcome.lookahead.has_value()) {
            scheduler_.ingest(*outcome.lookahead);
          }
        } else if constexpr (std::is_same_v<Outcome, VideoSeekBeforeFirst>) {
          if (outcome.first_sample.has_value()) {
            scheduler_.ingest(*outcome.first_sample);
          }
        } else if constexpr (std::is_same_v<Outcome, VideoSeekGap>) {
          scheduler_.ingest(outcome.before);
          if (outcome.after.has_value()) {
            scheduler_.ingest(*outcome.after);
          }
        } else if constexpr (std::is_same_v<Outcome, VideoSeekEndOfStream>) {
          if (outcome.last_sample.has_value()) {
            scheduler_.ingest(*outcome.last_sample);
          }
          scheduler_.ingest(VideoSourceEndOfStream{outcome.known_presentation_end,
                                                   outcome.terminal_end_known});
          source_exhausted_ = true;
        } else if constexpr (std::is_same_v<Outcome, VideoSourceCancelled>) {
        } else if constexpr (std::is_same_v<Outcome, VideoSourceError>) {
          set_error(outcome.message);
        }
      },
      std::move(complete.outcome));

  if (availability_ != PlaybackAvailability::Error) {
    availability_ = PlaybackAvailability::Ready;
  }

  if (pending_seek_position_.has_value()) {
    transport_.seek(*pending_seek_position_);
    pending_seek_position_.reset();
  }
  if (intent_ == PlaybackIntent::Playing && availability_ == PlaybackAvailability::Ready) {
    transport_.play();
  } else {
    transport_.pause();
  }
}

void PlaybackSession::request_read_if_needed(const PresentationDecision& decision) {
  if (read_in_flight_ || source_exhausted_ || availability_ != PlaybackAvailability::Ready) {
    return;
  }
  if (!std::holds_alternative<PresentationStarved>(decision) &&
      scheduler_.buffered_sample_count() >= 2) {
    return;
  }

  worker_->request_read(generation_);
  read_in_flight_ = true;
}

void PlaybackSession::request_seek_for_position(PlaybackPosition position,
                                                PlaybackAvailability availability) {
  if (worker_terminal_) return;
  ++generation_;
  pending_seek_position_ = position;
  read_in_flight_ = false;
  source_exhausted_ = false;
  availability_ = availability;
  scheduler_.reset();
  transport_.pause();
  worker_->request_seek_for_presentation(generation_, stream_target_for(position));
}

void PlaybackSession::set_error(std::string message) {
  error_message_ = std::move(message);
  availability_ = PlaybackAvailability::Error;
  transport_.pause();
}

StreamTimestamp PlaybackSession::stream_target_for(PlaybackPosition position) const {
  return time_map_.to_stream_timestamp(position);
}

} // namespace pulsar::application
