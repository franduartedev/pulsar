#pragma once

#include <pulsar/application/playback/playback_session.hpp>
#include <pulsar/application/timeline/timeline_clip_resolver.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace pulsar::application {

struct TimelinePresentSample {
  domain::ClipId clip_id;
  domain::MediaId media_id;
  domain::MediaTime timeline_position;
  VideoSample sample;
};

struct TimelineHoldPresentation {
  domain::ClipId clip_id;
  domain::MediaId media_id;
  domain::MediaTime timeline_position;
  VideoSample sample;
};

struct TimelineSourceGapPresentation {
  domain::ClipId clip_id;
  domain::MediaId media_id;
  domain::MediaTime timeline_position;
};

struct TimelineGapPresentation {
  domain::MediaTime position;
};

struct TimelineMissingMediaPresentation {
  domain::ClipId clip_id;
  domain::MediaId media_id;
  domain::MediaTime position;
  domain::TimeRange timeline_range;
  std::optional<std::string> reason;
};

struct TimelineStarvedPresentation {};

struct TimelineSourceEndedEarlyPresentation {
  domain::ClipId clip_id;
  domain::MediaId media_id;
  domain::MediaTime position;
};

struct TimelineSourceErrorPresentation {
  domain::ClipId clip_id;
  domain::MediaId media_id;
  std::string message;
};

struct TimelineEndedPresentation {
  domain::MediaTime position;
  domain::MediaTime duration;
};

using TimelinePresentation = std::variant<
    TimelinePresentSample, TimelineHoldPresentation, TimelineSourceGapPresentation,
    TimelineGapPresentation, TimelineMissingMediaPresentation, TimelineStarvedPresentation,
    TimelineSourceEndedEarlyPresentation, TimelineSourceErrorPresentation,
    TimelineEndedPresentation>;

using PlaybackWorkerFactory = std::function<std::unique_ptr<PlaybackMediaWorker>(
    const std::filesystem::path& source_locator, int video_stream_index)>;

class TimelinePlaybackCoordinator {
public:
  TimelinePlaybackCoordinator(const MonotonicClock& clock, PlaybackWorkerFactory worker_factory);
  ~TimelinePlaybackCoordinator() = default;

  TimelinePlaybackCoordinator(const TimelinePlaybackCoordinator&) = delete;
  TimelinePlaybackCoordinator& operator=(const TimelinePlaybackCoordinator&) = delete;

  void set_snapshot(ProjectSnapshot snapshot);
  void play();
  void pause();
  void seek(domain::MediaTime timeline_position);

  [[nodiscard]] TimelinePresentation update();
  [[nodiscard]] domain::MediaTime timeline_position() const;
  [[nodiscard]] PlaybackIntent intent() const noexcept { return intent_; }

private:
  enum class SegmentKind {
    None,
    Clip,
    Gap,
    Missing,
    Ended,
    SourceError,
  };

  void reconcile(const TimelineResolveOutcome& outcome, domain::MediaTime position,
                 bool force_transition);
  void enter_clip(const ResolvedClip& clip, bool force_transition);
  void enter_gap(const TimelineGap& gap, bool force_transition);
  void enter_missing(const MissingClipMedia& missing, bool force_transition);
  void enter_ended(const TimelineEnded& ended, bool force_transition);
  [[nodiscard]] TimelinePresentation evaluate_clip(const ResolvedClip& clip,
                                                    domain::MediaTime position);
  [[nodiscard]] TimelinePresentation make_gap(const TimelineGap& gap) const;
  [[nodiscard]] TimelinePresentation make_missing(const MissingClipMedia& missing) const;
  [[nodiscard]] TimelinePresentation make_ended(const TimelineEnded& ended) const;
  [[nodiscard]] domain::MediaTime next_gap_boundary(domain::MediaTime position) const;
  void reset_source();

  const MonotonicClock& clock_;
  PlaybackWorkerFactory worker_factory_;
  PlaybackTransport transport_;
  std::optional<ProjectSnapshot> snapshot_;
  std::unique_ptr<PlaybackSession> source_session_;
  std::optional<ResolvedClip> active_clip_;
  std::optional<MissingClipMedia> active_missing_;
  std::optional<domain::MediaTime> segment_end_;
  std::optional<TimelinePresentation> current_presentation_;
  std::optional<std::string> source_creation_error_;
  PlaybackIntent intent_{PlaybackIntent::Paused};
  SegmentKind segment_kind_{SegmentKind::None};
};

} // namespace pulsar::application
