#include <pulsar/application/waveform/waveform_renderer.hpp>

#include <pulsar/domain/support/wide_integer.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace pulsar::application {
namespace {

// Rational pixel mapping uses 128-bit products of normalized int64 rationals.
// The largest cross product is below 2^126, so the signed wide integer is
// large enough; multiplication by pixel width is performed as bounded modular
// doubling so it cannot overflow that product.
using WideUnsigned = domain::WideInteger;

std::uint32_t fraction_pixel(domain::MediaTime time, domain::MediaTime start,
                             domain::MediaTime end, std::uint32_t width,
                             bool round_up) {
  if (time <= start) return 0;
  if (time >= end) return width;
  const auto offset = (time - start).seconds();
  const auto duration = (end - start).seconds();
  const WideUnsigned numerator = static_cast<WideUnsigned>(offset.numerator()) *
                                 static_cast<WideUnsigned>(duration.denominator());
  const WideUnsigned denominator = static_cast<WideUnsigned>(offset.denominator()) *
                                   static_cast<WideUnsigned>(duration.numerator());
  WideUnsigned remainder = 0;
  std::uint32_t quotient = 0;
  int top_bit = 31;
  while (top_bit > 0 && ((width >> top_bit) & 1U) == 0U) --top_bit;
  for (int bit = top_bit; bit >= 0; --bit) {
    quotient *= 2U;
    remainder *= 2U;
    if (remainder >= denominator) {
      remainder -= denominator;
      ++quotient;
    }
    if (((width >> bit) & 1U) != 0U) {
      remainder += numerator;
      if (remainder >= denominator) {
        remainder -= denominator;
        ++quotient;
      }
    }
  }
  if (round_up && remainder != 0) ++quotient;
  return std::min(quotient, width);
}

struct Extrema {
  std::int16_t minimum{std::numeric_limits<std::int16_t>::max()};
  std::int16_t maximum{std::numeric_limits<std::int16_t>::min()};
  bool occupied{false};
};

} // namespace

WaveformGeometry prepare_waveform_geometry(const WaveformSlice& slice, int sample_rate,
                                            const WaveformRenderPolicy& policy) {
  return prepare_waveform_geometry(slice, {}, sample_rate, policy);
}

WaveformGeometry prepare_waveform_geometry(
    const WaveformSlice& slice, const std::vector<WaveformSlice>& boundary_refinements,
    int sample_rate, const WaveformRenderPolicy& policy) {
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
  if (slice.requested_end <= slice.requested_start)
    throw std::invalid_argument{"invalid waveform source interval"};
  for (const auto& refinement : boundary_refinements) {
    if (refinement.channel_count != slice.channel_count ||
        refinement.bucket_sample_span == 0 ||
        refinement.requested_end <= refinement.requested_start ||
        refinement.bucket_count > std::numeric_limits<std::uint64_t>::max() -
                                      refinement.first_bucket ||
        refinement.bucket_count > std::numeric_limits<std::size_t>::max() /
                                      static_cast<std::size_t>(refinement.channel_count) ||
        refinement.buckets.size() != refinement.bucket_count *
                                          static_cast<std::size_t>(refinement.channel_count)) {
      throw std::invalid_argument{"invalid waveform boundary refinement"};
    }
  }
  const std::uint32_t width = policy.raster_width;
  WaveformGeometry result;
  result.width = width;
  const std::uint32_t display_channels = policy.combine_channels ? 1U
      : static_cast<std::uint32_t>(slice.channel_count);
  const double lane_height = policy.content_height / static_cast<double>(display_channels);
  if (lane_height <= 0.0) return result;
  std::vector<Extrema> columns(static_cast<std::size_t>(width) * display_channels);
  std::vector<bool> refined_columns(width, false);
  std::vector<bool> unresolved_boundary_columns(width, false);
  for (const auto& refinement : boundary_refinements) {
    if (refinement.channel_count != slice.channel_count ||
        refinement.requested_end <= refinement.requested_start) continue;
    const auto x0 = fraction_pixel(refinement.requested_start, slice.requested_start,
                                   slice.requested_end, width, false);
    const auto x1 = fraction_pixel(refinement.requested_end, slice.requested_start,
                                   slice.requested_end, width, true);
    for (auto x = x0; x < x1; ++x) refined_columns[x] = true;
  }

  const auto merge_slice = [&](const WaveformSlice& input, bool require_refined_column) {
    for (std::uint64_t i = 0; i < input.bucket_count; ++i) {
      const std::uint64_t bucket = input.first_bucket + i;
      if (bucket > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) /
                       input.bucket_sample_span) {
        throw std::overflow_error{"waveform bucket sample index overflow"};
      }
      const auto first_sample = static_cast<std::int64_t>(bucket * input.bucket_sample_span);
      if (input.bucket_sample_span >
          static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() - first_sample)) {
        throw std::overflow_error{"waveform bucket end sample index overflow"};
      }
      const auto bucket_start = input.available_start +
          domain::MediaTime{first_sample, domain::Rational{1, sample_rate}};
      const auto bucket_end = input.available_start + domain::MediaTime{
          first_sample + static_cast<std::int64_t>(input.bucket_sample_span),
          domain::Rational{1, sample_rate}};
      const auto left = std::max(bucket_start, input.requested_start);
      const auto right = std::min(bucket_end, input.requested_end);
      if (right <= left) continue;
      const bool unresolved_l0_boundary = input.level == 0 &&
          ((bucket_start < slice.requested_start && slice.requested_start < bucket_end) ||
           (bucket_start < slice.requested_end && slice.requested_end < bucket_end));
      const auto x0 = fraction_pixel(left, slice.requested_start, slice.requested_end,
                                     width, false);
      const auto x1 = std::max(x0 + (x0 < width ? 1U : 0U),
                               fraction_pixel(right, slice.requested_start,
                                              slice.requested_end, width, true));
      for (std::uint32_t x = x0; x < x1; ++x) {
        if (refined_columns[x] != require_refined_column) continue;
        if (unresolved_l0_boundary) {
          unresolved_boundary_columns[x] = true;
          continue;
        }
        if (policy.combine_channels) {
          auto& target = columns[x];
          for (int channel = 0; channel < input.channel_count; ++channel) {
            const auto& sample = input.buckets[static_cast<std::size_t>(i) *
                                                static_cast<std::size_t>(input.channel_count) +
                                                static_cast<std::size_t>(channel)];
            target.minimum = std::min(target.minimum, sample.minimum);
            target.maximum = std::max(target.maximum, sample.maximum);
          }
          target.occupied = true;
        } else {
          for (int channel = 0; channel < input.channel_count; ++channel) {
            const auto& sample = input.buckets[static_cast<std::size_t>(i) *
                                                static_cast<std::size_t>(input.channel_count) +
                                                static_cast<std::size_t>(channel)];
            auto& target = columns[static_cast<std::size_t>(channel) * width + x];
            target.minimum = std::min(target.minimum, sample.minimum);
            target.maximum = std::max(target.maximum, sample.maximum);
            target.occupied = true;
          }
        }
      }
    }
  };
  merge_slice(slice, false);
  for (const auto& refinement : boundary_refinements) merge_slice(refinement, true);
  for (std::uint32_t x = 0; x < width; ++x) {
    if (!unresolved_boundary_columns[x]) continue;
    for (std::uint32_t channel = 0; channel < display_channels; ++channel)
      columns[static_cast<std::size_t>(channel) * width + x].occupied = false;
    result.unavailable_columns.push_back(x);
  }

  // Normalize the clip envelope for editing readability while preserving all
  // real peak relationships. Full-scale material is never amplified; quieter
  // clips gain visual headroom so their waveform does not collapse into the
  // centre line. The operation is bounded by raster width, not source length.
  double normalization_peak = 0.0;
  for (const auto& extrema : columns) {
    if (!extrema.occupied) continue;
    normalization_peak = std::max(normalization_peak,
        std::max(std::abs(static_cast<double>(extrema.minimum)),
                 std::abs(static_cast<double>(extrema.maximum))));
  }
  constexpr double target_peak = 32767.0 * 0.92;
  const double normalization = normalization_peak > 0.0
      ? std::max(1.0, target_peak / normalization_peak) : 1.0;

  for (std::uint32_t channel = 0; channel < display_channels; ++channel) {
    const double lane_top = static_cast<double>(channel) * lane_height;
    const double center = lane_top + lane_height / 2.0;
    for (std::uint32_t x = 0; x < width; ++x) {
      const auto& extrema = columns[static_cast<std::size_t>(channel) * width + x];
      if (!extrema.occupied) continue;
      constexpr double sample_scale = 32767.0;
      const double peak = std::max(std::abs(static_cast<double>(extrema.minimum)),
                                   std::abs(static_cast<double>(extrema.maximum)));
      const double half_height = peak * normalization / sample_scale * (lane_height / 2.0);
      const double top = center - half_height;
      const double bottom = center + half_height;
      result.segments.push_back({x, std::clamp(top, lane_top, lane_top + lane_height),
                                   std::clamp(bottom, lane_top, lane_top + lane_height)});
    }
  }

  const auto add_columns = [&](std::vector<std::uint32_t>& columns,
                               domain::MediaTime left, domain::MediaTime right) {
      left = std::max(left, slice.requested_start);
      right = std::min(right, slice.requested_end);
      if (right <= left) return;
      const auto x0 = fraction_pixel(left, slice.requested_start, slice.requested_end,
                                     width, false);
      const auto x1 = fraction_pixel(right, slice.requested_start, slice.requested_end,
                                     width, true);
      for (auto x = x0; x < x1; ++x) columns.push_back(x);
  };
  if (slice.completeness != WaveformCompleteness::Complete) {
    if (slice.completeness == WaveformCompleteness::Missing) {
      add_columns(result.pending_columns, slice.requested_start, slice.requested_end);
    } else {
      add_columns(result.unavailable_columns, slice.requested_start, slice.available_start);
      add_columns(result.pending_columns, slice.available_end, slice.requested_end);
    }
  } else {
    add_columns(result.unavailable_columns, slice.requested_start, slice.available_start);
    add_columns(result.unavailable_columns, slice.available_end, slice.requested_end);
  }
  const auto canonicalize = [](std::vector<std::uint32_t>& columns) {
    std::sort(columns.begin(), columns.end());
    columns.erase(std::unique(columns.begin(), columns.end()), columns.end());
  };
  canonicalize(result.pending_columns);
  canonicalize(result.unavailable_columns);
  return result;
}

} // namespace pulsar::application
