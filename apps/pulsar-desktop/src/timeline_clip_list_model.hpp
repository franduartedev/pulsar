#pragma once

#include <QAbstractListModel>
#include <QVariantList>

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace pulsar::desktop { class EditorSession; }

class TimelineClipListModel final : public QAbstractListModel {
  Q_OBJECT
public:
  enum Role {
    ClipKeyRole = Qt::UserRole + 1,
    NameRole,
    StartRole,
    EndRole,
    DurationRole,
    DurationSecondsRole,
    MissingRole,
    SelectedRole,
    MutedRole,
    GainDbRole,
    StartProgressRole,
    EndProgressRole,
    InViewportRole,
    TrackIndexRole,
    AudioTrackRole,
    HasAudioRole,
    ThumbSourceRole,
    KindRole,       // "video" | "image" | "color" | "audio"
    FadeInRole,     // fraction of the clip width
    FadeOutRole,
    TransitionInRole,
    TransitionStartProgressRole,
    GainKeyframesRole,
  };

  explicit TimelineClipListModel(QObject* parent = nullptr);
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  // Id of a ready thumbnail for a VIDEO-track clip ("" when there is none yet / ever).
  using ThumbnailLookup = std::function<QString(const pulsar::domain::Clip&)>;
  void refresh(const pulsar::desktop::EditorSession& session,
               pulsar::domain::MediaTime view_start,
               pulsar::domain::MediaTime view_duration,
               const ThumbnailLookup& thumbnails = {});

private:
  struct Row {
    std::optional<pulsar::domain::ClipId> id;
    QString key;
    QString name;
    QString start;
    QString end;
    QString duration;
    double duration_seconds{0.0};
    bool missing{false};
    bool selected{false};
    bool muted{false};
    double gain_db{0.0};
    double start_progress{0.0};
    double end_progress{0.0};
    bool in_viewport{false};
    int track_index{0};
    bool audio_track{false};
    bool has_audio{false};
    QString thumb_source;
    QString kind;
    double fade_in{0.0};
    double fade_out{0.0};
    bool transition_in{false};
    double transition_start_progress{0.0};
    QVariantList gain_keyframes;
  };
  std::vector<Row> rows_;
};
