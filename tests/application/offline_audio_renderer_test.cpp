#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/application/export/offline_audio_renderer.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <cassert>
#include <algorithm>
#include <cmath>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
using namespace pulsar;
namespace exporting = application::exporting;

class Probe final : public application::MediaProbe {
public:
  explicit Probe(domain::MediaTime duration = domain::MediaTime{1, {1, 1}})
      : duration_{duration} {}

  application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    application::MediaProbeResult result;
    result.path = path;
    result.container_name = "test";
    result.video_streams.push_back({0, "raw", 640, 360, domain::Rational{1, 30},
                                    duration_, domain::FrameRate{30, 1}});
    result.audio_streams.push_back({1, "pcm", 48'000, 2, domain::Rational{1, 48'000},
                                    duration_});
    return result;
  }

private:
  domain::MediaTime duration_;
};

struct SourceSpec {
  int rate;
  int channels;
  std::optional<domain::MediaTime> origin;
  std::vector<exporting::OfflineAudioSourceBlock> blocks;
};

class Source final : public exporting::OfflineAudioSource {
public:
  explicit Source(SourceSpec spec) : spec_{std::move(spec)} {}

  int sample_rate() const noexcept override { return spec_.rate; }
  int channel_count() const noexcept override { return spec_.channels; }
  std::optional<domain::MediaTime> stream_origin() const override { return spec_.origin; }

  void seek_to(domain::MediaTime) override { next_ = 0; }

  std::optional<exporting::OfflineAudioSourceBlock> decode_next() override {
    if (next_ >= spec_.blocks.size()) return std::nullopt;
    return spec_.blocks[next_++];
  }

private:
  SourceSpec spec_;
  std::size_t next_{0};
};

class Factory final : public exporting::OfflineAudioSourceFactory {
public:
  explicit Factory(SourceSpec spec) : spec_{std::move(spec)} {}

  std::unique_ptr<exporting::OfflineAudioSource> open(const std::filesystem::path&, int) override {
    ++opens;
    return std::make_unique<Source>(spec_);
  }

  std::size_t opens{0};

private:
  SourceSpec spec_;
};

application::ProjectSnapshot snapshot_with_clip(bool muted = false,
                                                domain::MediaTime duration = {1, {1, 1}}) {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto media = processor.import_asset("logical-audio", Probe{duration});
  const auto clip = processor.add_clip(media, domain::TimeRange{domain::MediaTime::zero(), duration},
                                       domain::MediaTime::zero());
  if (muted) processor.set_clip_muted(clip, true);
  return document.snapshot();
}

std::vector<float> render_all(const application::ProjectSnapshot& snapshot, std::size_t block_size,
                              Factory& factory, exporting::OfflineAudioRenderMetrics* metrics = nullptr) {
  const auto plan = exporting::ExportPlan::build(snapshot);
  exporting::OfflineAudioRenderer renderer{plan, factory, block_size};
  std::vector<float> result;
  while (const auto block = renderer.render_next_block()) {
    assert(block->sample_rate == 48'000);
    assert(block->channel_count == 2);
    assert(block->samples.size() == block->frame_count * 2);
    result.insert(result.end(), block->samples.begin(), block->samples.end());
  }
  assert(result.size() == static_cast<std::size_t>(plan.audio_sample_count()) * 2);
  if (metrics != nullptr) *metrics = renderer.metrics();
  return result;
}

SourceSpec constant_source(int rate, int channels, float value, std::size_t frames) {
  std::vector<float> samples(frames * static_cast<std::size_t>(channels), value);
  return SourceSpec{rate, channels, domain::MediaTime::zero(),
                    {{rate, channels, frames, std::move(samples), domain::MediaTime::zero()}}};
}

SourceSpec chunked_constant_source(std::int64_t seconds) {
  constexpr std::size_t chunk = 4096;
  const auto total = static_cast<std::size_t>(seconds) * 48'000;
  std::vector<exporting::OfflineAudioSourceBlock> blocks;
  for (std::size_t first = 0; first < total; first += chunk) {
    const auto frames = std::min(chunk, total - first);
    blocks.push_back({48'000, 1, frames, std::vector<float>(frames, 0.125F),
                      domain::MediaTime{static_cast<std::int64_t>(first), {1, 48'000}}});
  }
  return SourceSpec{48'000, 1, domain::MediaTime::zero(), std::move(blocks)};
}

void output_is_stereo_and_stable_across_block_sizes() {
  const auto snapshot = snapshot_with_clip();
  auto first_factory = Factory{constant_source(48'000, 2, 0.25F, 48'000)};
  auto second_factory = Factory{constant_source(48'000, 2, 0.25F, 48'000)};
  const auto first = render_all(snapshot, 257, first_factory);
  const auto second = render_all(snapshot, 4096, second_factory);
  assert(first == second);
  assert(first[0] == 0.25F);
  assert(first[1] == 0.25F);
  assert(first[first.size() - 2] == 0.25F);
  assert(first[first.size() - 1] == 0.25F);
  assert(first_factory.opens == 1);
  assert(second_factory.opens == 1);
}

void mono_is_rematrixed_and_mute_is_silence() {
  const auto snapshot = snapshot_with_clip();
  auto factory = Factory{constant_source(48'000, 1, 0.5F, 48'000)};
  const auto pcm = render_all(snapshot, 1024, factory);
  assert(pcm[0] == 0.5F && pcm[1] == 0.5F);

  auto muted_factory = Factory{constant_source(48'000, 1, 0.5F, 48'000)};
  const auto muted = render_all(snapshot_with_clip(true), 1024, muted_factory);
  for (const auto sample : muted) assert(sample == 0.0F);
}

void resampling_is_rational_and_source_origin_is_normalized() {
  const auto snapshot = snapshot_with_clip();
  constexpr std::size_t source_frames = 44'100;
  std::vector<float> samples(source_frames);
  for (std::size_t index = 0; index < source_frames; ++index) {
    samples[index] = static_cast<float>(index) / static_cast<float>(source_frames);
  }
  auto block = exporting::OfflineAudioSourceBlock{
      44'100, 1, source_frames, std::move(samples), domain::MediaTime{7, {1, 1}}};
  auto factory = Factory{SourceSpec{44'100, 1, domain::MediaTime{7, {1, 1}}, {std::move(block)}}};
  exporting::OfflineAudioRenderMetrics metrics;
  const auto pcm = render_all(snapshot, 311, factory, &metrics);
  assert(pcm.size() == 96'000);
  assert(std::isfinite(pcm[0]));
  assert(metrics.seeks == 1);
  assert(metrics.decoded_blocks == 1);
}

void source_ends_early_as_error() {
  const auto snapshot = snapshot_with_clip();
  auto factory = Factory{constant_source(48'000, 1, 1.0F, 100)};
  const auto plan = exporting::ExportPlan::build(snapshot);
  exporting::OfflineAudioRenderer renderer{plan, factory, 4096};
  bool failed = false;
  try {
    while (renderer.render_next_block()) {}
  } catch (const exporting::OfflineAudioRenderError& error) {
    failed = error.code() == exporting::OfflineAudioRenderErrorCode::SourceEndedEarly;
  }
  assert(failed);
}

void errors_carry_render_context() {
  auto factory = Factory{constant_source(48'000, 1, 1.0F, 100)};
  const auto plan = exporting::ExportPlan::build(snapshot_with_clip());
  exporting::OfflineAudioRenderer renderer{plan, factory, 4096};
  std::string message;
  try {
    while (renderer.render_next_block()) {}
  } catch (const exporting::OfflineAudioRenderError& error) {
    message = error.what();
  }
  for (const char* key : {"clip_id=", "media_id=", "stream_index=", "sample_rate=",
                          "timeline_time=", "source_time=", "source_grid_time=",
                          "next_grid_time=", "source_range="}) {
    assert(message.find(key) != std::string::npos);
  }
}

void source_timestamp_gaps_and_overlaps_are_explicit() {
  const auto snapshot = snapshot_with_clip();
  auto first_samples = std::vector<float>(24'000, 0.25F);
  auto second_samples = std::vector<float>(12'000, 0.75F);
  auto gap_source = SourceSpec{
      48'000, 1, domain::MediaTime::zero(),
      {{48'000, 1, 24'000, std::move(first_samples), domain::MediaTime::zero()},
       {48'000, 1, 12'000, std::move(second_samples), domain::MediaTime{3, {1, 4}}}}};
  auto gap_factory = Factory{std::move(gap_source)};
  const auto gap_pcm = render_all(snapshot, 4096, gap_factory);
  assert(gap_pcm[0] == 0.25F);
  assert(gap_pcm[30'000 * 2] == 0.0F);
  assert(std::abs(gap_pcm[38'000 * 2] - 0.75F) < 1.0e-6F);

  auto overlap_first = std::vector<float>(30'000, 0.25F);
  auto overlap_second = std::vector<float>(24'000, 0.75F);
  auto overlap_source = SourceSpec{
      48'000, 1, domain::MediaTime::zero(),
      {{48'000, 1, 30'000, std::move(overlap_first), domain::MediaTime::zero()},
       {48'000, 1, 24'000, std::move(overlap_second), domain::MediaTime{1, {1, 2}}}}};
  auto overlap_factory = Factory{std::move(overlap_source)};
  exporting::OfflineAudioRenderMetrics metrics;
  (void)render_all(snapshot, 4096, overlap_factory, &metrics);
  assert(metrics.trimmed_overlap_frames == 6'000);
}

void missing_timestamp_is_not_silence() {
  const auto snapshot = snapshot_with_clip();
  auto samples = std::vector<float>(48'000, 0.5F);
  auto block = exporting::OfflineAudioSourceBlock{48'000, 1, 48'000, std::move(samples), std::nullopt};
  auto factory = Factory{SourceSpec{48'000, 1, domain::MediaTime::zero(), {std::move(block)}}};
  const auto plan = exporting::ExportPlan::build(snapshot);
  exporting::OfflineAudioRenderer renderer{plan, factory};
  bool failed = false;
  try {
    (void)renderer.render_next_block();
  } catch (const exporting::OfflineAudioRenderError& error) {
    failed = error.code() == exporting::OfflineAudioRenderErrorCode::TimestampMissing;
  }
  assert(failed);
}

void long_renders_keep_bounded_source_state() {
  for (const auto seconds : {10, 30}) {
    const auto snapshot = snapshot_with_clip(false, domain::MediaTime{seconds, {1, 1}});
    auto factory = Factory{chunked_constant_source(seconds)};
    const auto plan = exporting::ExportPlan::build(snapshot);
    exporting::OfflineAudioRenderer renderer{plan, factory, 4096};
    while (renderer.render_next_block()) {}
    assert(renderer.metrics().output_samples == static_cast<std::uint64_t>(seconds) * 48'000);
    assert(renderer.metrics().source_opens == 1);
    assert(renderer.metrics().seeks == 1);
    assert(renderer.metrics().peak_buffered_source_frames <= 3 * 4096);
  }
}

// Non-aligned source range: a 44.1 kHz source whose clip starts at 2.127 s,
// which is not on the physical sample grid (2.127 * 44100 = 93800.7).
// The interpolation predecessor (floor) lies before source_range.start and the
// successor can lie past source_range.end; neither may escape the clip.
application::ProjectSnapshot snapshot_with_source_range(domain::MediaTime source_start,
                                                         domain::MediaTime duration) {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto media = processor.import_asset("logical-audio", Probe{domain::MediaTime{10, {1, 1}}});
  (void)processor.add_clip(media, domain::TimeRange{source_start, source_start + duration},
                           domain::MediaTime::zero());
  return document.snapshot();
}

// Ramp source: sample i has value i / total, so any clamp is observable.
SourceSpec ramp_source(int rate, std::size_t frames, std::size_t chunk = 0) {
  if (chunk == 0) chunk = frames;
  std::vector<exporting::OfflineAudioSourceBlock> blocks;
  for (std::size_t first = 0; first < frames; first += chunk) {
    const auto count = std::min(chunk, frames - first);
    std::vector<float> samples(count);
    for (std::size_t i = 0; i < count; ++i) {
      samples[i] = static_cast<float>(first + i) / static_cast<float>(frames);
    }
    blocks.push_back({rate, 1, count, std::move(samples),
                      domain::MediaTime{static_cast<std::int64_t>(first), {1, rate}}});
  }
  return SourceSpec{rate, 1, domain::MediaTime::zero(), std::move(blocks)};
}

void unaligned_source_start_is_clamped_not_an_error() {
  const domain::MediaTime start{2127, {1, 1000}};
  const domain::MediaTime duration{1, {1, 1}};
  const auto snapshot = snapshot_with_source_range(start, duration);
  auto factory = Factory{ramp_source(44'100, 441'000)};
  exporting::OfflineAudioRenderMetrics metrics;
  const auto pcm = render_all(snapshot, 1024, factory, &metrics);
  assert(pcm.size() == 96'000);
  for (const auto sample : pcm) assert(std::isfinite(sample));
  // First output sample sits at 2.127 s, i.e. source index 93800.7. The
  // predecessor (93800) is before the clip, so the clamp yields sample 93801.
  const float expected = 93'801.0F / 441'000.0F;
  assert(std::abs(pcm[0] - expected) < 1.0e-6F);
  assert(std::abs(pcm[1] - expected) < 1.0e-6F);
  assert(metrics.inserted_source_gap_silence == 0);
  assert(metrics.source_opens == 1);
}

void unaligned_source_end_is_clamped_not_an_error() {
  // Clip end 3.127 s -> index 137906.7: the successor tap (137907) lies at/after end.
  const domain::MediaTime start{2127, {1, 1000}};
  const domain::MediaTime duration{1, {1, 1}};
  const auto snapshot = snapshot_with_source_range(start, duration);
  auto factory = Factory{ramp_source(44'100, 441'000)};
  const auto pcm = render_all(snapshot, 4096, factory);
  const float last = pcm[pcm.size() - 2];
  assert(std::isfinite(last));
  // Never interpolates toward silence: the tail stays at the last in-clip level.
  assert(last > 137'000.0F / 441'000.0F);
  assert(last <= 137'907.0F / 441'000.0F + 1.0e-6F);
}

void unaligned_range_is_block_size_invariant() {
  const domain::MediaTime start{2127, {1, 1000}};
  const auto snapshot = snapshot_with_source_range(start, domain::MediaTime{1, {1, 1}});
  auto a = Factory{ramp_source(44'100, 441'000, 4096)};
  auto b = Factory{ramp_source(44'100, 441'000, 1000)};
  auto c = Factory{ramp_source(44'100, 441'000)};
  const auto first = render_all(snapshot, 257, a);
  const auto second = render_all(snapshot, 4096, b);
  const auto third = render_all(snapshot, 1, c);
  assert(first == second);
  assert(first == third);
}

void aligned_48k_range_is_unchanged_by_boundary_clamp() {
  const domain::MediaTime start{2, {1, 1}};
  const auto snapshot = snapshot_with_source_range(start, domain::MediaTime{1, {1, 1}});
  auto factory = Factory{ramp_source(48'000, 480'000)};
  const auto pcm = render_all(snapshot, 1024, factory);
  assert(pcm.size() == 96'000);
  assert(pcm[0] == 96'000.0F / 480'000.0F);
  assert(std::abs(pcm[2 * 47'999] - 143'999.0F / 480'000.0F) < 1.0e-6F);
}

void unaligned_source_range_with_blocks_not_covering_start_still_fails_explicitly() {
  // The clamp only forgives the interpolation neighbourhood; a source that
  // ends before the promised range is still a hard error.
  const domain::MediaTime start{2127, {1, 1000}};
  const auto snapshot = snapshot_with_source_range(start, domain::MediaTime{1, {1, 1}});
  auto factory = Factory{ramp_source(44'100, 100'000)};
  const auto plan = exporting::ExportPlan::build(snapshot);
  exporting::OfflineAudioRenderer renderer{plan, factory, 4096};
  bool failed = false;
  try {
    while (renderer.render_next_block()) {}
  } catch (const exporting::OfflineAudioRenderError& error) {
    failed = error.code() == exporting::OfflineAudioRenderErrorCode::SourceEndedEarly;
  }
  assert(failed);
}

} // namespace

// Linear fades at the clip edges, identical for any block size; a plain clip is untouched.
void fades_ramp_linearly_and_are_block_size_invariant() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto media = processor.import_asset("logical-audio", Probe{});
  const auto clip = processor.add_clip(media, domain::TimeRange{domain::MediaTime::zero(), domain::MediaTime{1, {1, 1}}},
                                       domain::MediaTime::zero());
  processor.set_clip_fades(clip, domain::MediaTime{250, {1, 1000}}, domain::MediaTime{500, {1, 1000}});
  const auto snapshot = document.snapshot();
  auto small = Factory{constant_source(48'000, 2, 0.5F, 48'000)};
  auto large = Factory{constant_source(48'000, 2, 0.5F, 48'000)};
  const auto pcm = render_all(snapshot, 333, small);
  assert(pcm == render_all(snapshot, 4096, large));
  const auto left = [&](std::size_t frame) { return pcm[frame * 2]; };
  assert(left(0) == 0.0F);                                    // starts silent
  assert(std::fabs(left(6'000) - 0.25F) < 1e-4F);             // halfway through a 12 000-frame fade-in
  assert(left(12'000) == 0.5F && left(23'999) == 0.5F);       // full level in between
  assert(std::fabs(left(36'000) - 0.25F) < 1e-3F);            // halfway through the 24 000-frame fade-out
  assert(left(47'999) < 1e-4F && left(47'999) > 0.0F);        // fades toward silence at the very end
  for (std::size_t frame = 1; frame < 12'000; ++frame) assert(left(frame) >= left(frame - 1));   // monotonic

  auto plain = Factory{constant_source(48'000, 2, 0.5F, 48'000)};
  const auto untouched = render_all(snapshot_with_clip(), 4096, plain);
  assert(untouched[0] == 0.5F && untouched[untouched.size() - 2] == 0.5F);
}

int main() {
  fades_ramp_linearly_and_are_block_size_invariant();
  output_is_stereo_and_stable_across_block_sizes();
  mono_is_rematrixed_and_mute_is_silence();
  resampling_is_rational_and_source_origin_is_normalized();
  source_ends_early_as_error();
  errors_carry_render_context();
  source_timestamp_gaps_and_overlaps_are_explicit();
  missing_timestamp_is_not_silence();
  long_renders_keep_bounded_source_state();
  unaligned_source_start_is_clamped_not_an_error();
  unaligned_source_end_is_clamped_not_an_error();
  unaligned_range_is_block_size_invariant();
  aligned_48k_range_is_unchanged_by_boundary_clamp();
  unaligned_source_range_with_blocks_not_covering_start_still_fails_explicitly();
}
