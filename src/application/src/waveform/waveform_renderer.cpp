#include <pulsar/application/waveform/waveform_renderer.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace pulsar::application {
namespace {

long double seconds(domain::MediaTime time) {
  const auto value = time.seconds();
  return static_cast<long double>(value.numerator()) /
         static_cast<long double>(value.denominator());
}

std::uint32_t pixel_floor(long double value, std::uint32_t width) {
  return static_cast<std::uint32_t>(std::clamp(std::floor(value), 0.0L,
                                               static_cast<long double>(width)));
}

std::uint32_t pixel_ceil(long double value, std::uint32_t width) {
  return static_cast<std::uint32_t>(std::clamp(std::ceil(value), 0.0L,
                                               static_cast<long double>(width)));
}

struct Extrema {
  std::int16_t minimum{std::numeric_limits<std::int16_t>::max()};
  std::int16_t maximum{std::numeric_limits<std::int16_t>::min()};
  bool occupied{false};
};

} // namespace

WaveformGeometry prepare_waveform_geometry(const WaveformSlice& slice, int sample_rate,
                                            const WaveformRenderPolicy& policy) {
  if (sample_rate <= 0 || !std::isfinite(policy.density) || policy.density <= 0.0 ||
      !std::isfinite(policy.content_height) || policy.content_height < 0.0 ||
      slice.channel_count <= 0 || slice.bucket_sample_span == 0 ||
      slice.requested_end <= slice.requested_start || policy.raster_width == 0 ||
      policy.raster_width > 16384 ||
      slice.bucket_count > std::numeric_limits<std::uint64_t>::max() - slice.first_bucket ||
      slice.bucket_count > std::numeric_limits<std::size_t>::max() /
                               static_cast<std::size_t>(slice.channel_count) ||
      slice.buckets.size() != slice.bucket_count * static_cast<std::size_t>(slice.channel_count)) {
    throw std::invalid_argument{"invalid waveform geometry input"};
  }
  const auto source_width = seconds(slice.requested_end) - seconds(slice.requested_start);
  if (!(source_width > 0.0L)) throw std::invalid_argument{"invalid waveform source interval"};
  const std::uint32_t width = policy.raster_width;
  WaveformGeometry result;
  result.width = width;
  const std::uint32_t display_channels = policy.combine_channels ? 1U
      : static_cast<std::uint32_t>(slice.channel_count);
  const double lane_height = policy.content_height / static_cast<double>(display_channels);
  if (lane_height <= 0.0) return result;
  std::vector<Extrema> columns(static_cast<std::size_t>(width) * display_channels);

  const long double requested_start = seconds(slice.requested_start);
  const long double requested_end = seconds(slice.requested_end);
  const long double origin = seconds(slice.available_start);
  const long double sample_period = 1.0L / static_cast<long double>(sample_rate);
  for (std::uint64_t i = 0; i < slice.bucket_count; ++i) {
    const std::uint64_t bucket = slice.first_bucket + i;
    const long double first_sample = static_cast<long double>(bucket) *
                                     static_cast<long double>(slice.bucket_sample_span);
    const long double bucket_start = origin + first_sample * sample_period;
    const long double bucket_end = bucket_start +
        static_cast<long double>(slice.bucket_sample_span) * sample_period;
    const long double left = std::max(bucket_start, requested_start);
    const long double right = std::min(bucket_end, requested_end);
    if (right <= left) continue;
    const auto x0 = pixel_floor((left - requested_start) / source_width * width, width);
    const auto x1 = std::max(x0 + (x0 < width ? 1U : 0U),
                             pixel_ceil((right - requested_start) / source_width * width, width));
    for (std::uint32_t x = x0; x < x1; ++x) {
      if (policy.combine_channels) {
        auto& target = columns[x];
        for (int channel = 0; channel < slice.channel_count; ++channel) {
          const auto& sample = slice.buckets[static_cast<std::size_t>(i) *
                                               static_cast<std::size_t>(slice.channel_count) +
                                               static_cast<std::size_t>(channel)];
          target.minimum = std::min(target.minimum, sample.minimum);
          target.maximum = std::max(target.maximum, sample.maximum);
        }
        target.occupied = true;
      } else {
        for (int channel = 0; channel < slice.channel_count; ++channel) {
          const auto& sample = slice.buckets[static_cast<std::size_t>(i) *
                                               static_cast<std::size_t>(slice.channel_count) +
                                               static_cast<std::size_t>(channel)];
          auto& target = columns[static_cast<std::size_t>(channel) * width + x];
          target.minimum = std::min(target.minimum, sample.minimum);
          target.maximum = std::max(target.maximum, sample.maximum);
          target.occupied = true;
        }
      }
    }
  }

  for (std::uint32_t channel = 0; channel < display_channels; ++channel) {
    const double lane_top = static_cast<double>(channel) * lane_height;
    const double center = lane_top + lane_height / 2.0;
    for (std::uint32_t x = 0; x < width; ++x) {
      const auto& extrema = columns[static_cast<std::size_t>(channel) * width + x];
      if (!extrema.occupied) continue;
      constexpr double negative_scale = 32768.0;
      constexpr double positive_scale = 32767.0;
      const double top = center - static_cast<double>(extrema.maximum) / positive_scale *
                                      (lane_height / 2.0);
      const double bottom = center - static_cast<double>(extrema.minimum) / negative_scale *
                                         (lane_height / 2.0);
      result.segments.push_back({x, std::clamp(top, lane_top, lane_top + lane_height),
                                   std::clamp(bottom, lane_top, lane_top + lane_height)});
    }
  }

  if (slice.completeness != WaveformCompleteness::Complete) {
    const long double available_start = seconds(slice.available_start);
    const long double available_end = seconds(slice.available_end);
    const auto add_pending = [&](long double left, long double right) {
      left = std::max(left, requested_start);
      right = std::min(right, requested_end);
      if (right <= left) return;
      const auto x0 = pixel_floor((left - requested_start) / source_width * width, width);
      const auto x1 = pixel_ceil((right - requested_start) / source_width * width, width);
      for (auto x = x0; x < x1; ++x) result.pending_columns.push_back(x);
    };
    if (slice.completeness == WaveformCompleteness::Missing) {
      add_pending(requested_start, requested_end);
    } else {
      add_pending(requested_start, available_start);
      add_pending(available_end, requested_end);
    }
    std::sort(result.pending_columns.begin(), result.pending_columns.end());
    result.pending_columns.erase(std::unique(result.pending_columns.begin(),
                                              result.pending_columns.end()),
                                 result.pending_columns.end());
  }
  return result;
}

} // namespace pulsar::application
