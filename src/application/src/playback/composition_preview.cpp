#include <pulsar/application/playback/composition_preview.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

namespace pulsar::application {

class AsyncCompositionPreview::Impl {
public:
  Impl(std::unique_ptr<exporting::OfflineVideoSourceFactory> source_factory,
       std::unique_ptr<exporting::OfflineFrameScaler> frame_scaler)
      : sources{std::move(source_factory)}, scaler{std::move(frame_scaler)} {
    if (!sources) throw std::invalid_argument{"preview source factory is required"};
    worker = std::thread{[this] { run(); }};
  }
  ~Impl() {
    {
      std::lock_guard lock{mutex};
      stop.store(true);
    }
    wake.notify_one();
    worker.join();
  }
  void invalidate_locked() {
    ++epoch;
    pending.reset();
    ready.reset();
    last_requested.reset();
    wake.notify_one();
  }
  struct Request {
    std::uint64_t epoch;
    std::shared_ptr<const ProjectSnapshot> snapshot;
    domain::MediaTime time;
  };
  void run() {
    std::uint64_t rendered_epoch = 0;
    std::uint64_t observed_epoch = 0;
    std::optional<exporting::ExportPlan> plan;
    std::unique_ptr<exporting::OfflineVideoRenderer> renderer;
    while (true) {
      std::optional<Request> job;
      {
        std::unique_lock lock{mutex};
        wake.wait(lock, [this, &observed_epoch] {
          return stop.load() || pending.has_value() || epoch.load() != observed_epoch;
        });
        if (stop.load()) return;
        observed_epoch = epoch.load();
        if (!pending) {
          // Drop invalidated decoder/cache state on the worker even when the
          // new timeline is empty or the caller switched back to one track.
          lock.unlock();
          renderer.reset();
          plan.reset();
          rendered_epoch = 0;
          continue;
        }
        job = std::move(pending);
        pending.reset();
      }
      CompositionPreviewResult result{job->time, {}, {}, {}};
      try {
        if (rendered_epoch != job->epoch) {
          renderer.reset();
          plan.reset();
          plan.emplace(exporting::ExportPlan::build(*job->snapshot));
          renderer = std::make_unique<exporting::OfflineVideoRenderer>(*plan, *sources,
              exporting::OfflineVideoRenderOptions{scaler.get(), nullptr},
              [this, generation = job->epoch] { return stop.load() || epoch.load() != generation; });
          rendered_epoch = job->epoch;
        }
        auto frame = renderer->render_at(job->time);
        result.surface = std::make_shared<const composition::RgbaSurface>(composition::RgbaSurface{
            frame.width, frame.height, frame.stride_bytes, std::move(frame.pixels)});
        result.render_metrics = renderer->metrics();
      } catch (const std::exception& error) {
        result.error = error.what();
        // A failed source is not reused as if it were a completed decode state.
        renderer.reset();
        rendered_epoch = 0;
      }
      {
        std::lock_guard lock{mutex};
        if (stop.load()) return;
        if (epoch.load() != job->epoch) {
          ++stats.stale_discarded;
        } else {
          if (ready) ++stats.results_replaced;
          ready = std::move(result);
          ++stats.completed;
        }
      }
    }
  }

  std::unique_ptr<exporting::OfflineVideoSourceFactory> sources;
  std::unique_ptr<exporting::OfflineFrameScaler> scaler;
  mutable std::mutex mutex;
  std::condition_variable wake;
  std::atomic<bool> stop{false};
  std::atomic<std::uint64_t> epoch{1};
  std::shared_ptr<const ProjectSnapshot> snapshot;
  std::optional<domain::MediaTime> last_requested;
  std::optional<Request> pending;
  std::optional<CompositionPreviewResult> ready;
  CompositionPreviewMetrics stats;
  std::thread worker;
};

AsyncCompositionPreview::AsyncCompositionPreview(
    std::unique_ptr<exporting::OfflineVideoSourceFactory> sources,
    std::unique_ptr<exporting::OfflineFrameScaler> scaler)
    : impl_{std::make_unique<Impl>(std::move(sources), std::move(scaler))} {}
AsyncCompositionPreview::~AsyncCompositionPreview() = default;

void AsyncCompositionPreview::configure(ProjectSnapshot snapshot) {
  auto owned = std::make_shared<const ProjectSnapshot>(std::move(snapshot));
  std::lock_guard lock{impl_->mutex};
  impl_->invalidate_locked();
  impl_->snapshot = std::move(owned);
}
void AsyncCompositionPreview::invalidate() {
  std::lock_guard lock{impl_->mutex};
  impl_->invalidate_locked();
}
void AsyncCompositionPreview::request(domain::MediaTime position) {
  std::lock_guard lock{impl_->mutex};
  if (!impl_->snapshot) throw std::logic_error{"preview has no snapshot"};
  if (impl_->last_requested == position) return;
  if (impl_->pending) ++impl_->stats.requests_coalesced;
  impl_->pending = Impl::Request{impl_->epoch.load(), impl_->snapshot, position};
  impl_->last_requested = position;
  impl_->wake.notify_one();
}
std::optional<CompositionPreviewResult> AsyncCompositionPreview::take() {
  std::unique_lock lock{impl_->mutex, std::try_to_lock};
  if (!lock || !impl_->ready) return std::nullopt;
  auto ready = std::move(impl_->ready);
  impl_->ready.reset();
  return ready;
}
CompositionPreviewMetrics AsyncCompositionPreview::metrics() const {
  std::lock_guard lock{impl_->mutex};
  return impl_->stats;
}

} // namespace pulsar::application
