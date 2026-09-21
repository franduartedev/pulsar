#include <pulsar/application/waveform/waveform_renderer.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace pulsar;

namespace {
void check(bool condition,
           const std::source_location location = std::source_location::current()) {
  if (!condition) throw std::runtime_error{"waveform renderer regression at line " +
                                          std::to_string(location.line())};
}

application::WaveformSlice slice(int channels, std::vector<application::WaveformBucket> buckets) {
  application::WaveformSlice result;
  result.requested_start = domain::MediaTime{0, domain::Rational{1, 1}};
  result.requested_end = domain::MediaTime{1, domain::Rational{1, 1}};
  result.available_start = result.requested_start;
  result.available_end = result.requested_end;
  result.bucket_sample_span = 1;
  result.channel_count = channels;
  result.bucket_count = buckets.size() / static_cast<std::size_t>(channels);
  result.buckets = std::move(buckets);
  result.completeness = application::WaveformCompleteness::Complete;
  return result;
}

void test_silence_and_impulses() {
  const auto silence = slice(1, {{0, 0}});
  const auto flat = application::prepare_waveform_geometry(
      silence, 1, {1.0, false, 1, 32.0});
  check(flat.segments.size() == 1);
  check(flat.segments[0].top == 16.0 && flat.segments[0].bottom == 16.0);

  const auto positive = slice(1, {{0, 0}, {0, 32767}});
  const auto positive_geometry = application::prepare_waveform_geometry(
      positive, 2, {1.0, false, 2, 32.0});
  check(positive_geometry.segments.size() == 2);
  check(positive_geometry.segments[1].top == 0.0);

  const auto negative = slice(1, {{0, 0}, {-32768, 0}});
  const auto negative_geometry = application::prepare_waveform_geometry(
      negative, 2, {1.0, false, 2, 32.0});
  check(negative_geometry.segments[1].bottom == 32.0);
}

void test_extrema_merge_and_channels() {
  // The odd-position impulse must survive reduction into one raster column.
  const auto stereo = slice(2, {{0, 0}, {0, 0}, {0, 30000}, {0, 0},
                                {0, 0}, {0, 0}, {0, 0}, {-30000, 0}});
  const auto combined = application::prepare_waveform_geometry(
      stereo, 4, {0.5, true, 1, 40.0});
  check(combined.segments.size() == 1);
  check(combined.segments.front().top < 10.0);
  check(combined.segments.front().bottom > 30.0);
  const auto stacked = application::prepare_waveform_geometry(
      stereo, 4, {2.0, false, 4, 80.0});
  check(stacked.segments.size() == 8);
  check(stacked.segments[1].top < 40.0);
  check(stacked.segments[7].bottom > 40.0);

  std::vector<std::uint32_t> event_columns;
  for (const double density : {0.5, 1.0, 2.0}) {
    const auto geometry = application::prepare_waveform_geometry(
        stereo, 4, {density, true, 4, 40.0});
    const auto event = std::find_if(geometry.segments.begin(), geometry.segments.end(),
                                    [](const auto& segment) { return segment.top < 10.0; });
    check(event != geometry.segments.end());
    event_columns.push_back(event->column);
  }
  check(event_columns[0] == event_columns[1] && event_columns[1] == event_columns[2]);
}

void test_partial_is_not_silence_and_clips_to_content() {
  auto partial = slice(1, {{0, 0}});
  partial.requested_end = domain::MediaTime{2, domain::Rational{1, 1}};
  partial.available_end = domain::MediaTime{1, domain::Rational{1, 1}};
  partial.completeness = application::WaveformCompleteness::Partial;
  const auto geometry = application::prepare_waveform_geometry(
      partial, 1, {1.0, false, 20, 24.0});
  check(geometry.segments.size() == 10);
  check(geometry.pending_columns.size() == 10);
  for (const auto& segment : geometry.segments) {
    check(segment.column < geometry.width);
    check(segment.top >= 0.0 && segment.bottom <= 24.0);
  }
  for (auto column : geometry.pending_columns) check(column >= 10 && column < 20);
}

void test_audio_late_prefix_is_unavailable_not_pending_or_silence() {
  auto late = slice(1, {{0, 12000}, {0, 20000}});
  late.available_start = domain::MediaTime{2, domain::Rational{1, 5}};
  late.available_end = domain::MediaTime{7, domain::Rational{5, 5}};
  const auto geometry = application::prepare_waveform_geometry(
      late, 2, {1.0, false, 10, 32.0});
  check(geometry.pending_columns.empty());
  check(geometry.unavailable_columns.size() == 5);
  check(geometry.unavailable_columns[0] == 0 && geometry.unavailable_columns[3] == 3);
  check(!geometry.segments.empty() && geometry.segments.front().column >= 2);
}

void test_two_hour_mapping_stays_bounded_and_exact() {
  application::WaveformSlice long_slice;
  long_slice.requested_start = domain::MediaTime::zero();
  long_slice.requested_end = domain::MediaTime{7200, domain::Rational{1, 1}};
  long_slice.available_start = domain::MediaTime::zero();
  long_slice.available_end = long_slice.requested_end;
  long_slice.bucket_sample_span = 65536;
  long_slice.level = 8;
  long_slice.channel_count = 1;
  long_slice.bucket_count = 5274;
  long_slice.buckets.resize(static_cast<std::size_t>(long_slice.bucket_count));
  long_slice.completeness = application::WaveformCompleteness::Complete;
  const auto event_bucket = static_cast<std::size_t>((5400LL * 48000LL) / 65536LL);
  long_slice.buckets[event_bucket].maximum = 32767;
  constexpr std::size_t exact_pixel_boundary_bucket = 3375;
  long_slice.buckets[exact_pixel_boundary_bucket].maximum = 32767;
  const auto geometry = application::prepare_waveform_geometry(
      long_slice, 48000, {1.0, false, 400, 48.0});
  check(geometry.segments.size() <= geometry.width);
  check(std::any_of(geometry.segments.begin(), geometry.segments.end(),
                    [](const auto& segment) {
                      return segment.column == 300 && segment.top == 0.0;
                    }));
  check(std::any_of(geometry.segments.begin(), geometry.segments.end(),
                    [](const auto& segment) {
                      return segment.column == 256 && segment.top == 0.0;
                    }));
  check(std::none_of(geometry.segments.begin(), geometry.segments.end(),
                     [](const auto& segment) {
                       return segment.column == 255 && segment.top == 0.0;
                     }));
}

void test_unresolved_l0_split_boundary_is_not_claimed_by_both_sides() {
  const auto make_child = [](domain::MediaTime start, domain::MediaTime end) {
    application::WaveformSlice child;
    child.requested_start = start;
    child.requested_end = end;
    child.available_start = domain::MediaTime::zero();
    child.available_end = domain::MediaTime{1, domain::Rational{1, 1}};
    child.level = 0;
    child.bucket_sample_span = 4;
    child.channel_count = 1;
    child.first_bucket = 0;
    child.bucket_count = 1;
    child.buckets = {{0, 32767}};
    child.completeness = application::WaveformCompleteness::Complete;
    return child;
  };
  const auto split = domain::MediaTime{3, domain::Rational{1, 4}};
  const auto left = make_child(domain::MediaTime::zero(), split);
  const auto right = make_child(split, domain::MediaTime{1, domain::Rational{1, 1}});
  const auto left_geometry = application::prepare_waveform_geometry(
      left, 4, {1.0, false, 16, 24.0});
  const auto right_geometry = application::prepare_waveform_geometry(
      right, 4, {1.0, false, 16, 24.0});
  check(left_geometry.segments.empty() && right_geometry.segments.empty());
  check(!left_geometry.unavailable_columns.empty() &&
        !right_geometry.unavailable_columns.empty());
}

void test_finer_boundary_refinement_replaces_coarse_bucket() {
  application::WaveformSlice coarse;
  coarse.requested_start = domain::MediaTime::zero();
  coarse.requested_end = domain::MediaTime{1, domain::Rational{1, 1}};
  coarse.available_start = coarse.requested_start;
  coarse.available_end = coarse.requested_end;
  coarse.level = 1;
  coarse.bucket_sample_span = 4;
  coarse.channel_count = 1;
  coarse.bucket_count = 1;
  coarse.buckets = {{0, 32767}}; // the coarse peak belongs after the split
  coarse.completeness = application::WaveformCompleteness::Complete;

  application::WaveformSlice left_detail;
  left_detail.requested_start = domain::MediaTime::zero();
  left_detail.requested_end = domain::MediaTime{1, domain::Rational{1, 4}};
  left_detail.available_start = left_detail.requested_start;
  left_detail.available_end = left_detail.requested_end;
  left_detail.level = 0;
  left_detail.bucket_sample_span = 1;
  left_detail.channel_count = 1;
  left_detail.bucket_count = 1;
  left_detail.buckets = {{0, 0}};
  left_detail.completeness = application::WaveformCompleteness::Complete;

  const auto geometry = application::prepare_waveform_geometry(
      coarse, {left_detail}, 4, {1.0, false, 64, 32.0});
  const auto left = std::find_if(geometry.segments.begin(), geometry.segments.end(),
                                 [](const auto& segment) { return segment.column == 0; });
  const auto right = std::find_if(geometry.segments.begin(), geometry.segments.end(),
                                  [](const auto& segment) { return segment.column == 32; });
  check(left != geometry.segments.end() && left->top == 16.0 && left->bottom == 16.0);
  check(right != geometry.segments.end() && right->top == 0.0);
}

void benchmark_visible_geometry_scaling() {
  using Clock = std::chrono::steady_clock;
  constexpr std::uint64_t source_frames = 7200ULL * 48000ULL;
  const auto source_end = domain::MediaTime{7200, domain::Rational{1, 1}};
  for (const std::uint32_t viewport_width : {1280U, 1920U, 3840U}) {
    for (const std::uint32_t visible_clips : {10U, 50U, 100U}) {
      const auto clip_width = std::max(1U, viewport_width / visible_clips);
      for (const double density : {0.5, 1.0, 2.0}) {
        const auto bucket_count = std::max<std::uint64_t>(
            1, static_cast<std::uint64_t>(std::ceil(clip_width * density)));
        application::WaveformSlice data;
        data.requested_start = domain::MediaTime::zero();
        data.requested_end = source_end;
        data.available_start = data.requested_start;
        data.available_end = source_end;
        data.level = 5;
        data.bucket_sample_span = (source_frames + bucket_count - 1) / bucket_count;
        data.channel_count = 1;
        data.bucket_count = bucket_count;
        data.buckets.resize(static_cast<std::size_t>(bucket_count));
        data.completeness = application::WaveformCompleteness::Complete;
        for (std::uint64_t i = 0; i < bucket_count; i += 11)
          data.buckets[static_cast<std::size_t>(i)].maximum = 24000;
        std::size_t segment_count = 0;
        const auto start = Clock::now();
        for (std::uint32_t clip = 0; clip < visible_clips; ++clip) {
          const auto geometry = application::prepare_waveform_geometry(
              data, 48000, {density, false, clip_width, 48.0});
          segment_count += geometry.segments.size();
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now() - start);
        std::cout << "geometry-bench clips=" << visible_clips
                  << " viewport=" << viewport_width << " density=" << density
                  << " queried_buckets=" << bucket_count * visible_clips
                  << " rendered_segments=" << segment_count
                  << " prep_us=" << elapsed.count() << '\n';
      }
    }
  }
}

} // namespace

int main() {
  test_silence_and_impulses();
  test_extrema_merge_and_channels();
  test_partial_is_not_silence_and_clips_to_content();
  test_audio_late_prefix_is_unavailable_not_pending_or_silence();
  test_two_hour_mapping_stays_bounded_and_exact();
  test_unresolved_l0_split_boundary_is_not_claimed_by_both_sides();
  test_finer_boundary_refinement_replaces_coarse_bucket();
  benchmark_visible_geometry_scaling();
  std::cout << "waveform renderer tests passed\n";
}
