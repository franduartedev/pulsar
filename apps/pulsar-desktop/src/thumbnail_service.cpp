#include "thumbnail_service.hpp"

#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>

#include <QMetaObject>

#include <algorithm>
#include <exception>
#include <optional>

bool ThumbnailCache::contains(const std::string& key) const {
  std::lock_guard lock{mutex_};
  return entries_.find(key) != entries_.end();
}

QImage ThumbnailCache::get(const std::string& key) {
  std::lock_guard lock{mutex_};
  const auto found = entries_.find(key);
  if (found == entries_.end()) return {};
  recency_.splice(recency_.begin(), recency_, found->second.position);
  return found->second.image;
}

void ThumbnailCache::put(const std::string& key, QImage image) {
  std::lock_guard lock{mutex_};
  if (max_entries_ == 0) return;
  if (const auto found = entries_.find(key); found != entries_.end()) {
    found->second.image = std::move(image);
    recency_.splice(recency_.begin(), recency_, found->second.position);
    return;
  }
  recency_.push_front(key);
  entries_.emplace(key, Entry{std::move(image), recency_.begin()});
  while (entries_.size() > max_entries_) {
    entries_.erase(recency_.back());
    recency_.pop_back();
  }
}

void ThumbnailCache::clear() {
  std::lock_guard lock{mutex_};
  entries_.clear();
  recency_.clear();
}

std::size_t ThumbnailCache::size() const {
  std::lock_guard lock{mutex_};
  return entries_.size();
}

std::size_t ThumbnailCache::bytes() const {
  std::lock_guard lock{mutex_};
  std::size_t total = 0;
  for (const auto& [key, entry] : entries_) total += static_cast<std::size_t>(entry.image.sizeInBytes());
  return total;
}

QImage ThumbnailImageProvider::requestImage(const QString& id, QSize* size, const QSize&) {
  QImage image = cache_ ? cache_->get(id.toStdString()) : QImage{};
  if (size != nullptr) *size = image.size();
  return image;
}

ThumbnailService::ThumbnailService(std::shared_ptr<ThumbnailCache> cache, QObject* parent)
    : QObject{parent}, cache_{std::move(cache)} {
  thread_ = std::thread{[this] { run(); }};
}

ThumbnailService::~ThumbnailService() { shutdown(); }

bool ThumbnailService::request(ThumbnailRequest request) {
  {
    std::lock_guard lock{mutex_};
    if (stop_ || queue_.size() >= kMaxPending) return false;
    queue_.push_back(std::move(request));
  }
  wake_.notify_one();
  return true;
}

void ThumbnailService::cancelPending() {
  std::lock_guard lock{mutex_};
  queue_.clear();
}

void ThumbnailService::shutdown() {
  {
    std::lock_guard lock{mutex_};
    stop_ = true;
    queue_.clear();
  }
  wake_.notify_all();
  if (thread_.joinable()) thread_.join();
}

namespace {
// Decodes the frame at/after `position` with a private decoder and shrinks it to the cache size.
std::optional<QImage> decode_thumbnail(const ThumbnailRequest& request) {
  using namespace pulsar::media::ffmpeg;
  FFmpegVideoDecodeSession session{request.path, request.video_stream};
  auto seek = session.seek_to_presentation_timestamp_visual(request.position);
  std::optional<DecodedVisualVideoFrame> frame = std::move(seek.frame);
  if (!frame) frame = std::move(seek.lookahead);
  if (!frame) frame = session.decode_next_visual();
  if (!frame) return std::nullopt;
  const auto converted = FFmpegPreviewConverter{}.convert(*frame);
  if (converted.width <= 0 || converted.height <= 0) return std::nullopt;
  const QImage wrapped{converted.pixels.data(), converted.width, converted.height, converted.stride_bytes,
                       QImage::Format_RGBA8888};
  return wrapped.scaled(ThumbnailCache::kWidth, ThumbnailCache::kHeight, Qt::KeepAspectRatio,
                        Qt::SmoothTransformation)
      .convertToFormat(QImage::Format_RGBA8888);  // detaches from the temporary pixel buffer
}
}  // namespace

void ThumbnailService::run() {
  for (;;) {
    ThumbnailRequest request;
    {
      std::unique_lock lock{mutex_};
      wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
      if (stop_) return;
      request = std::move(queue_.front());
      queue_.pop_front();
    }
    bool ok = false;
    try {
      if (cache_->contains(request.key)) {
        ok = true;
      } else if (auto image = decode_thumbnail(request); image && !image->isNull()) {
        cache_->put(request.key, std::move(*image));
        ok = true;
      }
    } catch (...) {
      ok = false;  // decoration only: a bad file just has no picture
    }
    {
      std::lock_guard lock{mutex_};
      if (stop_) return;  // never signal while shutting down
    }
    QMetaObject::invokeMethod(this, [this, key = QString::fromStdString(request.key), ok] { emit finished(key, ok); },
                              Qt::QueuedConnection);
  }
}
