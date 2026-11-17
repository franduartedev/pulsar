#include <pulsar/application/composition/title_compositor.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pulsar::application::composition {

namespace {
double smoothstep(double value) {
  value = std::clamp(value, 0.0, 1.0);
  return value * value * (3.0 - 2.0 * value);
}

double seconds(domain::MediaTime value) { return static_cast<double>(value); }

void apply_phase(TitleAnimationState& state, domain::TitleAnimation animation, double progress,
                 bool entering, int canvas_width, int canvas_height) {
  progress = smoothstep(progress);
  const double amount = entering ? progress : 1.0 - progress;
  switch (animation) {
    case domain::TitleAnimation::None: break;
    case domain::TitleAnimation::Fade: state.opacity *= amount; break;
    case domain::TitleAnimation::ScaleReveal:
      state.opacity *= amount;
      state.scale *= 0.94 + 0.06 * amount;
      break;
    case domain::TitleAnimation::SlideFromLeft:
      state.translate_x += static_cast<int>(std::lround((amount - 1.0) * canvas_width * 0.18));
      break;
    case domain::TitleAnimation::SlideFromRight:
      state.translate_x += static_cast<int>(std::lround((1.0 - amount) * canvas_width * 0.18));
      break;
    case domain::TitleAnimation::SlideFromBottom:
      state.translate_y += static_cast<int>(std::lround((1.0 - amount) * canvas_height * 0.14));
      break;
  }
}
} // namespace

TitleAnimationState evaluate_title_animation(const domain::Title& title, domain::MediaTime time,
                                             int canvas_width, int canvas_height) {
  TitleAnimationState state;
  if (!title.active_at(time)) { state.opacity = 0.0; return state; }
  const double total = seconds(title.end() - title.start());
  if (total <= 0.0) { state.opacity = 0.0; return state; }
  double in_duration = std::max(0.0, seconds(title.motion().in_duration));
  double out_duration = std::max(0.0, seconds(title.motion().out_duration));
  if (in_duration + out_duration > total) {
    const double factor = total / (in_duration + out_duration);
    in_duration *= factor; out_duration *= factor;
  }
  const double elapsed = seconds(time - title.start());
  if (in_duration > 0.0 && elapsed < in_duration) {
    apply_phase(state, title.motion().in, elapsed / in_duration, true, canvas_width, canvas_height);
  }
  const double remaining = total - elapsed;
  if (out_duration > 0.0 && remaining <= out_duration) {
    apply_phase(state, title.motion().out, remaining / out_duration, false, canvas_width, canvas_height);
  }
  return state;
}

TitleRaster transform_title_raster(const TitleRaster& raster, TitleAnimationState state) {
  state.opacity = std::clamp(state.opacity, 0.0, 1.0);
  state.scale = std::clamp(state.scale, 0.90, 1.0);
  if (state.opacity >= 1.0 && state.scale >= 1.0 && state.translate_x == 0 && state.translate_y == 0) return raster;
  TitleRaster result;
  result.width = std::max(1, static_cast<int>(std::lround(raster.width * state.scale)));
  result.height = std::max(1, static_cast<int>(std::lround(raster.height * state.scale)));
  result.x = raster.x + (raster.width - result.width) / 2 + state.translate_x;
  result.y = raster.y + (raster.height - result.height) / 2 + state.translate_y;
  result.premultiplied_rgba.resize(static_cast<std::size_t>(result.width) * result.height * 4);
  for (int y = 0; y < result.height; ++y) for (int x = 0; x < result.width; ++x) {
    const int source_x = std::min(raster.width - 1, static_cast<int>(x / state.scale));
    const int source_y = std::min(raster.height - 1, static_cast<int>(y / state.scale));
    const auto source = (static_cast<std::size_t>(source_y) * raster.width + source_x) * 4;
    const auto target = (static_cast<std::size_t>(y) * result.width + x) * 4;
    for (int channel = 0; channel < 4; ++channel) result.premultiplied_rgba[target + channel] =
        static_cast<std::uint8_t>(std::lround(raster.premultiplied_rgba[source + channel] * state.opacity));
  }
  return result;
}

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
    const auto state = evaluate_title_animation(title, time, canvas.width, canvas.height);
    if (state.opacity <= 0.0) continue;
    const auto transformed = transform_title_raster(*found->raster, state);
    blend_premultiplied_over(canvas, transformed);
    ++transform_count_;
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

FitGeometry cover_geometry(int source_width, int source_height, int output_width, int output_height) {
  FitGeometry geometry;
  if (source_width <= 0 || source_height <= 0 || output_width <= 0 || output_height <= 0) return geometry;
  const auto scaled = [](std::int64_t numerator, std::int64_t denominator) {
    return static_cast<int>(std::max<std::int64_t>(1, (numerator + denominator / 2) / denominator));
  };
  // Cover uses the limiting dimension like contain, but crops is now the larger
  // scale: compare the source against the output the other way around.
  const bool width_limited = static_cast<std::int64_t>(output_width) * source_height >=
                             static_cast<std::int64_t>(output_height) * source_width;
  geometry.scaled_width = width_limited
      ? output_width
      : scaled(static_cast<std::int64_t>(output_height) * source_width, source_height);
  geometry.scaled_height = width_limited
      ? scaled(static_cast<std::int64_t>(output_width) * source_height, source_width)
      : output_height;
  geometry.scaled_width = std::max(geometry.scaled_width, output_width);
  geometry.scaled_height = std::max(geometry.scaled_height, output_height);
  geometry.offset_x = (output_width - geometry.scaled_width) / 2;
  geometry.offset_y = (output_height - geometry.scaled_height) / 2;
  return geometry;
}

} // namespace pulsar::application::composition
