#include "preview_image_provider.hpp"

#include <QMutexLocker>

PreviewImageProvider::PreviewImageProvider()
    : QQuickImageProvider{QQuickImageProvider::Image} {}

QImage PreviewImageProvider::requestImage(const QString&, QSize* size,
                                          const QSize& requestedSize) {
  QMutexLocker lock{&mutex_};
  QImage result = image_;
  if (size != nullptr) {
    *size = result.size();
  }
  if (!result.isNull() && requestedSize.isValid()) {
    result = result.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }
  return result;
}

void PreviewImageProvider::setImage(QImage image) {
  QMutexLocker lock{&mutex_};
  image_ = std::move(image);
}

void PreviewImageProvider::clear() {
  QMutexLocker lock{&mutex_};
  image_ = QImage{};
}

QImage PreviewImageProvider::image() const {
  QMutexLocker lock{&mutex_};
  return image_;
}
