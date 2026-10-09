#include <pulsar/application/playback/composition_preview.hpp>
#include <pulsar/application/playback/timeline_playback_coordinator.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <thread>

using namespace pulsar;
namespace ex = application::exporting;
domain::MediaTime t(int n) { return {n, {1, 1}}; }

application::ProjectSnapshot snapshot() {
  auto value = application::ProjectDocument{}.snapshot();
  value.assets.emplace_back(domain::MediaId{1}, "picture", 0, t(10),
      application::VideoAssetMetadata{"test", 2, 2, {1, 1}, domain::FrameRate{1, 1}, std::nullopt});
  value.timeline.insert_clip(value.video_track_id,
      domain::Clip{domain::ClipId{1}, domain::MediaId{1}, {t(0), t(10)}, t(0)});
  value.timeline.add_track(domain::Track{domain::TrackId{9}, domain::TrackKind::Video});
  value.timeline.insert_clip(domain::TrackId{9},
      domain::Clip{domain::ClipId{9}, domain::MediaId{1}, {t(2), t(10)}, t(0)});
  value.visual_track_ids.push_back(domain::TrackId{9});
  return value;
}

struct Gate {
  std::promise<void> started;
  std::promise<void> release;
  std::shared_future<void> released{release.get_future()};
  std::atomic<int> opens{0};
  std::atomic<int> live_sources{0};
};
struct Source final : ex::OfflineVideoSource {
  std::shared_ptr<Gate> gate;
  explicit Source(std::shared_ptr<Gate> value) : gate{std::move(value)} { ++gate->live_sources; }
  ~Source() override { --gate->live_sources; }
  ex::OfflineVideoSeekResult seek_to_presentation_timestamp(domain::MediaTime target) override {
    auto surface = application::composition::black_surface(2, 2);
    surface.pixels[0] = static_cast<std::uint8_t>(static_cast<double>(target) * 10);
    return {ex::OfflineVideoSourceFrame{target, t(1),
        std::make_shared<const ex::OfflineVideoPixelBuffer>(std::move(surface))}, std::nullopt, 1};
  }
  std::optional<ex::OfflineVideoSourceFrame> decode_next() override { return std::nullopt; }
};
struct Factory final : ex::OfflineVideoSourceFactory {
  std::shared_ptr<Gate> gate;
  explicit Factory(std::shared_ptr<Gate> value) : gate{std::move(value)} {}
  std::unique_ptr<ex::OfflineVideoSource> open(const std::filesystem::path&, int) override {
    if (gate->opens.fetch_add(1) == 0) {
      gate->started.set_value();
      gate->released.wait();
    }
    return std::make_unique<Source>(gate);
  }
};

application::CompositionPreviewResult await(application::CompositionPreview& preview) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
  while (std::chrono::steady_clock::now() < deadline) {
    if (auto result = preview.take()) return std::move(*result);
    std::this_thread::yield();
  }
  throw std::runtime_error{"preview did not complete"};
}

int main() {
  auto gate = std::make_shared<Gate>();
  auto started = gate->started.get_future();
  application::AsyncCompositionPreview preview{std::make_unique<Factory>(gate)};
  preview.configure(snapshot());
  preview.request(t(0));
  assert(started.wait_for(std::chrono::seconds{10}) == std::future_status::ready);
  // Real decode is blocked, but editorial changes and request replacement remain responsive.
  preview.invalidate();
  preview.request(t(4));
  preview.configure(snapshot());
  preview.request(t(2));
  preview.request(t(3));
  preview.request(t(1));
  assert(!preview.take());
  gate->release.set_value();
  auto result = await(preview);
  assert(!result.error && result.timeline_time == t(1));
  // Upper clip uses source T+2. No frame from the obsolete T=0/2/3/4 can escape.
  assert(result.surface && result.surface->pixels[0] == 30);
  assert(result.render_metrics.peak_active_decoders == 2);
  assert(preview.metrics().stale_discarded == 1);
  assert(preview.metrics().requests_coalesced >= 2);
  // Backward seeks invalidate decoder state and the completed mailbox as one epoch.
  preview.invalidate();
  preview.request(t(0));
  result = await(preview);
  assert(!result.error && result.timeline_time == t(0) && result.surface->pixels[0] == 20);
  // Paused ticks at the same timestamp do not enqueue duplicate conversions.
  const auto completed = preview.metrics().completed;
  for (int i = 0; i < 100; ++i) preview.request(t(0));
  assert(!preview.take() && preview.metrics().completed == completed);
  assert(gate->live_sources == 2);
  preview.invalidate();
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
  while (gate->live_sources != 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  assert(gate->live_sources == 0); // invalidation frees decoders even without another request
  // A failed snapshot produces an explicit result, never a partial composition.
  auto missing = snapshot();
  missing.assets.clear();
  preview.configure(std::move(missing));
  preview.request(t(0));
  result = await(preview);
  assert(result.error && !result.surface);
}
