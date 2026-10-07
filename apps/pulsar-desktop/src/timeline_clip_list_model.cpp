#include "timeline_clip_list_model.hpp"

#include "editor_session.hpp"

#include <algorithm>

TimelineClipListModel::TimelineClipListModel(QObject* parent) : QAbstractListModel{parent} {}

int TimelineClipListModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant TimelineClipListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      static_cast<std::size_t>(index.row()) >= rows_.size()) return {};
  const auto& row = rows_[static_cast<std::size_t>(index.row())];
  switch (role) {
  case ClipKeyRole: return row.key;
  case NameRole: return row.name;
  case StartRole: return row.start;
  case EndRole: return row.end;
  case DurationRole: return row.duration;
  case MissingRole: return row.missing;
  case SelectedRole: return row.selected;
  case MutedRole: return row.muted;
  case GainDbRole: return row.gain_db;
  case StartProgressRole: return row.start_progress;
  case EndProgressRole: return row.end_progress;
  case InViewportRole: return row.in_viewport;
  case TrackIndexRole: return row.track_index;
  case AudioTrackRole: return row.audio_track;
  case HasAudioRole: return row.has_audio;
  case ThumbSourceRole: return row.thumb_source;
  default: return {};
  }
}

QHash<int, QByteArray> TimelineClipListModel::roleNames() const {
  return {{ClipKeyRole, "clipKey"}, {NameRole, "clipName"}, {StartRole, "startText"},
          {EndRole, "endText"}, {DurationRole, "durationText"}, {MissingRole, "missing"},
          {SelectedRole, "selected"}, {MutedRole, "muted"}, {GainDbRole, "gainDb"},
          {StartProgressRole, "startProgress"}, {EndProgressRole, "endProgress"},
          {InViewportRole, "inViewport"}, {TrackIndexRole, "trackIndex"},
          {AudioTrackRole, "audioTrack"}, {HasAudioRole, "hasAudio"}, {ThumbSourceRole, "thumbSource"}};
}

void TimelineClipListModel::refresh(const pulsar::desktop::EditorSession& session,
                                    pulsar::domain::MediaTime view_start,
                                    pulsar::domain::MediaTime view_duration,
                                    const ThumbnailLookup& thumbnails) {
  std::vector<Row> desired;
  desired.reserve(session.timeline_views().size());
  const pulsar::desktop::TimelineViewport viewport{view_start, view_duration};
  const auto view_end = view_start + view_duration;
  for (const auto& view : session.timeline_views()) {
    const auto* clip = session.document().timeline().find_clip(view.id);
    if (!clip) continue;
    Row row;
    row.id = view.id;
    row.key = QString::fromStdString(view.key);
    row.name = QString::fromStdString(view.name);
    row.start = QString::fromStdString(view.start);
    row.end = QString::fromStdString(view.end);
    row.duration = QString::fromStdString(view.duration);
    row.missing = view.missing;
    row.selected = view.selected;
    row.muted = clip->muted();
    row.gain_db = clip->audio_gain_db();
    row.start_progress = viewport.pixel_at_unclamped(clip->timeline_range().start(), 1.0);
    row.end_progress = viewport.pixel_at_unclamped(clip->timeline_range().end(), 1.0);
    row.in_viewport = clip->timeline_range().end() > view_start &&
                      clip->timeline_range().start() < view_end;
    row.track_index = static_cast<int>(view.track_index);
    row.audio_track = view.track_kind == pulsar::domain::TrackKind::Audio;
    row.has_audio = view.has_audio;
    if (thumbnails && view.track_kind == pulsar::domain::TrackKind::Video && !view.missing) row.thumb_source = thumbnails(*clip);
    desired.push_back(std::move(row));
  }

  const bool same_order = rows_.size() == desired.size() &&
      std::equal(rows_.begin(), rows_.end(), desired.begin(),
                 [](const Row& current, const Row& next) { return current.id == next.id; });
  if (!same_order) {
    beginResetModel();
    rows_ = std::move(desired);
    endResetModel();
    return;
  }

  constexpr int roles[] = {ClipKeyRole, NameRole, StartRole, EndRole, DurationRole,
                           MissingRole, SelectedRole, MutedRole, GainDbRole,
                           StartProgressRole, EndProgressRole, InViewportRole};
  constexpr int track_roles[] = {TrackIndexRole, AudioTrackRole, HasAudioRole, ThumbSourceRole};
  for (std::size_t row_index = 0; row_index < rows_.size(); ++row_index) {
    auto& row = rows_[row_index];
    const Row& next = desired[row_index];
    bool changed = false;
    for (const int role : roles) {
      bool role_changed = false;
      switch (role) {
      case ClipKeyRole: role_changed = row.key != next.key; break;
      case NameRole: role_changed = row.name != next.name; break;
      case StartRole: role_changed = row.start != next.start; break;
      case EndRole: role_changed = row.end != next.end; break;
      case DurationRole: role_changed = row.duration != next.duration; break;
      case MissingRole: role_changed = row.missing != next.missing; break;
      case SelectedRole: role_changed = row.selected != next.selected; break;
      case MutedRole: role_changed = row.muted != next.muted; break;
      case GainDbRole: role_changed = row.gain_db != next.gain_db; break;
      case StartProgressRole: role_changed = row.start_progress != next.start_progress; break;
      case EndProgressRole: role_changed = row.end_progress != next.end_progress; break;
      case InViewportRole: role_changed = row.in_viewport != next.in_viewport; break;
      default: break;
      }
      changed = changed || role_changed;
    }
    for (const int role : track_roles) {
      const bool role_changed = role == TrackIndexRole ? row.track_index != next.track_index
                              : role == AudioTrackRole ? row.audio_track != next.audio_track
                              : role == HasAudioRole ? row.has_audio != next.has_audio
                                                     : row.thumb_source != next.thumb_source;
      changed = changed || role_changed;
    }
    if (changed) {
      row = next;
      const auto model_index = index(static_cast<int>(row_index), 0);
      emit dataChanged(model_index, model_index);
    }
  }
}
