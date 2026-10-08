#include "title_list_model.hpp"

#include <cmath>
#include <cstdio>

namespace {
double seconds(pulsar::domain::MediaTime t) {
  const auto s = t.seconds();
  return static_cast<double>(s.numerator()) / static_cast<double>(s.denominator());
}
QString clock_text(pulsar::domain::MediaTime t) {
  const double value = std::max(0.0, seconds(t));
  const int whole = static_cast<int>(value);
  char buffer[32];
  std::snprintf(buffer, sizeof buffer, "%02d:%02d:%02d.%03d", whole / 3600, (whole / 60) % 60, whole % 60,
                static_cast<int>(std::lround((value - whole) * 1000.0)) % 1000);
  return QString::fromLatin1(buffer);
}
}  // namespace

int TitleListModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(rows_.size()); }

QVariant TitleListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size())) return {};
  const auto& row = rows_[static_cast<std::size_t>(index.row())];
  switch (role) {
  case KeyRole: return row.key;
  case TextRole: return row.text;
  case StartTextRole: return row.start_text;
  case EndTextRole: return row.end_text;
  case StartProgressRole: return row.start_progress;
  case EndProgressRole: return row.end_progress;
  case SelectedRole: return row.selected;
  case InViewportRole: return row.in_viewport;
  default: return {};
  }
}

QHash<int, QByteArray> TitleListModel::roleNames() const {
  return {{KeyRole, "titleKey"}, {TextRole, "titleText"}, {StartTextRole, "startText"}, {EndTextRole, "endText"},
          {StartProgressRole, "startProgress"}, {EndProgressRole, "endProgress"}, {SelectedRole, "selected"},
          {InViewportRole, "inViewport"}};
}

void TitleListModel::refresh(const std::vector<pulsar::domain::Title>& titles, std::optional<pulsar::domain::TitleId> selected,
                             pulsar::domain::MediaTime view_start, pulsar::domain::MediaTime view_duration) {
  std::vector<Row> next;
  next.reserve(titles.size());
  const double start = seconds(view_start);
  const double duration = std::max(1e-9, seconds(view_duration));
  for (const auto& title : titles) {
    Row row;
    row.key = QString::number(title.id().value());
    row.text = QString::fromUtf8(title.text().data(), static_cast<qsizetype>(title.text().size()));
    row.start_text = clock_text(title.start());
    row.end_text = clock_text(title.end());
    row.start_progress = (seconds(title.start()) - start) / duration;
    row.end_progress = (seconds(title.end()) - start) / duration;
    row.selected = selected.has_value() && *selected == title.id();
    row.in_viewport = row.end_progress > 0.0 && row.start_progress < 1.0;
    next.push_back(std::move(row));
  }
  if (next == rows_) return;
  beginResetModel();
  rows_ = std::move(next);
  endResetModel();
}
