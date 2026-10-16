#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

class PreviewImageProvider final : public QQuickImageProvider {
public:
  PreviewImageProvider();

  [[nodiscard]] QImage requestImage(const QString& id, QSize* size,
                                    const QSize& requestedSize) override;
  void setImage(QImage image);
  void clear();
  [[nodiscard]] QImage image() const;

private:
  mutable QMutex mutex_;
  QImage image_;
};
