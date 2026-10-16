#include <pulsar/application/composition/visual_compositor.hpp>

#include <algorithm>
#include <cmath>
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

RgbaSurface fitted_image_nearest(const RgbaSurface& source, FitGeometry geometry) {
  validate_surface(source);
  if (geometry.scaled_width <= 0 || geometry.scaled_height <= 0) {
    throw std::invalid_argument{"invalid fitted image geometry"};
  }
  const int stride = geometry.scaled_width * 4;
  RgbaSurface output{geometry.scaled_width, geometry.scaled_height, stride,
                     std::vector<std::uint8_t>(surface_size(geometry.scaled_width, geometry.scaled_height, stride))};
  for (int y = 0; y < geometry.scaled_height; ++y) {
    const auto sy = static_cast<std::int64_t>(y) * source.height / geometry.scaled_height;
    const auto* from = source.pixels.data() + static_cast<std::size_t>(sy) * source.stride_bytes;
    auto* to = output.pixels.data() + static_cast<std::size_t>(y) * output.stride_bytes;
    if (geometry.scaled_width == source.width) {
      std::copy_n(from, static_cast<std::size_t>(source.width) * 4, to);
    } else {
      for (int x = 0; x < geometry.scaled_width; ++x) {
        const auto sx = static_cast<std::int64_t>(x) * source.width / geometry.scaled_width;
        std::copy_n(from + static_cast<std::size_t>(sx) * 4, 4, to + static_cast<std::size_t>(x) * 4);
      }
    }
  }
  return output;
}

void composite_transformed_layer(RgbaSurface& canvas, const RgbaSurface& fitted,
                                 const domain::VisualTransform& transform) {
  validate_surface(canvas);
  validate_surface(fitted);
  if (transform.opacity <= 0.0) return;
  if (transform.opacity > 1.0 || !std::isfinite(transform.opacity)) {
    throw std::invalid_argument{"invalid transformed layer opacity"};
  }
  const double scale = transform.scale <= 0.0 ? 1.0 : transform.scale;
  const double radians = transform.rotation_degrees * 3.14159265358979323846 / 180.0;
  const double cos_a = std::cos(radians);
  const double sin_a = std::sin(radians);
  const double cx = static_cast<double>(canvas.width) * 0.5;
  const double cy = static_cast<double>(canvas.height) * 0.5;
  const double ox = transform.position_x * static_cast<double>(canvas.width);
  const double oy = transform.position_y * static_cast<double>(canvas.height);
  const double fw = static_cast<double>(fitted.width);
  const double fh = static_cast<double>(fitted.height);

  // Only destination pixel centres inside the transformed picture's axis-aligned
  // bounds can contribute. Keep a one-pixel conservative fringe so the acceptance
  // and bilinear edge tests below remain the pixel authority; this changes no
  // sampling math, but avoids scanning an entire canvas for a small overlay.
  const auto transformed_x = [&](double x, double y) {
    return cx + ox + scale * (cos_a * x + sin_a * y);
  };
  const auto transformed_y = [&](double x, double y) {
    return cy + oy + scale * (-sin_a * x + cos_a * y);
  };
  const double half_w = fw * 0.5;
  const double half_h = fh * 0.5;
  const double xs[] = {transformed_x(-half_w, -half_h), transformed_x(half_w, -half_h),
                       transformed_x(-half_w, half_h), transformed_x(half_w, half_h)};
  const double ys[] = {transformed_y(-half_w, -half_h), transformed_y(half_w, -half_h),
                       transformed_y(-half_w, half_h), transformed_y(half_w, half_h)};
  const auto [min_x, max_x] = std::minmax_element(std::begin(xs), std::end(xs));
  const auto [min_y, max_y] = std::minmax_element(std::begin(ys), std::end(ys));
  const int first_x = std::clamp(static_cast<int>(std::floor(*min_x)) - 1, 0, canvas.width);
  const int last_x = std::clamp(static_cast<int>(std::ceil(*max_x)) + 1, 0, canvas.width);
  const int first_y = std::clamp(static_cast<int>(std::floor(*min_y)) - 1, 0, canvas.height);
  const int last_y = std::clamp(static_cast<int>(std::ceil(*max_y)) + 1, 0, canvas.height);
  if (first_x >= last_x || first_y >= last_y) return;

  for (int dy = first_y; dy < last_y; ++dy) {
    auto* dst_row = canvas.pixels.data() + static_cast<std::size_t>(dy) * canvas.stride_bytes;
    const double y_prime = (static_cast<double>(dy) + 0.5 - cy - oy);
    for (int dx = first_x; dx < last_x; ++dx) {
      const double x_prime = (static_cast<double>(dx) + 0.5 - cx - ox);
      // Inverse clockwise rotation, then inverse scale, back to fitted edge coords.
      const double x_local = (cos_a * x_prime - sin_a * y_prime) / scale;
      const double y_local = (sin_a * x_prime + cos_a * y_prime) / scale;
      const double ex = x_local + fw * 0.5;
      const double ey = y_local + fh * 0.5;
      // Index coords where integer = pixel centre.
      const double sx = ex - 0.5;
      const double sy = ey - 0.5;
      if (sx < -0.5 || sx > fw - 0.5 || sy < -0.5 || sy > fh - 0.5) continue;
      int x0 = static_cast<int>(std::floor(sx));
      int y0 = static_cast<int>(std::floor(sy));
      double fx = sx - static_cast<double>(x0);
      double fy = sy - static_cast<double>(y0);
      // Clamp neighbour indices; weights stay with the unclamped position so
      // edge pixels do not stretch. Outside (already skipped) stays transparent.
      int x1 = x0 + 1;
      int y1 = y0 + 1;
      const int cx0 = std::clamp(x0, 0, fitted.width - 1);
      const int cx1 = std::clamp(x1, 0, fitted.width - 1);
      const int cy0 = std::clamp(y0, 0, fitted.height - 1);
      const int cy1 = std::clamp(y1, 0, fitted.height - 1);
      if (x0 < 0) fx = 0.0;
      if (y0 < 0) fy = 0.0;
      if (x0 >= fitted.width) continue;
      if (y0 >= fitted.height) continue;
      const auto* p00 = fitted.pixels.data() + static_cast<std::size_t>(cy0) * fitted.stride_bytes +
                        static_cast<std::size_t>(cx0) * 4;
      const auto* p10 = fitted.pixels.data() + static_cast<std::size_t>(cy0) * fitted.stride_bytes +
                        static_cast<std::size_t>(cx1) * 4;
      const auto* p01 = fitted.pixels.data() + static_cast<std::size_t>(cy1) * fitted.stride_bytes +
                        static_cast<std::size_t>(cx0) * 4;
      const auto* p11 = fitted.pixels.data() + static_cast<std::size_t>(cy1) * fitted.stride_bytes +
                        static_cast<std::size_t>(cx1) * 4;
      const double w00 = (1.0 - fx) * (1.0 - fy);
      const double w10 = fx * (1.0 - fy);
      const double w01 = (1.0 - fx) * fy;
      const double w11 = fx * fy;
      double src_r = p00[0] * w00 + p10[0] * w10 + p01[0] * w01 + p11[0] * w11;
      double src_g = p00[1] * w00 + p10[1] * w10 + p01[1] * w01 + p11[1] * w11;
      double src_b = p00[2] * w00 + p10[2] * w10 + p01[2] * w01 + p11[2] * w11;
      // Decoded video is overwhelmingly opaque. Preserve the general alpha
      // oracle for graphics, but avoid four weighted alpha operations for the
      // common video quad (255 interpolates to exactly 255).
      const bool opaque_quad = p00[3] == 255 && p10[3] == 255 && p01[3] == 255 && p11[3] == 255;
      const double src_a = opaque_quad
          ? 255.0
          : p00[3] * w00 + p10[3] * w10 + p01[3] * w01 + p11[3] * w11;
      const int alpha255 = static_cast<int>(std::llround(src_a * transform.opacity));
      const int a = std::clamp(alpha255, 0, 255);
      if (a == 0) continue;
      auto* dst = dst_row + static_cast<std::size_t>(dx) * 4;
      const int inv = 255 - a;
      // Straight source-over onto the opaque canvas, integer rounding.
      const int r = static_cast<int>(std::llround(src_r)) * a + dst[0] * inv;
      const int g = static_cast<int>(std::llround(src_g)) * a + dst[1] * inv;
      const int b = static_cast<int>(std::llround(src_b)) * a + dst[2] * inv;
      dst[0] = static_cast<std::uint8_t>(std::clamp((r + 127) / 255, 0, 255));
      dst[1] = static_cast<std::uint8_t>(std::clamp((g + 127) / 255, 0, 255));
      dst[2] = static_cast<std::uint8_t>(std::clamp((b + 127) / 255, 0, 255));
      dst[3] = 255;
    }
  }
}

} // namespace pulsar::application::composition
