#pragma once

#include <pulsar/application/composition/title_compositor.hpp>
#include <pulsar/domain/timeline/visual_transform.hpp>

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

// Transformed layer reference (E2.1): `fitted` is the aspect-preserving fit of
// the source (E1 fit_geometry, scaler or nearest) WITHOUT letterbox bars.
// The transform is applied around the canvas centre: uniform `scale`, clockwise
// `rotation_degrees`, then `position_x/y` offsets in canvas fractions. Opacity
// is source-over coverage in [0,1]. Sampling is deterministic bilinear; outside
// the transformed picture the canvas is untouched (transparent). Source alpha,
// when present, multiplies opacity; the canvas stays opaque. Premultiplied
// source-over convergence: straight RGB * effective alpha over opaque dst with
// integer rounding, deterministic across preview/export.
void composite_transformed_layer(RgbaSurface& canvas, const RgbaSurface& fitted,
                                 const domain::VisualTransform& transform);

// Nearest-neighbour fit without bars, for the transformed reference path when
// no high-quality scaler is supplied. Deterministic.
[[nodiscard]] RgbaSurface fitted_image_nearest(const RgbaSurface& source, FitGeometry geometry);

[[nodiscard]] RgbaSurface compose_opaque_layers(
    int width, int height, std::span<const RgbaSurface* const> layers);

} // namespace pulsar::application::composition
