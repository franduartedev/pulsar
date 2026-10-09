#pragma once

#include <pulsar/application/export/offline_video_renderer.hpp>

#include <memory>
#include <optional>
#include <string>

namespace pulsar::application {

struct CompositionPreviewResult {
  domain::MediaTime timeline_time;
  std::shared_ptr<const composition::RgbaSurface> surface;
  std::optional<std::string> error;
  exporting::OfflineVideoRenderMetrics render_metrics;
};

struct CompositionPreviewMetrics {
  std::uint64_t completed{0};
  std::uint64_t stale_discarded{0};
  std::uint64_t requests_coalesced{0};
  std::uint64_t results_replaced{0};
};

// Control-thread port. configure/invalidate discard all pre-discontinuity results.
// One final surface per exact requested time; no partially resolved stack escapes.
class CompositionPreview {
public:
  virtual ~CompositionPreview() = default;
  virtual void configure(ProjectSnapshot snapshot) = 0;
  virtual void invalidate() = 0;
  virtual void request(domain::MediaTime position) = 0;
  [[nodiscard]] virtual std::optional<CompositionPreviewResult> take() = 0;
  [[nodiscard]] virtual CompositionPreviewMetrics metrics() const = 0;
};

// One persistent worker, one pending request, one completed-result mailbox.
// The owned adapters and their decoder/scaler state are used only by the worker.
// Cancellation is epoch-based, not clock-tick-based: a slow coherent frame may
// finish during ordinary playback; a seek/edit can never publish stale results.
class AsyncCompositionPreview final : public CompositionPreview {
public:
  AsyncCompositionPreview(std::unique_ptr<exporting::OfflineVideoSourceFactory> sources,
                          std::unique_ptr<exporting::OfflineFrameScaler> scaler = {});
  ~AsyncCompositionPreview() override;
  void configure(ProjectSnapshot snapshot) override;
  void invalidate() override;
  void request(domain::MediaTime position) override;
  [[nodiscard]] std::optional<CompositionPreviewResult> take() override;
  [[nodiscard]] CompositionPreviewMetrics metrics() const override;
private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace pulsar::application
