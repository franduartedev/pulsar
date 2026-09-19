#include <pulsar/application/waveform/waveform_cache.hpp>

#include <deque>
#include <exception>
#include <unordered_map>

namespace pulsar::application {

class WaveformCacheRequest::Impl {
public:
  mutable std::mutex mutex;
  mutable std::condition_variable condition;
  std::unique_ptr<WaveformCacheBuilder> builder;
  std::optional<WaveformCacheReader> reader;
  WaveformProgress progress;
  WaveformCacheKey key;
  std::filesystem::path destination;
  std::function<bool()> cancelled;
  bool cancellation_requested{false};
  bool terminal{false};
};

WaveformCacheRequest::WaveformCacheRequest(std::shared_ptr<Impl> impl) : impl_{std::move(impl)} {}
WaveformCacheRequest::~WaveformCacheRequest() = default;

WaveformProgress WaveformCacheRequest::progress() const {
  std::lock_guard lock{impl_->mutex};
  return impl_->progress;
}

std::optional<WaveformSlice> WaveformCacheRequest::query_partial(const WaveformQuery& query) const {
  std::lock_guard lock{impl_->mutex};
  if (impl_->reader) return impl_->reader->query(query);
  if (impl_->builder && impl_->progress.status == WaveformCacheStatus::Partial) return impl_->builder->query_partial(query);
  return std::nullopt;
}

bool WaveformCacheRequest::wait_for(std::chrono::milliseconds timeout) const {
  std::unique_lock lock{impl_->mutex};
  return impl_->condition.wait_for(lock, timeout, [this] { return impl_->terminal; });
}

void WaveformCacheRequest::cancel() const noexcept {
  std::lock_guard lock{impl_->mutex};
  impl_->cancellation_requested = true;
  impl_->condition.notify_all();
}

class WaveformCacheManager::Impl {
public:
  struct Job { std::shared_ptr<WaveformCacheRequest::Impl> request; WaveformCacheManager::SourceFactory factory; };

  explicit Impl(std::size_t maximum_active_jobs) {
    workers.reserve(maximum_active_jobs);
    for (std::size_t i = 0; i < maximum_active_jobs; ++i) workers.emplace_back([this] { worker_loop(); });
  }

  ~Impl() { shutdown(); }

  void shutdown() noexcept {
    {
      std::lock_guard lock{mutex};
      if (stopping) return;
      stopping = true;
      for (auto& [key, request] : registry) request_cancel(*request);
    }
    wake.notify_all();
    for (auto& worker : workers) if (worker.joinable()) worker.join();
    workers.clear();
  }

  std::shared_ptr<WaveformCacheRequest::Impl> find_or_create(
      std::filesystem::path directory, WaveformCacheKey key, WaveformCacheManager::SourceFactory factory,
      std::optional<std::uint64_t> total_frames) {
    const auto identity = key.stable_text();
    {
      std::lock_guard lock{mutex};
      if (stopping) throw std::logic_error{"waveform cache manager is shut down"};
      if (const auto found = registry.find(identity); found != registry.end()) {
        std::lock_guard request_lock{found->second->mutex};
        if (found->second->progress.status != WaveformCacheStatus::Failed) return found->second;
        registry.erase(found);
      }
    }
    const auto destination = waveform_cache_path(directory, key);
    if (auto reader = WaveformCacheReader::open(destination, key)) {
      auto request = std::make_shared<WaveformCacheRequest::Impl>();
      request->key = key; request->destination = destination; request->reader = std::move(reader);
      request->progress = {WaveformCacheStatus::Ready, request->reader->source_frames(), request->reader->source_frames(), request->reader->level_count(), {}};
      request->terminal = true;
      std::lock_guard lock{mutex};
      auto [it, inserted] = registry.emplace(identity, request);
      return inserted ? request : it->second;
    }
    auto request = std::make_shared<WaveformCacheRequest::Impl>();
    request->key = key; request->destination = destination;
    request->progress.status = WaveformCacheStatus::Missing; request->progress.total_frames = total_frames;
    std::lock_guard lock{mutex};
    auto [it, inserted] = registry.emplace(identity, request);
    if (inserted) pending.push_back(Job{request, std::move(factory)});
    if (inserted) wake.notify_one();
    return inserted ? request : it->second;
  }

private:
  static void request_cancel(WaveformCacheRequest::Impl& request) noexcept {
    std::lock_guard lock{request.mutex}; request.cancellation_requested = true;
  }

  void worker_loop() noexcept {
    for (;;) {
      Job job;
      {
        std::unique_lock lock{mutex};
        wake.wait(lock, [this] { return stopping || !pending.empty(); });
        if (pending.empty()) { if (stopping) return; continue; }
        job = std::move(pending.front()); pending.pop_front();
      }
      run_job(std::move(job));
    }
  }

  void run_job(Job job) noexcept {
    const auto request = job.request;
    try {
      {
        std::lock_guard lock{request->mutex};
        if (request->cancellation_requested) { request->progress.status = WaveformCacheStatus::Failed; request->progress.error = "cancelled"; request->terminal = true; request->condition.notify_all(); return; }
        request->progress.status = WaveformCacheStatus::Generating;
        request->builder = std::make_unique<WaveformCacheBuilder>(request->key, request->destination);
      }
      auto source = job.factory();
      if (!source) throw std::runtime_error{"waveform source factory returned null"};
      const auto info = source->info();
      if (info.sample_rate != request->key.sample_rate || info.channel_count != request->key.channel_count || info.time_base != request->key.time_base || info.stream_origin != request->key.stream_origin) throw std::runtime_error{"waveform source metadata changed"};
      for (;;) {
        {
          std::lock_guard lock{request->mutex};
          if (request->cancellation_requested) { request->builder->cancel(); throw std::runtime_error{"cancelled"}; }
        }
        auto block = source->next_block();
        if (!block) break;
        {
          std::lock_guard lock{request->mutex};
          request->builder->consume(*block);
          request->progress.status = WaveformCacheStatus::Partial;
          request->progress.generated_frames = request->builder->report().source_frames;
          request->progress.available_levels = request->builder->report().level_count;
        }
        request->condition.notify_all();
      }
      {
        std::lock_guard lock{request->mutex};
        if (request->cancellation_requested) throw std::runtime_error{"cancelled"};
      }
      WaveformBuildReport report;
      {
        std::lock_guard lock{request->mutex};
        report = request->builder->finish();
        request->progress.generated_frames = report.source_frames;
        request->progress.available_levels = report.level_count;
      }
      auto reader = WaveformCacheReader::open(request->destination, request->key);
      if (!reader) throw std::runtime_error{"published waveform cache failed validation"};
      {
        std::lock_guard lock{request->mutex};
        request->reader = std::move(reader);
        request->progress.status = WaveformCacheStatus::Ready;
        request->progress.total_frames = report.source_frames;
        request->terminal = true;
        request->builder.reset();
      }
      request->condition.notify_all();
    } catch (const std::exception& error) {
      std::lock_guard lock{request->mutex};
      if (request->builder) request->builder->cancel();
      request->progress.status = WaveformCacheStatus::Failed; request->progress.error = error.what(); request->terminal = true; request->condition.notify_all();
    } catch (...) {
      std::lock_guard lock{request->mutex};
      if (request->builder) request->builder->cancel();
      request->progress.status = WaveformCacheStatus::Failed; request->progress.error = "unknown waveform generation failure"; request->terminal = true; request->condition.notify_all();
    }
  }

  std::mutex mutex;
  std::condition_variable wake;
  std::deque<Job> pending;
  std::unordered_map<std::string, std::shared_ptr<WaveformCacheRequest::Impl>> registry;
  std::vector<std::thread> workers;
  bool stopping{false};
};

WaveformCacheManager::WaveformCacheManager(std::size_t maximum_active_jobs)
    : maximum_active_jobs_{std::max<std::size_t>(1, maximum_active_jobs)}, impl_{std::make_unique<Impl>(maximum_active_jobs_)} {}
WaveformCacheManager::~WaveformCacheManager() { shutdown(); }

std::shared_ptr<WaveformCacheRequest> WaveformCacheManager::request(
    std::filesystem::path cache_directory, WaveformCacheKey key, SourceFactory source_factory,
    std::optional<std::uint64_t> total_frames) {
  if (!source_factory) throw std::invalid_argument{"waveform source factory is empty"};
  auto impl = impl_->find_or_create(std::move(cache_directory), key, std::move(source_factory), total_frames);
  auto request = std::shared_ptr<WaveformCacheRequest>{new WaveformCacheRequest{std::move(impl)}};
  return request;
}

void WaveformCacheManager::shutdown() noexcept { impl_->shutdown(); }

} // namespace pulsar::application
