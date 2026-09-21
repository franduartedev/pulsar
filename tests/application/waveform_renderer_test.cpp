#include <pulsar/application/waveform/waveform_renderer.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace pulsar;

namespace {
void check(bool condition) {
  if (!condition) throw std::runtime_error{"waveform renderer regression"};
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

} // namespace

int main() {
  test_silence_and_impulses();
  test_extrema_merge_and_channels();
  test_partial_is_not_silence_and_clips_to_content();
  std::cout << "waveform renderer tests passed\n";
}
