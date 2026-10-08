#pragma once

#include <QAbstractListModel>

#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/timeline/title.hpp>

#include <optional>
#include <vector>

// Read model of the title track for the timeline (positions are fractions of the visible view).
class TitleListModel final : public QAbstractListModel {
  Q_OBJECT
public:
  enum Role { KeyRole = Qt::UserRole + 1, TextRole, StartTextRole, EndTextRole, StartProgressRole, EndProgressRole,
              SelectedRole, InViewportRole };

  explicit TitleListModel(QObject* parent = nullptr) : QAbstractListModel{parent} {}
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void refresh(const std::vector<pulsar::domain::Title>& titles, std::optional<pulsar::domain::TitleId> selected,
               pulsar::domain::MediaTime view_start, pulsar::domain::MediaTime view_duration);

private:
  struct Row {
    QString key, text, start_text, end_text;
    double start_progress{0}, end_progress{0};
    bool selected{false}, in_viewport{false};
    bool operator==(const Row&) const = default;
  };
  std::vector<Row> rows_;
};
