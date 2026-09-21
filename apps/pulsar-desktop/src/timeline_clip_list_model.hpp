#pragma once

#include <QAbstractListModel>

#include <pulsar/domain/ids/ids.hpp>

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
    MissingRole,
    SelectedRole,
    MutedRole,
    GainDbRole,
    StartProgressRole,
    EndProgressRole,
    InViewportRole,
  };

  explicit TimelineClipListModel(QObject* parent = nullptr);
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  void refresh(const pulsar::desktop::EditorSession& session,
               pulsar::domain::MediaTime view_start,
               pulsar::domain::MediaTime view_duration);

private:
  struct Row {
    std::optional<pulsar::domain::ClipId> id;
    QString key;
    QString name;
    QString start;
    QString end;
    QString duration;
    bool missing{false};
    bool selected{false};
    bool muted{false};
    double gain_db{0.0};
    double start_progress{0.0};
    double end_progress{0.0};
    bool in_viewport{false};
  };
  std::vector<Row> rows_;
};
