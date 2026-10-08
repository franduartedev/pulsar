#pragma once

#include <pulsar/application/composition/title_compositor.hpp>

#include <span>

namespace pulsar::application::composition {

// Owned RGBA8888 surface. No decoder or framework objects cross this boundary.
// E1 layers are opaque; alpha interpretation belongs to the composition operation
// so premultiplied source-over can be added without changing surface ownership.
struct RgbaSurface {
  int width{0};
  int height{0};
  int stride_bytes{0};
  std::vector<std::uint8_t> pixels;
};

void validate_surface(const RgbaSurface& surface);
[[nodiscard]] RgbaSurface black_surface(int width, int height);

// Full-frame opaque layer, including black letterbox bars. Null layers are gaps.
// Geometry comes from fit_geometry; nearest-neighbour sampling is the reference
// fallback. Adapters may supply an already-scaled source with the same geometry.
// Validates all geometry before modifying the destination. No allocations.
void composite_opaque_layer(RgbaSurface& canvas, const RgbaSurface& source,
                            FitGeometry geometry);

[[nodiscard]] RgbaSurface compose_opaque_layers(
    int width, int height, std::span<const RgbaSurface* const> layers);

} // namespace pulsar::application::composition
