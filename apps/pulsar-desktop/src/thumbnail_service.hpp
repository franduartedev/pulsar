#pragma once

#include <QImage>
#include <QObject>
#include <QQuickImageProvider>
#include <QString>

#include <pulsar/domain/time/media_time.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// One representative picture per video clip: decoration only. Nothing here may ever affect
// editing, playback or export; every failure just means "no picture".

// Thread-safe, bounded LRU of small images. Budget: kMaxEntries * 160x90x4 bytes (~57.6 KB each)
// = about 11 MB at the default of 192 entries, however many clips the project has.
class ThumbnailCache {
public:
  static constexpr int kWidth = 160;
  static constexpr int kHeight = 90;
  explicit ThumbnailCache(std::size_t max_entries = 192) : max_entries_{max_entries} {}

  [[nodiscard]] bool contains(const std::string& key) const;
  [[nodiscard]] QImage get(const std::string& key);  // null when absent; refreshes recency
  void put(const std::string& key, QImage image);    // evicts the least recently used beyond the cap
  void clear();
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] std::size_t bytes() const;

private:
  struct Entry { QImage image; std::list<std::string>::iterator position; };
  mutable std::mutex mutex_;
  std::size_t max_entries_;
  std::list<std::string> recency_;  // front = most recent
  std::unordered_map<std::string, Entry> entries_;
};

// "image://pulsar-thumb/<key>". Holds the cache by shared ownership so a late request from the
// QML image loader can never touch a destroyed controller.
class ThumbnailImageProvider final : public QQuickImageProvider {
public:
  explicit ThumbnailImageProvider(std::shared_ptr<ThumbnailCache> cache)
      : QQuickImageProvider(QQuickImageProvider::Image), cache_{std::move(cache)} {}
  QImage requestImage(const QString& id, QSize* size, const QSize& requested) override;

private:
  std::shared_ptr<ThumbnailCache> cache_;
};

struct ThumbnailRequest {
  std::string key;
  std::filesystem::path path;  // runtime path of the media
  int video_stream{0};
  pulsar::domain::MediaTime position{pulsar::domain::MediaTime::zero()};  // source time of the representative frame
};

// ONE worker thread (own decoder, never shared with playback) draining a bounded queue.
// Destruction cancels, wakes and joins the worker before the QObject goes away, so no signal
// can be delivered to a dead object.
class ThumbnailService final : public QObject {
  Q_OBJECT
public:
  static constexpr std::size_t kMaxPending = 512;
  explicit ThumbnailService(std::shared_ptr<ThumbnailCache> cache, QObject* parent = nullptr);
  ~ThumbnailService() override;

  // False when the queue is full (the caller simply asks again on a later refresh).
  bool request(ThumbnailRequest request);
  void cancelPending();   // drops queued work (project switch); a running decode finishes
  void shutdown();        // idempotent: cancel + join

signals:
  void finished(const QString& key, bool ok);  // queued to the owner's thread

private:
  void run();
  std::shared_ptr<ThumbnailCache> cache_;
  std::mutex mutex_;
  std::condition_variable wake_;
  std::deque<ThumbnailRequest> queue_;
  bool stop_{false};
  std::thread thread_;
};
