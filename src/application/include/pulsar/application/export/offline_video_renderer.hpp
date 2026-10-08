#pragma once

#include <pulsar/application/composition/title_compositor.hpp>
#include <pulsar/application/composition/visual_compositor.hpp>
#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/application/timeline/timeline_clip_resolver.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace pulsar::application::exporting {

enum class OfflinePixelFormat { Rgba8888 };

using OfflineVideoPixelBuffer = composition::RgbaSurface;

struct OfflineVideoSourceFrame {
  std::optional<domain::MediaTime> presentation_timestamp;
  std::optional<domain::MediaTime> presentation_duration;
  std::shared_ptr<const OfflineVideoPixelBuffer> image;
};

struct OfflineVideoSeekResult {
  std::optional<OfflineVideoSourceFrame> candidate;
  std::optional<OfflineVideoSourceFrame> lookahead;
  std::size_t frames_decoded_after_seek{0};
};

class OfflineVideoSource {
public:
  virtual ~OfflineVideoSource() = default;
  [[nodiscard]] virtual OfflineVideoSeekResult seek_to_presentation_timestamp(
      domain::MediaTime target) = 0;
  [[nodiscard]] virtual std::optional<OfflineVideoSourceFrame> decode_next() = 0;
};

class OfflineVideoSourceFactory {
public:
  virtual ~OfflineVideoSourceFactory() = default;
  [[nodiscard]] virtual std::unique_ptr<OfflineVideoSource> open(
      const std::filesystem::path& source_locator, int video_stream_index) = 0;
};

enum class OfflineVideoRenderErrorCode {
  SourceOpenFailed,
  SeekFailed,
  DecodeFailed,
  TimestampMissing,
  SourceEndedEarly,
  UnsupportedFrame,
  ConversionFailed,
  MissingMedia,
  InternalInvariant,
  Cancelled,
};

class OfflineVideoRenderError final : public std::runtime_error {
public:
  OfflineVideoRenderError(OfflineVideoRenderErrorCode code, std::string message)
      : std::runtime_error{std::move(message)}, code_{code} {}

  [[nodiscard]] OfflineVideoRenderErrorCode code() const noexcept { return code_; }
  void set_diagnostic_context(std::string context) { diagnostic_context_ = std::move(context); }
  [[nodiscard]] const std::string& diagnostic_context() const noexcept {
    return diagnostic_context_;
  }

private:
  OfflineVideoRenderErrorCode code_;
  std::string diagnostic_context_;
};

struct OfflineVideoFrame {
  domain::FrameNumber frame_index;
  domain::MediaTime timeline_time;
  int width;
  int height;
  int stride_bytes;
  OfflinePixelFormat pixel_format;
  std::vector<std::uint8_t> pixels;
};

struct OfflineVideoRenderMetrics {
  std::uint64_t output_frames{0};
  std::uint64_t decoder_opens{0};
  std::uint64_t seeks{0};
  std::uint64_t decoded_source_frames{0};
  std::uint64_t conversion_count{0};
  std::uint64_t peak_live_source_frames{0};
  std::uint64_t peak_active_decoders{0};
  std::uint64_t peak_active_layers{0};
  std::uint64_t scale_nanoseconds{0};
  std::uint64_t composite_nanoseconds{0};
};

// Port: high quality rescaling of an RGBA picture (implemented over libswscale by the media
// adapter). Without one the renderer falls back to its basic nearest-neighbour copy.
class OfflineFrameScaler {
public:
  virtual ~OfflineFrameScaler() = default;
  [[nodiscard]] virtual OfflineVideoPixelBuffer scale(const OfflineVideoPixelBuffer& source,
                                                      int target_width, int target_height) = 0;
};

struct OfflineVideoRenderOptions {
  OfflineFrameScaler* scaler{nullptr};              // non-owning
  composition::TitleCompositor* titles{nullptr};    // non-owning; titles of the plan's snapshot
};

class OfflineVideoRenderer final {
public:
  OfflineVideoRenderer(const ExportPlan& plan, OfflineVideoSourceFactory& source_factory,
                       OfflineVideoRenderOptions options = {},
                       std::function<bool()> is_cancelled = {});
  ~OfflineVideoRenderer();

  OfflineVideoRenderer(const OfflineVideoRenderer&) = delete;
  OfflineVideoRenderer& operator=(const OfflineVideoRenderer&) = delete;

  [[nodiscard]] OfflineVideoFrame render_frame(domain::FrameNumber frame_index);
  [[nodiscard]] const OfflineVideoRenderMetrics& metrics() const noexcept { return metrics_; }

private:
  struct ActiveSource;

  void compose_frame(composition::RgbaSurface& canvas, ActiveSource& active,
                     const OfflineVideoSourceFrame& source_frame);
  [[nodiscard]] const OfflineVideoSourceFrame& select_source_frame(
      domain::TrackId track_id, const ResolvedClip& clip, domain::MediaTime source_time);
  void open_source(domain::TrackId track_id, const ResolvedClip& clip, domain::MediaTime source_time);
  void update_live_frame_metric() noexcept;
  void check_cancelled() const;

  const ExportPlan& plan_;
  OfflineVideoSourceFactory& source_factory_;
  OfflineVideoRenderOptions options_;
  std::map<std::uint64_t, std::unique_ptr<ActiveSource>> active_sources_;
  std::function<bool()> is_cancelled_;
  std::int64_t next_frame_index_{0};
  OfflineVideoRenderMetrics metrics_;
};

} // namespace pulsar::application::exporting
