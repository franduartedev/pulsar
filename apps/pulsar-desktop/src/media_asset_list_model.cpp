#include "media_asset_list_model.hpp"

#include <pulsar/application/project/project_document.hpp>

#include <cmath>

namespace {

QString short_duration(pulsar::domain::MediaTime time) {
  const auto seconds = time.seconds();
  const double value = static_cast<double>(seconds.numerator()) /
                       static_cast<double>(seconds.denominator());
  if (!(value >= 0.0)) return QStringLiteral("--");
  if (value < 60.0) return QStringLiteral("%1 s").arg(value, 0, 'f', 1);
  const int total = static_cast<int>(std::lround(value));
  const int hours = total / 3600;
  const int minutes = (total / 60) % 60;
  const int secs = total % 60;
  return hours > 0 ? QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0'))
                   : QStringLiteral("%1:%2").arg(minutes).arg(secs, 2, 10, QLatin1Char('0'));
}

} // namespace

MediaAssetListModel::MediaAssetListModel(QObject* parent) : QAbstractListModel{parent} {}

int MediaAssetListModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant MediaAssetListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size())) return {};
  const auto& row = rows_[static_cast<std::size_t>(index.row())];
  switch (role) {
    case MediaIdRole: return row.media_id;
    case NameRole: return row.name;
    case KindRole: return row.kind;
    case DurationRole: return row.duration;
    case InfoRole: return row.info;
    case AvailableRole: return row.available;
    default: return {};
  }
}

QHash<int, QByteArray> MediaAssetListModel::roleNames() const {
  return {{MediaIdRole, "mediaId"}, {NameRole, "name"}, {KindRole, "kind"},
          {DurationRole, "durationText"}, {InfoRole, "info"}, {AvailableRole, "available"}};
}

bool MediaAssetListModel::contains(const QString& media_id) const {
  for (const auto& row : rows_) if (row.media_id == media_id) return true;
  return false;
}

void MediaAssetListModel::refresh(const pulsar::application::ProjectDocument& document) {
  std::vector<Row> next;
  next.reserve(document.asset_list().size());
  for (const auto& asset : document.asset_list()) {
    const auto locator = asset.source_locator().string();
    if (locator.rfind("pulsar-sound:", 0) == 0) continue;
    Row row;
    row.media_id = QString::number(asset.id().value());
    row.name = QString::fromStdString(asset.source_locator().filename().string());
    row.kind = asset.has_video() ? QStringLiteral("video") : QStringLiteral("audio");
    row.duration = short_duration(asset.playable_duration());
    row.available = asset.availability() == pulsar::application::MediaAvailability::Available;
    row.info = row.available
                   ? (asset.has_video() ? QStringLiteral("Video · ") : QStringLiteral("Audio · ")) + row.duration
                   : QStringLiteral("No disponible");
    next.push_back(std::move(row));
  }
  beginResetModel();
  rows_ = std::move(next);
  endResetModel();
}
