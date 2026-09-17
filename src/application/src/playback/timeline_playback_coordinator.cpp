#include <pulsar/application/playback/timeline_playback_coordinator.hpp>

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace pulsar::application {

TimelinePlaybackCoordinator::TimelinePlaybackCoordinator(const MonotonicClock& clock,
                                                         PlaybackWorkerFactory worker_factory)
    : clock_{clock}, worker_factory_{std::move(worker_factory)}, transport_{clock} {}

void TimelinePlaybackCoordinator::set_snapshot(ProjectSnapshot snapshot) {
  const domain::MediaTime position = timeline_position();
  transport_.pause();
  transport_.seek(PlaybackPosition{position});
  reset_source();
  snapshot_ = std::move(snapshot);
  segment_kind_ = SegmentKind::None;
  current_presentation_.reset();
  source_creation_error_.reset();
  active_missing_.reset();
  segment_end_.reset();
  const auto outcome = resolve_clip_at(*snapshot_, snapshot_->video_track_id, position);
  reconcile(outcome, position, true);
}

void TimelinePlaybackCoordinator::play() {
  intent_ = PlaybackIntent::Playing;
  if (!snapshot_.has_value()) {
    return;
  }
  if (segment_kind_ == SegmentKind::Gap || segment_kind_ == SegmentKind::Missing) {
    transport_.play();
  } else if (segment_kind_ == SegmentKind::Clip && source_session_ != nullptr &&
             source_session_->availability() == PlaybackAvailability::Ready) {
    transport_.play();
  }
}

void TimelinePlaybackCoordinator::pause() {
  intent_ = PlaybackIntent::Paused;
  transport_.pause();
}

void TimelinePlaybackCoordinator::seek(domain::MediaTime timeline_position_value) {
  if (timeline_position_value < domain::MediaTime::zero()) {
    throw std::invalid_argument{"timeline playback position cannot be negative"};
  }
  if (!snapshot_.has_value()) {
    throw std::invalid_argument{"cannot seek without a project snapshot"};
  }
  transport_.pause();
  transport_.seek(PlaybackPosition{timeline_position_value});
  const auto outcome = resolve_clip_at(*snapshot_, snapshot_->video_track_id,
                                       timeline_position_value);
  reconcile(outcome, timeline_position_value, true);
}

TimelinePresentation TimelinePlaybackCoordinator::update() {
  if (!snapshot_.has_value()) {
    return TimelineEndedPresentation{domain::MediaTime::zero(), domain::MediaTime::zero()};
  }

  const domain::MediaTime position = timeline_position();
  switch (segment_kind_) {
  case SegmentKind::Clip:
    if (active_clip_.has_value() && position < active_clip_->timeline_range.end()) {
      return evaluate_clip(*active_clip_, position);
    }
    break;
  case SegmentKind::Gap:
    if (segment_end_.has_value() && position < *segment_end_) {
      current_presentation_ = TimelineGapPresentation{position};
      return *current_presentation_;
    }
    break;
  case SegmentKind::Missing:
    if (active_missing_.has_value() && position < active_missing_->timeline_range.end()) {
      current_presentation_ = TimelineMissingMediaPresentation{
          active_missing_->clip_id, active_missing_->media_id, position,
          active_missing_->timeline_range, active_missing_->reason};
      return *current_presentation_;
    }
    break;
  case SegmentKind::Ended:
  case SegmentKind::SourceError:
    return *current_presentation_;
  case SegmentKind::None:
    break;
  }

  const auto outcome = resolve_clip_at(*snapshot_, snapshot_->video_track_id, position);
  reconcile(outcome, position, true);
  if (std::holds_alternative<ResolvedClip>(outcome)) {
    return evaluate_clip(std::get<ResolvedClip>(outcome), position);
  }
  return *current_presentation_;
}

domain::MediaTime TimelinePlaybackCoordinator::timeline_position() const {
  return transport_.position().value();
}

void TimelinePlaybackCoordinator::reconcile(const TimelineResolveOutcome& outcome,
                                            domain::MediaTime position, bool force_transition) {
  std::visit(
      [this, position, force_transition](const auto& value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, ResolvedClip>) {
          enter_clip(value, force_transition);
        } else if constexpr (std::is_same_v<Value, TimelineGap>) {
          enter_gap(value, force_transition);
        } else if constexpr (std::is_same_v<Value, MissingClipMedia>) {
          enter_missing(value, force_transition);
        } else if constexpr (std::is_same_v<Value, TimelineEnded>) {
          enter_ended(value, force_transition);
        }
      },
      outcome);
  (void)position;
}

void TimelinePlaybackCoordinator::enter_clip(const ResolvedClip& clip, bool force_transition) {
  const bool same_identity = active_clip_.has_value() && active_clip_->clip_id == clip.clip_id;
  if (same_identity && !force_transition &&
      (segment_kind_ == SegmentKind::Clip || segment_kind_ == SegmentKind::SourceError)) {
    return;
  }

  transport_.pause();
  source_creation_error_.reset();
  const PlaybackPosition source_position{clip.source_time};
  const bool reuse_session = source_session_ != nullptr && active_clip_.has_value() &&
                             active_clip_->media_id == clip.media_id;
  if (reuse_session) {
    source_session_->seek(source_position);
  } else {
    reset_source();
    if (!worker_factory_) {
      source_creation_error_ = "playback worker factory is not configured";
    } else {
      try {
        auto worker = worker_factory_(clip.source_locator, clip.video_stream_index);
        if (!worker) {
          source_creation_error_ = "playback worker factory returned no worker";
        } else {
          source_session_ =
              std::make_unique<PlaybackSession>(clock_, std::move(worker));
          source_session_->seek(source_position);
        }
      } catch (const std::exception& error) {
        source_creation_error_ = error.what();
      }
    }
  }
  active_clip_ = clip;
  active_missing_.reset();
  segment_end_ = clip.timeline_range.end();
  segment_kind_ = SegmentKind::Clip;
  current_presentation_ = TimelineStarvedPresentation{clip.clip_id, clip.media_id, clip.timeline_range.start()};
}

void TimelinePlaybackCoordinator::enter_gap(const TimelineGap& gap, bool force_transition) {
  if (!force_transition && segment_kind_ == SegmentKind::Gap) {
    return;
  }
  reset_source();
  active_clip_.reset();
  active_missing_.reset();
  source_creation_error_.reset();
  segment_end_ = next_gap_boundary(gap.position);
  segment_kind_ = SegmentKind::Gap;
  current_presentation_ = make_gap(gap);
  if (intent_ == PlaybackIntent::Playing) {
    transport_.play();
  }
}

void TimelinePlaybackCoordinator::enter_missing(const MissingClipMedia& missing,
                                                bool force_transition) {
  if (!force_transition && segment_kind_ == SegmentKind::Missing && active_clip_.has_value() &&
      active_clip_->clip_id == missing.clip_id) {
    return;
  }
  reset_source();
  active_clip_.reset();
  active_missing_ = missing;
  source_creation_error_.reset();
  segment_end_ = missing.timeline_range.end();
  segment_kind_ = SegmentKind::Missing;
  current_presentation_ = make_missing(missing);
  if (intent_ == PlaybackIntent::Playing) {
    transport_.play();
  }
}

void TimelinePlaybackCoordinator::enter_ended(const TimelineEnded& ended, bool force_transition) {
  if (!force_transition && segment_kind_ == SegmentKind::Ended) {
    return;
  }
  reset_source();
  active_clip_.reset();
  active_missing_.reset();
  source_creation_error_.reset();
  segment_end_ = ended.duration;
  transport_.pause();
  transport_.seek(PlaybackPosition{ended.duration});
  segment_kind_ = SegmentKind::Ended;
  current_presentation_ = make_ended(ended);
}

TimelinePresentation TimelinePlaybackCoordinator::evaluate_clip(const ResolvedClip& clip,
                                                                domain::MediaTime position) {
  if (source_creation_error_.has_value()) {
    transport_.pause();
    segment_kind_ = SegmentKind::SourceError;
    current_presentation_ = TimelineSourceErrorPresentation{
        clip.clip_id, clip.media_id, *source_creation_error_};
    return *current_presentation_;
  }
  if (source_session_ == nullptr) {
    transport_.pause();
    current_presentation_ = TimelineStarvedPresentation{clip.clip_id, clip.media_id, position};
    return *current_presentation_;
  }

  const domain::MediaTime source_time =
      clip.source_range.start() + (position - clip.timeline_range.start());
  const PresentationDecision decision =
      source_session_->update_at(PlaybackPosition{source_time});

  if (std::holds_alternative<PresentSample>(decision)) {
    if (intent_ == PlaybackIntent::Playing &&
        source_session_->availability() == PlaybackAvailability::Ready) {
      transport_.play();
    }
    const auto& present = std::get<PresentSample>(decision);
    current_presentation_ = TimelinePresentSample{clip.clip_id, clip.media_id, position,
                                                   present.sample};
  } else if (std::holds_alternative<HoldPresentation>(decision)) {
    if (intent_ == PlaybackIntent::Playing &&
        source_session_->availability() == PlaybackAvailability::Ready) {
      transport_.play();
    }
    const auto& hold = std::get<HoldPresentation>(decision);
    current_presentation_ = TimelineHoldPresentation{clip.clip_id, clip.media_id, position,
                                                     hold.sample};
  } else if (std::holds_alternative<PresentationGap>(decision)) {
    if (intent_ == PlaybackIntent::Playing) {
      transport_.play();
    }
    current_presentation_ = TimelineSourceGapPresentation{clip.clip_id, clip.media_id, position};
  } else if (std::holds_alternative<PresentationStarved>(decision)) {
    transport_.pause();
    current_presentation_ = TimelineStarvedPresentation{clip.clip_id, clip.media_id, position};
  } else if (std::holds_alternative<PresentationEnded>(decision)) {
    if (intent_ == PlaybackIntent::Playing) {
      transport_.play();
    }
    current_presentation_ = TimelineSourceEndedEarlyPresentation{clip.clip_id, clip.media_id,
                                                                  position};
  } else {
    const auto& error = std::get<PresentationTemporalError>(decision);
    transport_.pause();
    segment_kind_ = SegmentKind::SourceError;
    current_presentation_ = TimelineSourceErrorPresentation{clip.clip_id, clip.media_id,
                                                            error.message};
  }
  return *current_presentation_;
}

TimelinePresentation TimelinePlaybackCoordinator::make_gap(const TimelineGap& gap) const {
  return TimelineGapPresentation{gap.position};
}

TimelinePresentation TimelinePlaybackCoordinator::make_missing(
    const MissingClipMedia& missing) const {
  return TimelineMissingMediaPresentation{missing.clip_id, missing.media_id, missing.timeline_position,
                                          missing.timeline_range, missing.reason};
}

TimelinePresentation TimelinePlaybackCoordinator::make_ended(const TimelineEnded& ended) const {
  return TimelineEndedPresentation{ended.duration, ended.duration};
}

domain::MediaTime TimelinePlaybackCoordinator::next_gap_boundary(
    domain::MediaTime position) const {
  const auto* track = snapshot_->timeline.find_track(snapshot_->video_track_id);
  if (track != nullptr) {
    for (const auto& clip : track->clips()) {
      if (clip.timeline_start() > position) {
        return clip.timeline_start();
      }
    }
  }
  return snapshot_->timeline.duration();
}

void TimelinePlaybackCoordinator::reset_source() {
  source_session_.reset();
}

} // namespace pulsar::application
