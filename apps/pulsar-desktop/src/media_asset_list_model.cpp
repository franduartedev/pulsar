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
  for (const auto& row : all_) if (row.media_id == media_id) return true;
  return false;
}

void MediaAssetListModel::setFilter(const QString& kind, const QString& text) {
  if (kind == filter_kind_ && text == filter_text_) return;
  filter_kind_ = kind;
  filter_text_ = text;
  beginResetModel();
  applyFilter();
  endResetModel();
}

void MediaAssetListModel::applyFilter() {
  rows_.clear();
  const auto needle = filter_text_.trimmed();
  for (const auto& row : all_) {
    // Photos and colour cards are both "image" in the filter: visual material that is not video.
    const bool kind_ok = filter_kind_.isEmpty() || row.kind == filter_kind_ ||
                         (filter_kind_ == QLatin1String("image") && row.kind == QLatin1String("color"));
    if (kind_ok && (needle.isEmpty() || row.name.contains(needle, Qt::CaseInsensitive))) rows_.push_back(row);
  }
}

void MediaAssetListModel::refresh(const pulsar::application::ProjectDocument& document) {
  std::vector<Row> next;
  next.reserve(document.asset_list().size());
  for (const auto& asset : document.asset_list()) {
    const auto locator = asset.source_locator().string();
    if (locator.rfind("pulsar-sound:", 0) == 0) continue;
    Row row;
    row.media_id = QString::number(asset.id().value());
    row.name = asset.is_color_card()
                   ? QStringLiteral("Color #%1").arg(QString::fromStdString(locator.substr(std::string{"pulsar-color:"}.size())).toUpper())
                   : QString::fromStdString(asset.source_locator().filename().string());
    row.kind = asset.is_color_card() ? QStringLiteral("color")
               : asset.is_still()    ? QStringLiteral("image")
               : asset.has_video()   ? QStringLiteral("video")
                                     : QStringLiteral("audio");
    row.duration = asset.is_still() ? QStringLiteral("5 s") : short_duration(asset.playable_duration());
    row.available = asset.availability() == pulsar::application::MediaAvailability::Available;
    QString kind_label = asset.has_video() ? QStringLiteral("Video · ") : QStringLiteral("Audio · ");
    QString detail = row.duration;
    if (asset.is_color_card()) { kind_label = QStringLiteral("Color · "); detail = QStringLiteral("placa"); }
    else if (asset.is_still()) {
      kind_label = QStringLiteral("Imagen · ");
      detail = QStringLiteral("%1×%2").arg(asset.metadata().width).arg(asset.metadata().height);
    }
    row.info = row.available ? kind_label + detail : QStringLiteral("No disponible");
    next.push_back(std::move(row));
  }
  beginResetModel();
  const bool total_changed = all_.size() != next.size();
  all_ = std::move(next);
  applyFilter();
  endResetModel();
  if (total_changed) emit totalCountChanged();
}
