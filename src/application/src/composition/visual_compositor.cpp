#include <pulsar/application/composition/visual_compositor.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pulsar::application::composition {
namespace {

std::size_t surface_size(int width, int height, int stride) {
  if (width <= 0 || height <= 0 || stride < static_cast<std::int64_t>(width) * 4) {
    throw std::invalid_argument{"invalid RGBA surface geometry"};
  }
  const auto size = static_cast<std::uint64_t>(stride) * static_cast<std::uint64_t>(height);
  if (size > std::numeric_limits<std::size_t>::max()) {
    throw std::invalid_argument{"RGBA surface size overflows"};
  }
  return static_cast<std::size_t>(size);
}

void clear_black(RgbaSurface& surface) {
  for (int y = 0; y < surface.height; ++y) {
    auto* row = surface.pixels.data() + static_cast<std::size_t>(y) * surface.stride_bytes;
    std::fill_n(row, static_cast<std::size_t>(surface.width) * 4, 0);
    for (int x = 0; x < surface.width; ++x) row[static_cast<std::size_t>(x) * 4 + 3] = 255;
  }
}

} // namespace

void validate_surface(const RgbaSurface& surface) {
  if (surface.pixels.size() < surface_size(surface.width, surface.height, surface.stride_bytes)) {
    throw std::invalid_argument{"truncated RGBA surface"};
  }
}

RgbaSurface black_surface(int width, int height) {
  if (width <= 0 || width > std::numeric_limits<int>::max() / 4) {
    throw std::invalid_argument{"invalid RGBA canvas width"};
  }
  const int stride = width * 4;
  RgbaSurface output{width, height, stride, std::vector<std::uint8_t>(surface_size(width, height, stride))};
  clear_black(output);
  return output;
}

void composite_opaque_layer(RgbaSurface& canvas, const RgbaSurface& source, FitGeometry geometry) {
  validate_surface(canvas);
  validate_surface(source);
  if (&canvas == &source || geometry.offset_x < 0 || geometry.offset_y < 0 ||
      geometry.scaled_width <= 0 || geometry.scaled_height <= 0 ||
      static_cast<std::int64_t>(geometry.offset_x) + geometry.scaled_width > canvas.width ||
      static_cast<std::int64_t>(geometry.offset_y) + geometry.scaled_height > canvas.height) {
    throw std::invalid_argument{"invalid opaque layer placement"};
  }
  clear_black(canvas);
  for (int y = 0; y < geometry.scaled_height; ++y) {
    const auto sy = static_cast<std::int64_t>(y) * source.height / geometry.scaled_height;
    const auto* from = source.pixels.data() + static_cast<std::size_t>(sy) * source.stride_bytes;
    auto* to = canvas.pixels.data() + static_cast<std::size_t>(geometry.offset_y + y) * canvas.stride_bytes +
               static_cast<std::size_t>(geometry.offset_x) * 4;
    if (geometry.scaled_width == source.width) {
      std::copy_n(from, static_cast<std::size_t>(source.width) * 4, to);
    } else {
      for (int x = 0; x < geometry.scaled_width; ++x) {
        const auto sx = static_cast<std::int64_t>(x) * source.width / geometry.scaled_width;
        std::copy_n(from + static_cast<std::size_t>(sx) * 4, 4, to + static_cast<std::size_t>(x) * 4);
      }
    }
    for (int x = 0; x < geometry.scaled_width; ++x) to[static_cast<std::size_t>(x) * 4 + 3] = 255;
  }
}

RgbaSurface compose_opaque_layers(int width, int height, std::span<const RgbaSurface* const> layers) {
  auto output = black_surface(width, height);
  for (const auto* layer : layers) {
    if (layer != nullptr) {
      composite_opaque_layer(output, *layer, fit_geometry(layer->width, layer->height, width, height));
    }
  }
  return output;
}

} // namespace pulsar::application::composition
