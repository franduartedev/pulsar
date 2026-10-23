#include <pulsar/application/composition/title_compositor.hpp>

#include <algorithm>
#include <cstdint>

namespace pulsar::application::composition {

void blend_premultiplied_over(RgbaCanvas canvas, const TitleRaster& raster) {
  if (canvas.pixels == nullptr || raster.width <= 0 || raster.height <= 0) return;
  const int x0 = std::max(0, raster.x);
  const int y0 = std::max(0, raster.y);
  const int x1 = std::min(canvas.width, raster.x + raster.width);
  const int y1 = std::min(canvas.height, raster.y + raster.height);
  if (x0 >= x1 || y0 >= y1) return;
  for (int y = y0; y < y1; ++y) {
    const std::uint8_t* source = raster.premultiplied_rgba.data() +
                                 (static_cast<std::size_t>(y - raster.y) * raster.width + (x0 - raster.x)) * 4;
    std::uint8_t* destination = canvas.pixels + static_cast<std::size_t>(y) * canvas.stride_bytes + static_cast<std::size_t>(x0) * 4;
    for (int x = x0; x < x1; ++x, source += 4, destination += 4) {
      const unsigned alpha = source[3];
      if (alpha == 0) continue;
      const unsigned inverse = 255u - alpha;
      for (int channel = 0; channel < 3; ++channel) {
        const unsigned value = source[channel] + (destination[channel] * inverse + 127u) / 255u;
        destination[channel] = static_cast<std::uint8_t>(std::min(255u, value));
      }
      destination[3] = 255;
    }
  }
}

const domain::Title* active_title(const std::vector<domain::Title>& titles, domain::MediaTime time) {
  for (const auto& title : titles) {
    if (title.active_at(time)) return &title;
  }
  return nullptr;
}

TitleCompositor::TitleCompositor(std::shared_ptr<TitleRasterizer> rasterizer, std::size_t cache_entries)
    : rasterizer_{std::move(rasterizer)}, max_entries_{std::max<std::size_t>(1, cache_entries)} {}

bool TitleCompositor::composite(RgbaCanvas canvas, const std::vector<domain::Title>& titles,
                                domain::MediaTime time) {
  if (!rasterizer_ || canvas.pixels == nullptr || canvas.width <= 0 || canvas.height <= 0) return false;
  bool drawn = false;
  for (const auto& title : titles) {
    if (!title.active_at(time)) continue;
    const std::string key = std::to_string(title.id().value()) + "@" + std::to_string(canvas.width) + "x" +
                            std::to_string(canvas.height) + "|" + title.look_key();
    auto found = std::find_if(cache_.begin(), cache_.end(), [&](const Entry& e) { return e.key == key; });
    if (found == cache_.end()) {
      Entry entry{key, std::nullopt};
      try {
        entry.raster = rasterizer_->rasterize(title, canvas.width, canvas.height);
      } catch (...) {
        entry.raster.reset();
      }
      ++raster_count_;
      cache_.push_front(std::move(entry));
      while (cache_.size() > max_entries_) cache_.pop_back();
      found = cache_.begin();
    } else if (found != cache_.begin()) {
      cache_.splice(cache_.begin(), cache_, found);
      found = cache_.begin();
    }
    if (!found->raster.has_value()) continue;
    blend_premultiplied_over(canvas, *found->raster);
    ++blend_count_;
    drawn = true;
  }
  return drawn;
}

FitGeometry fit_geometry(int source_width, int source_height, int output_width, int output_height) {
  FitGeometry geometry;
  if (source_width <= 0 || source_height <= 0 || output_width <= 0 || output_height <= 0) return geometry;
  const bool width_limited = static_cast<std::int64_t>(output_width) * source_height <=
                             static_cast<std::int64_t>(output_height) * source_width;
  const auto scaled = [](std::int64_t numerator, std::int64_t denominator) {
    return static_cast<int>(std::max<std::int64_t>(1, (numerator + denominator / 2) / denominator));
  };
  geometry.scaled_width = width_limited ? output_width : scaled(static_cast<std::int64_t>(output_height) * source_width, source_height);
  geometry.scaled_height = width_limited ? scaled(static_cast<std::int64_t>(output_width) * source_height, source_width) : output_height;
  geometry.scaled_width = std::min(geometry.scaled_width, output_width);
  geometry.scaled_height = std::min(geometry.scaled_height, output_height);
  geometry.offset_x = (output_width - geometry.scaled_width) / 2;
  geometry.offset_y = (output_height - geometry.scaled_height) / 2;
  return geometry;
}

} // namespace pulsar::application::composition
