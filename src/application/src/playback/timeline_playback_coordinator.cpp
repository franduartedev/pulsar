#include <pulsar/application/playback/timeline_playback_coordinator.hpp>

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace pulsar::application {

TimelinePlaybackCoordinator::TimelinePlaybackCoordinator(const MonotonicClock& clock,
                                                         PlaybackWorkerFactory worker_factory,
                                                         AudioPlaybackSourceFactory audio_source_factory,
                                                         AudioOutput* audio_output)
    : clock_{clock},
      worker_factory_{std::move(worker_factory)},
      transport_{clock},
      audio_session_{std::make_unique<AudioMixPlaybackSession>(audio_output,
                                                                std::move(audio_source_factory))} {}

void TimelinePlaybackCoordinator::set_advance_permission(TimelineAdvancePermission permission) {
  advance_permission_ = permission;
}

void TimelinePlaybackCoordinator::synchronize_transport() {
  if (intent_ == PlaybackIntent::Playing &&
      advance_permission_ == TimelineAdvancePermission::Advance) {
    transport_.play();
  } else {
    transport_.pause();
  }
}

void TimelinePlaybackCoordinator::reanchor_transport(domain::MediaTime position) {
  transport_.pause();
  transport_.seek(PlaybackPosition{position});
}

void TimelinePlaybackCoordinator::set_snapshot(ProjectSnapshot snapshot) {
  const domain::MediaTime position = timeline_position();
  set_advance_permission(TimelineAdvancePermission::Hold);
  editorial_transition_pending_ = true;
  reanchor_transport(position);
  reset_source();
  snapshot_ = std::move(snapshot);
  audio_session_->configure(*snapshot_);
  segment_kind_ = SegmentKind::None;
  current_presentation_.reset();
  source_creation_error_.reset();
  active_clip_.reset();
  active_missing_.reset();
  segment_end_.reset();
  semantic_condition_ = PlaybackSemanticCondition::Inactive;
  const auto outcome = resolve_clip_at(*snapshot_, snapshot_->video_track_id, position);
  reconcile(outcome, position, true);
  synchronize_transport();
}

void TimelinePlaybackCoordinator::play() {
  intent_ = PlaybackIntent::Playing;
  const auto* video_track = snapshot_.has_value()
                                ? snapshot_->timeline.find_track(snapshot_->video_track_id)
                                : nullptr;
  const bool has_video_track = video_track != nullptr && !video_track->clips().empty();
  if (audio_session_ != nullptr && (!editorial_transition_pending_ || !has_video_track)) {
    audio_session_->play();
  }
  if (!snapshot_.has_value()) {
    return;
  }
  if (audio_session_ != nullptr && audio_session_->has_device_clock()) {
    clock_source_ = PlaybackClockSource::AudioDevice;
  } else {
    clock_source_ = PlaybackClockSource::Software;
  }
  synchronize_transport();
}

void TimelinePlaybackCoordinator::pause() {
  intent_ = PlaybackIntent::Paused;
  if (audio_session_ != nullptr) {
    audio_session_->pause();
  }
  clock_source_ = PlaybackClockSource::Paused;
  synchronize_transport();
}

void TimelinePlaybackCoordinator::seek(domain::MediaTime timeline_position_value) {
  if (timeline_position_value < domain::MediaTime::zero()) {
    throw std::invalid_argument{"timeline playback position cannot be negative"};
  }
  if (!snapshot_.has_value()) {
    throw std::invalid_argument{"cannot seek without a project snapshot"};
  }
  const auto outcome = resolve_clip_at(*snapshot_, snapshot_->video_track_id,
                                       timeline_position_value);
  editorial_transition_pending_ = true;
  reanchor_transport(timeline_position_value);
  // A seek is one authoritative discontinuity. Invalidate mixer sources and
  // queued device PCM before publishing the new video segment or resuming the
  // transport, so no old audio can observe the new editorial position.
  audio_session_->seek(timeline_position_value);
  clock_source_ = intent_ == PlaybackIntent::Playing ? PlaybackClockSource::Software
                                                       : PlaybackClockSource::Paused;
  reconcile(outcome, timeline_position_value, true);
  synchronize_transport();
}

TimelinePresentation TimelinePlaybackCoordinator::update() {
  if (!snapshot_.has_value()) {
    return TimelineEndedPresentation{domain::MediaTime::zero(), domain::MediaTime::zero()};
  }

  const auto present_at = [this](domain::MediaTime position) {
    const auto outcome = resolve_clip_at(*snapshot_, snapshot_->video_track_id, position);
    // Re-resolving on every update is deliberate at this boundary: an audio
    // device re-anchor can cross a half-open segment boundary between two
    // calls, and the cached ResolvedClip must never be evaluated at the old
    // position.  enter_* keeps the steady-state path cheap and does not seek
    // again when the clip identity is unchanged.
    reconcile(outcome, position, false);
    if (std::holds_alternative<ResolvedClip>(outcome)) {
      return evaluate_clip(std::get<ResolvedClip>(outcome), position);
    }
    return *current_presentation_;
  };

  const domain::MediaTime before_audio = timeline_position();
  const auto first = present_at(before_audio);
  const bool video_ready = std::holds_alternative<TimelinePresentSample>(first) ||
                           std::holds_alternative<TimelineHoldPresentation>(first) ||
                           std::holds_alternative<TimelineGapPresentation>(first) ||
                           std::holds_alternative<TimelineMissingMediaPresentation>(first) ||
                           std::holds_alternative<TimelineSourceGapPresentation>(first) ||
                           std::holds_alternative<TimelineSourceEndedEarlyPresentation>(first) ||
                           std::holds_alternative<TimelineEndedPresentation>(first) ||
                           std::holds_alternative<TimelineSourceErrorPresentation>(first);
  if (editorial_transition_pending_ && !video_ready) {
    return first;
  }
  editorial_transition_pending_ = false;

  synchronize_audio_clock();
  const domain::MediaTime after_audio = timeline_position();
  if (after_audio != before_audio) {
    return present_at(after_audio);
  }
  return first;
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

  set_advance_permission(TimelineAdvancePermission::Hold);
  synchronize_transport();
  source_creation_error_.reset();
  const PlaybackPosition source_position{clip.source_time};  const bool reuse_session = source_session_ != nullptr && active_clip_.has_value() &&
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
  semantic_condition_ = PlaybackSemanticCondition::IntentionalSilence;
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
  semantic_condition_ = PlaybackSemanticCondition::SourceGap;
  set_advance_permission(TimelineAdvancePermission::Advance);
  synchronize_transport();
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
  semantic_condition_ = PlaybackSemanticCondition::IntentionalSilence;
  set_advance_permission(TimelineAdvancePermission::Advance);
  synchronize_transport();
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
  set_advance_permission(TimelineAdvancePermission::Hold);
  reanchor_transport(ended.duration);
  segment_kind_ = SegmentKind::Ended;
  current_presentation_ = make_ended(ended);
  semantic_condition_ = PlaybackSemanticCondition::TimelineEnded;
}

TimelinePresentation TimelinePlaybackCoordinator::evaluate_clip(const ResolvedClip& clip,
                                                                domain::MediaTime position) {
  if (source_creation_error_.has_value()) {
    set_advance_permission(TimelineAdvancePermission::Hold);
    synchronize_transport();
    segment_kind_ = SegmentKind::SourceError;
    semantic_condition_ = PlaybackSemanticCondition::Error;
    current_presentation_ = TimelineSourceErrorPresentation{
        clip.clip_id, clip.media_id, *source_creation_error_};
    return *current_presentation_;
  }
  if (source_session_ == nullptr) {
    set_advance_permission(TimelineAdvancePermission::Hold);
    synchronize_transport();
    current_presentation_ = TimelineStarvedPresentation{clip.clip_id, clip.media_id, position};
    return *current_presentation_;
  }

  const domain::MediaTime source_time = clip_source_position(clip, position);
  const PresentationDecision decision =
      source_session_->update_at(PlaybackPosition{source_time});

  if (std::holds_alternative<PresentSample>(decision)) {
    set_advance_permission(TimelineAdvancePermission::Advance);
    synchronize_transport();
    const auto& present = std::get<PresentSample>(decision);
    current_presentation_ = TimelinePresentSample{clip.clip_id, clip.media_id, position,
                                                   present.sample};
  } else if (std::holds_alternative<HoldPresentation>(decision)) {
    set_advance_permission(TimelineAdvancePermission::Advance);
    synchronize_transport();
    const auto& hold = std::get<HoldPresentation>(decision);
    current_presentation_ = TimelineHoldPresentation{clip.clip_id, clip.media_id, position,
                                                     hold.sample};
  } else if (std::holds_alternative<PresentationGap>(decision)) {
    semantic_condition_ = PlaybackSemanticCondition::SourceGap;
    set_advance_permission(TimelineAdvancePermission::Advance);
    synchronize_transport();
    current_presentation_ = TimelineSourceGapPresentation{clip.clip_id, clip.media_id, position};
  } else if (std::holds_alternative<PresentationStarved>(decision)) {
    semantic_condition_ = PlaybackSemanticCondition::Starved;
    // A video starvation is a recoverable presentation hold, not a user
    // pause.  Keep the audio device running so a transient video read gap
    // cannot tear down/recreate QAudioSink and inject audible gaps/clicks.
    // Explicit user pause still goes through pause() above.
    set_advance_permission(TimelineAdvancePermission::Hold);
    synchronize_transport();
    current_presentation_ = TimelineStarvedPresentation{clip.clip_id, clip.media_id, position};
  } else if (std::holds_alternative<PresentationEnded>(decision)) {
    semantic_condition_ = PlaybackSemanticCondition::SourceEndedEarly;
    set_advance_permission(TimelineAdvancePermission::Advance);
    synchronize_transport();
    current_presentation_ = TimelineSourceEndedEarlyPresentation{clip.clip_id, clip.media_id,
                                                                  position};
  } else {
    const auto& error = std::get<PresentationTemporalError>(decision);
    set_advance_permission(TimelineAdvancePermission::Hold);
    synchronize_transport();
    segment_kind_ = SegmentKind::SourceError;
    semantic_condition_ = PlaybackSemanticCondition::Error;
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

void TimelinePlaybackCoordinator::synchronize_audio_clock() {
  if (audio_session_ == nullptr || intent_ != PlaybackIntent::Playing || !snapshot_.has_value()) {
    return;
  }
  if (editorial_transition_pending_) {
    return;
  }
  audio_session_->update(timeline_position());
  if (audio_session_->state() == AudioPlaybackState::Ready ||
      audio_session_->state() == AudioPlaybackState::Recovering) {
    audio_session_->play();
  }
  if (audio_session_->state() == AudioPlaybackState::Running &&
      audio_session_->has_device_clock()) {
    apply_audio_condition(AudioPlaybackCondition::Present);
    clock_source_ = PlaybackClockSource::AudioDevice;
    if (const auto position = audio_session_->clock_position(); position.has_value()) {
      reanchor_transport(*position);
      synchronize_transport();
    }
  } else if (intent_ == PlaybackIntent::Playing) {
    apply_audio_condition(audio_session_->condition());
    clock_source_ = PlaybackClockSource::Software;
  }
}

void TimelinePlaybackCoordinator::apply_audio_condition(AudioPlaybackCondition condition) {
  switch (condition) {
  case AudioPlaybackCondition::Present:
    if (segment_kind_ == SegmentKind::Clip) {
      semantic_condition_ = PlaybackSemanticCondition::Present;
    }
    break;
  case AudioPlaybackCondition::IntentionalSilence:
    if (segment_kind_ == SegmentKind::Clip) {
      semantic_condition_ = PlaybackSemanticCondition::IntentionalSilence;
    }
    set_advance_permission(TimelineAdvancePermission::Advance);
    break;
  case AudioPlaybackCondition::SourceEndedEarly:
    if (segment_kind_ == SegmentKind::Clip) {
      semantic_condition_ = PlaybackSemanticCondition::SourceEndedEarly;
    }
    set_advance_permission(TimelineAdvancePermission::Advance);
    clock_source_ = PlaybackClockSource::Software;
    break;
  case AudioPlaybackCondition::Starved:
    if (segment_kind_ == SegmentKind::Clip) {
      semantic_condition_ = PlaybackSemanticCondition::Starved;
    }
    // An unavailable contributor is local to audio. Video and healthy audio
    // contributors continue on the software clock.
    clock_source_ = PlaybackClockSource::Software;
    break;
  case AudioPlaybackCondition::Recovering:
    if (segment_kind_ == SegmentKind::Clip) {
      semantic_condition_ = PlaybackSemanticCondition::Recovering;
    }
    clock_source_ = PlaybackClockSource::Software;
    break;
  case AudioPlaybackCondition::DeviceUnavailable:
    if (segment_kind_ == SegmentKind::Clip) {
      semantic_condition_ = PlaybackSemanticCondition::DeviceUnavailable;
    }
    set_advance_permission(TimelineAdvancePermission::Advance);
    clock_source_ = PlaybackClockSource::Software;
    break;
  case AudioPlaybackCondition::Error:
    if (segment_kind_ == SegmentKind::Clip) {
      semantic_condition_ = PlaybackSemanticCondition::Error;
    }
    clock_source_ = PlaybackClockSource::Software;
    break;
  case AudioPlaybackCondition::Inactive:
    if (segment_kind_ == SegmentKind::None) {
      semantic_condition_ = PlaybackSemanticCondition::Inactive;
    }
    break;
  }
  synchronize_transport();
}

} // namespace pulsar::application
