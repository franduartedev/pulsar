#pragma once

#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/timeline/title.hpp>

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pulsar::application::composition {

// A writable RGBA8888 canvas (alpha stays opaque). Non-owning.
struct RgbaCanvas {
  int width{0};
  int height{0};
  int stride_bytes{0};
  std::uint8_t* pixels{nullptr};
};

// A rasterised title: PREMULTIPLIED RGBA8888 pixels, positioned on the canvas it was made for.
struct TitleRaster {
  int x{0};
  int y{0};
  int width{0};
  int height{0};
  std::vector<std::uint8_t> premultiplied_rgba;  // width * height * 4, tightly packed
};

// Port: turns a title into pixels for a canvas of a given size. The Qt/font implementation
// lives in the desktop adapter; preview and export use the SAME implementation and the same
// compositor below, so what you see is what is exported.
class TitleRasterizer {
public:
  virtual ~TitleRasterizer() = default;
  [[nodiscard]] virtual std::optional<TitleRaster> rasterize(const domain::Title& title,
                                                             int canvas_width, int canvas_height) = 0;
};

// Source-over of a premultiplied raster onto an opaque canvas, clipped to the canvas.
// Integer arithmetic only: deterministic, allocation-free.
void blend_premultiplied_over(RgbaCanvas canvas, const TitleRaster& raster);

// The title on screen at `time` (titles never overlap), or nullptr.
[[nodiscard]] const domain::Title* active_title(const std::vector<domain::Title>& titles,
                                                domain::MediaTime time);

// Rasterises once per (title look, canvas size) and blends per frame.
class TitleCompositor {
public:
  explicit TitleCompositor(std::shared_ptr<TitleRasterizer> rasterizer, std::size_t cache_entries = 8);

  // Draws the active title (if any) onto the canvas. Returns whether something was drawn.
  bool composite(RgbaCanvas canvas, const std::vector<domain::Title>& titles, domain::MediaTime time);

  [[nodiscard]] std::uint64_t raster_count() const noexcept { return raster_count_; }
  [[nodiscard]] std::uint64_t blend_count() const noexcept { return blend_count_; }
  void clear() { cache_.clear(); }

private:
  struct Entry {
    std::string key;
    std::optional<TitleRaster> raster;  // nullopt: rasterisation failed (cached too)
  };
  std::shared_ptr<TitleRasterizer> rasterizer_;
  std::size_t max_entries_;
  std::list<Entry> cache_;  // most recent first
  std::uint64_t raster_count_{0};
  std::uint64_t blend_count_{0};
};

// ---- Letterbox geometry shared by export and preview -----------------------------------
struct FitGeometry {
  int scaled_width{0};
  int scaled_height{0};
  int offset_x{0};
  int offset_y{0};
};
// Aspect-preserving fit of a source into an output canvas (bars where the aspect differs).
[[nodiscard]] FitGeometry fit_geometry(int source_width, int source_height, int output_width,
                                       int output_height);

} // namespace pulsar::application::composition
