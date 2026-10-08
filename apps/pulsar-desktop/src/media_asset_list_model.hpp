#pragma once

#include <QAbstractListModel>
#include <QString>

#include <vector>

namespace pulsar::application { class ProjectDocument; }

// Read model for the Library "Medios" tab: one row per imported media asset.
// Built from the document only when its asset set changes (never per frame).
// Built-in sounds ("pulsar-sound:" locators) are catalogue items, not imported
// media, and are not listed.
class MediaAssetListModel final : public QAbstractListModel {
  Q_OBJECT
public:
  Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
  enum Role { MediaIdRole = Qt::UserRole + 1, NameRole, KindRole, DurationRole, InfoRole, AvailableRole };

  explicit MediaAssetListModel(QObject* parent = nullptr);
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void refresh(const pulsar::application::ProjectDocument& document);
  [[nodiscard]] int count() const noexcept { return static_cast<int>(rows_.size()); }
  [[nodiscard]] int totalCount() const noexcept { return static_cast<int>(all_.size()); }
  [[nodiscard]] bool contains(const QString& media_id) const;
  // The bin view: kind "" (everything), "video", "image" (photos and colour cards) or "audio", plus a
  // case-insensitive name search. Only changes what is listed, never the project.
  Q_INVOKABLE void setFilter(const QString& kind, const QString& text);

signals:
  void totalCountChanged();

private:
  struct Row {
    QString media_id;
    QString name;
    QString kind;      // "video" | "audio"
    QString duration;  // "10.9 s" / "2:41"
    QString info;      // "Video · 10.9 s"
    bool available{true};
  };
  void applyFilter();
  std::vector<Row> all_;
  std::vector<Row> rows_;   // what the filter lets through
  QString filter_kind_;
  QString filter_text_;
};
