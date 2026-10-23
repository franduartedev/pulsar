#include <pulsar/application/audio/audio_contributor_resolver.hpp>
#include <pulsar/application/audio/audio_mix_session.hpp>
#include <pulsar/application/audio/output_stage.hpp>
#include <pulsar/application/project/media_asset.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/timeline.hpp>
#include <pulsar/domain/support/wide_integer.hpp>
#include <pulsar/domain/timeline/track.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace pulsar;

constexpr int kMixRate = 48'000;

domain::MediaTime frames(std::int64_t value, int rate = kMixRate) {
  return {value, domain::Rational{1, rate}};
}

struct SignalSpec {
  int rate{kMixRate};
  int channels{1};
  std::size_t total_frames{kMixRate * 4U};
  std::size_t block_frames{257};
  float constant{0.0F};
  std::size_t impulse_frame{std::numeric_limits<std::size_t>::max()};
  std::int64_t origin_frame{0};
  bool has_origin{false};
  bool starve{false};
  bool throw_decode{false};
};

class SignalSource final : public application::AudioPlaybackSource {
public:
  SignalSource(SignalSpec spec, std::size_t* decoded_frames,
               std::vector<domain::MediaTime>* seek_targets)
      : spec_{std::move(spec)}, decoded_frames_{decoded_frames}, seek_targets_{seek_targets} {}

  int sample_rate() const noexcept override { return spec_.rate; }
  int channel_count() const noexcept override { return spec_.channels; }
  std::optional<domain::MediaTime> stream_origin() const override {
    return spec_.has_origin ? std::optional<domain::MediaTime>{frames(spec_.origin_frame, spec_.rate)}
                            : std::nullopt;
  }

  bool seek_to(domain::MediaTime target) override {
    seek_targets_->push_back(target);
    const auto relative = target - (spec_.has_origin ? frames(spec_.origin_frame, spec_.rate)
                                                     : domain::MediaTime::zero());
    if (relative < domain::MediaTime::zero()) return false;
    const auto seconds = relative.seconds();
    const auto exact = domain::WideInteger{seconds.numerator()} * spec_.rate /
                       seconds.denominator();
    if (exact < 0 || exact > domain::WideInteger::from_unsigned(spec_.total_frames)) return false;
    cursor_ = static_cast<std::size_t>(static_cast<std::uint64_t>(exact));
    return true;
  }

  std::optional<application::AudioBlock> decode_next() override {
    if (spec_.throw_decode) throw std::runtime_error{"synthetic decode failure"};
    if (cursor_ >= spec_.total_frames) return std::nullopt;
    const auto count = std::min(spec_.block_frames, spec_.total_frames - cursor_);
    std::vector<float> samples(count * static_cast<std::size_t>(spec_.channels), spec_.constant);
    if (spec_.impulse_frame >= cursor_ && spec_.impulse_frame < cursor_ + count) {
      const auto local = spec_.impulse_frame - cursor_;
      for (int channel = 0; channel < spec_.channels; ++channel) {
        samples[local * static_cast<std::size_t>(spec_.channels) +
                static_cast<std::size_t>(channel)] = 1.0F;
      }
    }
    const auto pts = frames(spec_.origin_frame + static_cast<std::int64_t>(cursor_), spec_.rate);
    cursor_ += count;
    *decoded_frames_ += count;
    return application::AudioBlock{spec_.rate, spec_.channels, count, std::move(samples),
                                   domain::Rational{1, spec_.rate}, pts};
  }

  ReadResult read_next() override {
    if (spec_.starve) return {ReadState::Starved, std::nullopt};
    auto block = decode_next();
    return block.has_value() ? ReadResult{ReadState::Block, std::move(block)}
                             : ReadResult{ReadState::Ended, std::nullopt};
  }

private:
  SignalSpec spec_;
  std::size_t* decoded_frames_;
  std::vector<domain::MediaTime>* seek_targets_;
  std::size_t cursor_{0};
};

struct SourceBank {
  SourceBank() = default;
  explicit SourceBank(std::map<std::string, SignalSpec> initial_specs)
      : specs{std::move(initial_specs)} {}

  std::map<std::string, SignalSpec> specs;
  std::size_t opens{0};
  std::size_t decoded_frames{0};
  std::vector<domain::MediaTime> seek_targets;

  application::AudioPlaybackSourceFactory factory() {
    return [this](const std::filesystem::path& path, int) {
      ++opens;
      const auto found = specs.find(path.string());
      if (found == specs.end()) return std::unique_ptr<application::AudioPlaybackSource>{};
      return std::unique_ptr<application::AudioPlaybackSource>{
          std::make_unique<SignalSource>(found->second, &decoded_frames, &seek_targets)};
    };
  }
};

application::MediaAsset video_asset(std::uint64_t id, std::string path, int rate = kMixRate,
                                    int channels = 1) {
  return {domain::MediaId{id}, path, 0, frames(kMixRate * 4),
          application::VideoAssetMetadata{"h264", 1920, 1080, domain::Rational{1, 1000},
                                          std::nullopt, "fixture"},
          application::MediaAvailability::Available, std::nullopt,
          application::AudioAssetMetadata{1, rate, channels, domain::Rational{1, rate},
                                          frames(rate * 4, rate)}};
}

application::MediaAsset audio_asset(std::uint64_t id, std::string path, int rate = kMixRate,
                                    int channels = 1,
                                    application::MediaAvailability availability =
                                        application::MediaAvailability::Available) {
  return application::MediaAsset::audio_only(
      domain::MediaId{id}, path, frames(rate * 4, rate),
      application::AudioAssetMetadata{0, rate, channels, domain::Rational{1, rate},
                                      frames(rate * 4, rate)},
      availability, availability == application::MediaAvailability::Unavailable
                        ? std::optional<std::string>{"missing fixture"}
                        : std::nullopt);
}

struct ClipSpec {
  std::uint64_t clip;
  std::uint64_t media;
  std::int64_t timeline_start;
  std::int64_t source_start;
  std::int64_t length;
  double gain_db{0.0};
  bool muted{false};
  std::int64_t fade_in_frames{0};
  std::int64_t fade_out_frames{0};
};

application::ProjectSnapshot snapshot(std::vector<application::MediaAsset> assets,
                                      std::optional<ClipSpec> video,
                                      std::vector<std::vector<ClipSpec>> audio_tracks,
                                      std::uint64_t generation = 1) {
  domain::Timeline timeline{domain::TimelineId{1}};
  domain::Track video_track{domain::TrackId{1}, domain::TrackKind::Video};
  if (video.has_value()) {
    const auto& spec = *video;
    video_track.insert_clip(domain::Clip{
        domain::ClipId{spec.clip}, domain::MediaId{spec.media},
        domain::TimeRange{frames(spec.source_start), frames(spec.source_start + spec.length)},
        frames(spec.timeline_start), spec.gain_db, spec.muted, frames(spec.fade_in_frames), frames(spec.fade_out_frames)});
  }
  timeline.add_track(std::move(video_track));
  std::uint64_t track_id = 2;
  for (const auto& track_specs : audio_tracks) {
    domain::Track track{domain::TrackId{track_id++}, domain::TrackKind::Audio};
    for (const auto& spec : track_specs) {
      track.insert_clip(domain::Clip{
          domain::ClipId{spec.clip}, domain::MediaId{spec.media},
          domain::TimeRange{frames(spec.source_start), frames(spec.source_start + spec.length)},
          frames(spec.timeline_start), spec.gain_db, spec.muted, frames(spec.fade_in_frames), frames(spec.fade_out_frames)});
    }
    timeline.add_track(std::move(track));
  }
  return {std::move(timeline), std::move(assets), domain::TrackId{1},
          application::ProjectAllocatorState{}, 1, 1, 2, generation};
}

void assert_near(float actual, float expected, float tolerance = 1.0e-4F) {
  assert(std::abs(actual - expected) <= tolerance);
}

float left(const application::AudioBlock& block, std::size_t frame) {
  return block.samples()[frame * 2];
}

void contributor_resolution_is_complete_and_deterministic() {
  auto project = snapshot({video_asset(1, "video"), audio_asset(2, "a"),
                           audio_asset(3, "b"), audio_asset(4, "missing", kMixRate, 1,
                                                           application::MediaAvailability::Unavailable)},
                          ClipSpec{1, 1, 0, 0, 1024},
                          {{{2, 2, 0, 0, 1024}}, {{3, 3, 0, 0, 1024}},
                           {{4, 4, 0, 0, 1024}}});
  const auto resolved = application::resolve_audio_contributors(
      project, domain::TimeRange{frames(0), frames(1024)});
  assert(resolved.contributors.size() == 3);
  assert(resolved.contributors[0].track_kind == domain::TrackKind::Video);
  assert(resolved.contributors[0].clip_id == domain::ClipId{1});
  assert(resolved.contributors[1].clip_id == domain::ClipId{2});
  assert(resolved.contributors[2].clip_id == domain::ClipId{3});
  assert(resolved.unavailable.size() == 1);
}

void mix_alignment_gain_mute_and_clipping() {
  SourceBank bank{{{"video", SignalSpec{.constant = 0.25F}},
                   {"sfx", SignalSpec{.constant = 0.50F}},
                   {"muted", SignalSpec{.constant = 0.80F}}}};
  auto project = snapshot({video_asset(1, "video"), audio_asset(2, "sfx"),
                           audio_asset(3, "muted")},
                          ClipSpec{1, 1, 0, 0, 1024},
                          {{{2, 2, 256, 100, 512}},
                           {{3, 3, 0, 0, 1024, 0.0, true}}});
  application::AudioMixSession mixer{bank.factory()};
  mixer.configure(std::move(project));
  const auto result = mixer.render(frames(0), 1024);
  const auto& metrics = mixer.metrics();
  assert(metrics.timestamp_observations > 0);
  assert(metrics.timestamp_inserted_silence_frames == 0);
  assert(metrics.timestamp_trimmed_frames == 0);
  assert(result.mixed_contributors == 2);
  assert(result.usable_contributors == 3);
  assert_near(left(result.block, 100), 0.25F);
  assert_near(left(result.block, 300), 0.75F);
  assert_near(left(result.block, 900), 0.25F);

  SourceBank clip_bank{{{"a", SignalSpec{.constant = 0.75F}},
                        {"b", SignalSpec{.constant = 0.75F}}}};
  application::AudioMixSession clipping{clip_bank.factory()};
  clipping.configure(snapshot({audio_asset(1, "a"), audio_asset(2, "b")}, std::nullopt,
                              {{{1, 1, 0, 0, 1024}}, {{2, 2, 0, 0, 1024}}}));
  const auto clipped = clipping.render(frames(0), 1024);
  assert_near(left(clipped.block, 10), application::soft_output_stage(1.5F));   // bent, not chopped
  assert(left(clipped.block, 10) < 1.0F && left(clipped.block, 10) > application::kOutputKnee);
}

// Fades ramp on the output grid from the clip's own first frame, whatever the block boundaries.
void fades_ramp_in_the_mixer_across_blocks() {
  const auto render_blocks = [](const std::vector<std::size_t>& sizes) {
    SourceBank bank{{{"sfx", SignalSpec{.constant = 0.50F}}}};
    auto project = snapshot({audio_asset(2, "sfx")}, std::nullopt,
                            {{{2, 2, 100, 0, 1024, 0.0, false, 256, 256}}});
    application::AudioMixSession mixer{bank.factory()};
    mixer.configure(std::move(project));
    std::vector<float> out;
    std::int64_t position = 0;
    for (const auto size : sizes) {
      const auto result = mixer.render(frames(position), size);
      for (std::size_t frame = 0; frame < size; ++frame) out.push_back(left(result.block, frame));
      position += static_cast<std::int64_t>(size);
    }
    return out;
  };
  const auto whole = render_blocks({1300});
  assert(whole[99] == 0.0F && whole[100] == 0.0F);                // before the clip / its first (silent) frame
  assert_near(whole[100 + 128], 0.25F, 2.0e-3F);                  // halfway up the 256-frame fade-in
  assert_near(whole[100 + 256], 0.5F);                            // full level
  assert_near(whole[100 + 700], 0.5F);
  assert_near(whole[100 + 1024 - 128], 0.25F, 2.0e-3F);           // halfway down the fade-out
  assert(whole[100 + 1023] > 0.0F && whole[100 + 1023] < 0.01F);  // nearly silent at the last frame
  assert(whole[100 + 1024] == 0.0F);                              // nothing after the clip
  assert(render_blocks({333, 333, 334, 300}) == whole);           // block boundaries do not matter
}

void mute_preserves_decode_and_clock_contract() {
  SourceBank normal_bank{{{"normal", SignalSpec{.constant = 0.4F}}}};
  application::AudioMixSession normal{normal_bank.factory()};
  normal.configure(snapshot({audio_asset(1, "normal")}, std::nullopt,
                            {{{1, 1, 0, 0, 2048}}}));
  const auto audible = normal.render(frames(0), 256);
  assert(audible.usable_contributors == 1);
  assert(audible.mixed_contributors == 1);
  assert(audible.clock == application::AudioMixClockRecommendation::Device);
  assert_near(left(audible.block, 10), 0.4F);

  SourceBank muted_bank{{{"muted", SignalSpec{.constant = 0.4F}}}};
  application::AudioMixSession muted{muted_bank.factory()};
  muted.configure(snapshot({audio_asset(1, "muted")}, std::nullopt,
                           {{{1, 1, 0, 0, 2048, 0.0, true}}}));
  const auto first = muted.render(frames(0), 256);
  assert(first.usable_contributors == 1);
  assert(first.mixed_contributors == 0);
  assert(first.clock == application::AudioMixClockRecommendation::Device);
  assert(std::all_of(first.block.samples().begin(), first.block.samples().end(),
                     [](float value) { return value == 0.0F; }));
  const auto decoded_after_first = muted_bank.decoded_frames;
  const auto second = muted.render(frames(256), 256);
  assert(second.usable_contributors == 1);
  assert(second.clock == application::AudioMixClockRecommendation::Device);
  assert(muted_bank.decoded_frames > decoded_after_first);
  assert(muted_bank.opens == 1);
  assert(muted_bank.seek_targets.size() == 1);
}

void mute_transitions_reanchor_to_current_truth_without_replay() {
  SourceBank bank{{{"signal", SignalSpec{.constant = 0.2F, .impulse_frame = 300}}}};
  application::AudioMixSession mixer{bank.factory()};

  mixer.configure(snapshot({audio_asset(1, "signal")}, std::nullopt,
                           {{{1, 1, 0, 0, 2048, 0.0, true}}}, 1));
  const auto muted_first = mixer.render(frames(0), 256);
  assert(muted_first.usable_contributors == 1);
  assert(muted_first.mixed_contributors == 0);

  mixer.configure(snapshot({audio_asset(1, "signal")}, std::nullopt,
                           {{{1, 1, 0, 0, 2048, 0.0, false}}}, 2));
  const auto unmuted = mixer.render(frames(256), 256);
  assert(unmuted.usable_contributors == 1);
  assert(unmuted.mixed_contributors == 1);
  assert_near(left(unmuted.block, 44), application::soft_output_stage(1.0F));
  assert_near(left(unmuted.block, 0), 0.2F);

  mixer.configure(snapshot({audio_asset(1, "signal")}, std::nullopt,
                           {{{1, 1, 0, 0, 2048, 0.0, true}}}, 3));
  const auto muted_again = mixer.render(frames(512), 256);
  assert(muted_again.usable_contributors == 1);
  assert(muted_again.mixed_contributors == 0);
  assert(muted_again.clock == application::AudioMixClockRecommendation::Device);

  mixer.configure(snapshot({audio_asset(1, "signal")}, std::nullopt,
                           {{{1, 1, 0, 0, 2048, 0.0, false}}}, 4));
  const auto unmuted_again = mixer.render(frames(768), 256);
  assert(unmuted_again.usable_contributors == 1);
  assert(unmuted_again.mixed_contributors == 1);
  assert_near(left(unmuted_again.block, 0), 0.2F);
  assert(bank.opens == 1);
  assert(bank.seek_targets.size() == 1);
  assert(bank.seek_targets[0] == frames(0));
}

void muted_embedded_and_parallel_audio_preserve_timing() {
  SourceBank bank{{{"embedded", SignalSpec{.constant = 0.7F}},
                   {"healthy", SignalSpec{.constant = 0.25F}}}};
  application::AudioMixSession mixer{bank.factory()};
  mixer.configure(snapshot({video_asset(1, "embedded"), audio_asset(2, "healthy")},
                           ClipSpec{1, 1, 0, 0, 1024, 0.0, true},
                           {{{2, 2, 0, 0, 1024}}}));
  const auto result = mixer.render(frames(0), 512);
  assert(result.usable_contributors == 2);
  assert(result.mixed_contributors == 1);
  assert(result.clock == application::AudioMixClockRecommendation::Device);
  assert_near(left(result.block, 100), 0.25F);
  assert(bank.opens == 2);
}

void muted_starvation_remains_observable() {
  SourceBank bank{{{"starved", SignalSpec{.constant = 0.5F, .starve = true}}}};
  application::AudioMixSession mixer{bank.factory()};
  mixer.configure(snapshot({audio_asset(1, "starved")}, std::nullopt,
                           {{{1, 1, 0, 0, 1024, 0.0, true}}}));
  const auto result = mixer.render(frames(0), 256);
  assert(result.usable_contributors == 0);
  assert(result.mixed_contributors == 0);
  assert(result.clock == application::AudioMixClockRecommendation::Software);
  assert(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue) {
    return issue.failure == application::AudioContributorFailure::Starved;
  }));
  assert(bank.opens == 1);
}

void nonzero_source_trim_split_and_gap_mapping() {
  SourceBank bank{{{"impulse", SignalSpec{.constant = 0.0F, .impulse_frame = 600}}}};
  application::AudioMixSession mixer{bank.factory()};
  mixer.configure(snapshot({audio_asset(1, "impulse")}, std::nullopt,
                           {{{1, 1, 0, 500, 512}, {2, 1, 768, 1012, 256}}}));
  const auto first = mixer.render(frames(0), 1024);
  assert_near(left(first.block, 100), application::soft_output_stage(1.0F));
  assert_near(left(first.block, 600), 0.0F);
  assert_near(left(first.block, 700), 0.0F);
  assert(first.clock == application::AudioMixClockRecommendation::Device);
}

void gain_and_runtime_stream_origin_are_applied() {
  SourceBank bank{{{"origin", SignalSpec{.constant = 0.5F,
                                          .origin_frame = kMixRate * 5,
                                          .has_origin = true}}}};
  application::AudioMixSession mixer{bank.factory()};
  mixer.configure(snapshot({audio_asset(1, "origin")}, std::nullopt,
                           {{{1, 1, 0, 240, 1024, -6.0, false}}}));
  const auto result = mixer.render(frames(0), 512);
  const auto expected = 0.5F * static_cast<float>(std::pow(10.0, -6.0 / 20.0));
  assert_near(left(result.block, 100), expected);
  assert(result.issues.empty());
}

void editorial_split_is_pcm_equivalent_to_unsplit_source() {
  SignalSpec signal{.block_frames = 137, .constant = 0.1F, .impulse_frame = 1024};
  SourceBank unsplit_bank{{{"signal", signal}}};
  application::AudioMixSession unsplit{unsplit_bank.factory()};
  unsplit.configure(snapshot({audio_asset(1, "signal")}, std::nullopt,
                             {{{1, 1, 0, 0, 2048}}}));
  const auto expected = unsplit.render(frames(0), 2048).block.samples();

  SourceBank split_bank{{{"signal", signal}}};
  application::AudioMixSession split{split_bank.factory()};
  split.configure(snapshot({audio_asset(1, "signal")}, std::nullopt,
                           {{{1, 1, 0, 0, 1024}, {2, 1, 1024, 1024, 1024}}}));
  std::vector<float> actual;
  const auto first_half = split.render(frames(0), 1024).block.samples();
  const auto second_half = split.render(frames(1024), 1024).block.samples();
  actual.insert(actual.end(), first_half.begin(), first_half.end());
  actual.insert(actual.end(), second_half.begin(), second_half.end());
  assert(expected == actual);
  assert_near(actual[1024 * 2], application::soft_output_stage(1.0F));
  assert(split_bank.opens == 1);
  assert(split.metrics().source_opens == 1);
}

void rates_channels_seek_and_block_invariance() {
  SourceBank bank{{{"mono44", SignalSpec{.rate = 44'100, .channels = 1,
                                          .total_frames = 44'100 * 4U,
                                          .block_frames = 113, .constant = 0.2F}},
                   {"stereo48", SignalSpec{.channels = 2, .block_frames = 97,
                                            .constant = 0.3F}}}};
  auto project = snapshot({audio_asset(1, "mono44", 44'100, 1),
                           audio_asset(2, "stereo48", 48'000, 2)},
                          std::nullopt,
                          {{{1, 1, 0, 0, 8192}}, {{2, 2, 0, 0, 8192}}});
  application::AudioMixSession contiguous{bank.factory()};
  contiguous.configure(project);
  const auto whole = contiguous.render(frames(0), 4096).block.samples();

  application::AudioMixSession chunked{bank.factory()};
  chunked.configure(project);
  std::vector<float> pieces;
  for (int block = 0; block < 4; ++block) {
    const auto part = chunked.render(frames(block * 1024), 1024);
    pieces.insert(pieces.end(), part.block.samples().begin(), part.block.samples().end());
  }
  assert(whole.size() == pieces.size());
  for (std::size_t index = 0; index < whole.size(); ++index) {
    assert_near(whole[index], pieces[index], 2.0e-4F);
  }
  assert_near(whole[100], 0.5F, 2.0e-4F);

  const auto before_seek_opens = bank.opens;
  const auto seeked = chunked.render(frames(6000), 512);
  assert(bank.opens > before_seek_opens);
  assert_near(left(seeked.block, 20), 0.5F, 2.0e-4F);
  const auto backward = chunked.render(frames(1000), 512);
  assert_near(left(backward.block, 20), 0.5F, 2.0e-4F);
  const domain::MediaTime awkward_position{1, domain::Rational{1, 7}};
  const auto awkward = chunked.render(awkward_position, 512);
  assert(awkward.timeline_position == awkward_position);
  assert_near(left(awkward.block, 20), 0.5F, 2.0e-4F);
}

void failures_are_isolated_and_generation_invalidates_pcm() {
  SourceBank bank{{{"good", SignalSpec{.constant = 0.25F}},
                   {"starved", SignalSpec{.constant = 0.5F, .starve = true}},
                   {"broken", SignalSpec{.constant = 0.5F, .throw_decode = true}},
                   {"short", SignalSpec{.total_frames = 10, .constant = 0.5F}},
                   {"surround", SignalSpec{.channels = 6, .constant = 0.5F}}}};
  auto project = snapshot({audio_asset(1, "good"), audio_asset(2, "starved"),
                           audio_asset(3, "broken"), audio_asset(4, "short"),
                           audio_asset(5, "missing", kMixRate, 1,
                                       application::MediaAvailability::Unavailable),
                           audio_asset(6, "surround", kMixRate, 6)},
                          std::nullopt,
                          {{{1, 1, 0, 0, 1024}}, {{2, 2, 0, 0, 1024}},
                           {{3, 3, 0, 0, 1024}}, {{4, 4, 0, 0, 1024}},
                           {{5, 5, 0, 0, 1024}}, {{6, 6, 0, 0, 1024}}},
                          10);
  application::AudioMixSession mixer{bank.factory()};
  mixer.configure(project);
  const auto first_generation = mixer.generation();
  const auto mixed = mixer.render(frames(0), 512);
  assert_near(left(mixed.block, 100), 0.25F);
  assert(std::any_of(mixed.issues.begin(), mixed.issues.end(), [](const auto& issue) {
    return issue.failure == application::AudioContributorFailure::Starved;
  }));
  assert(std::any_of(mixed.issues.begin(), mixed.issues.end(), [](const auto& issue) {
    return issue.failure == application::AudioContributorFailure::Decode;
  }));
  assert(std::any_of(mixed.issues.begin(), mixed.issues.end(), [](const auto& issue) {
    return issue.failure == application::AudioContributorFailure::EndedEarly;
  }));
  assert(std::any_of(mixed.issues.begin(), mixed.issues.end(), [](const auto& issue) {
    return issue.failure == application::AudioContributorFailure::Unavailable;
  }));
  assert(std::any_of(mixed.issues.begin(), mixed.issues.end(), [](const auto& issue) {
    return issue.failure == application::AudioContributorFailure::UnsupportedChannels;
  }));

  const auto opens_before = bank.opens;
  auto edited = snapshot({}, std::nullopt, {}, 11);
  mixer.configure(std::move(edited));
  assert(mixer.generation() > first_generation);
  const auto after_edit = mixer.render(frames(0), 512);
  assert(after_edit.mixed_contributors == 0);
  assert_near(left(after_edit.block, 100), 0.0F);
  assert(bank.opens == opens_before);
  assert(mixer.metrics().live_sources == 0);
}

void empty_mix_uses_software_clock_and_bounds_are_enforced() {
  SourceBank bank;
  application::AudioMixSession mixer{bank.factory()};
  mixer.configure(snapshot({}, std::nullopt, {}));
  const auto silent = mixer.render(frames(0), 256);
  assert(silent.clock == application::AudioMixClockRecommendation::Software);
  assert(std::all_of(silent.block.samples().begin(), silent.block.samples().end(),
                     [](float value) { return value == 0.0F; }));
  bool rejected = false;
  try {
    (void)mixer.render(frames(0), application::AudioMixSession::maximum_render_frames + 1);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void benchmark_mix_scaling() {
  for (const int contributor_count : {1, 4, 8, 16}) {
    SourceBank bank;
    std::vector<application::MediaAsset> assets;
    std::vector<std::vector<ClipSpec>> tracks;
    for (int index = 0; index < contributor_count; ++index) {
      const auto name = "bench-" + std::to_string(index);
      bank.specs.emplace(name, SignalSpec{.total_frames = kMixRate * 8U,
                                          .block_frames = 1024, .constant = 0.01F});
      assets.push_back(audio_asset(static_cast<std::uint64_t>(index + 1), name));
      tracks.push_back({ClipSpec{static_cast<std::uint64_t>(index + 1),
                                 static_cast<std::uint64_t>(index + 1), 0, 0,
                                 kMixRate * 8}});
    }
    application::AudioMixSession mixer{bank.factory()};
    mixer.configure(snapshot(std::move(assets), std::nullopt, std::move(tracks)));
    constexpr int blocks = 100;
    constexpr std::size_t block_frames = 1024;
    const auto started = std::chrono::steady_clock::now();
    for (int block = 0; block < blocks; ++block) {
      const auto result = mixer.render(frames(block * static_cast<int>(block_frames)),
                                       block_frames);
      assert(result.mixed_contributors == static_cast<std::size_t>(contributor_count));
    }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started);
    const double audio_seconds = static_cast<double>(blocks * block_frames) / kMixRate;
    const double realtime_factor = audio_seconds / elapsed.count();
    assert(mixer.metrics().source_opens == static_cast<std::uint64_t>(contributor_count));
    assert(mixer.metrics().peak_queued_frames <=
           static_cast<std::size_t>(contributor_count) *
               application::AudioMixSession::maximum_queued_frames_per_source);
    std::cout << "audio_mix_benchmark contributors=" << contributor_count
              << " realtime_factor=" << realtime_factor
              << " peak_sources=" << mixer.metrics().peak_live_sources
              << " peak_queued_frames=" << mixer.metrics().peak_queued_frames << '\n';
    assert(realtime_factor > 1.0);
  }
}

} // namespace

int main() {
  contributor_resolution_is_complete_and_deterministic();
  mix_alignment_gain_mute_and_clipping();
  fades_ramp_in_the_mixer_across_blocks();
  mute_preserves_decode_and_clock_contract();
  mute_transitions_reanchor_to_current_truth_without_replay();
  muted_embedded_and_parallel_audio_preserve_timing();
  muted_starvation_remains_observable();
  nonzero_source_trim_split_and_gap_mapping();
  gain_and_runtime_stream_origin_are_applied();
  editorial_split_is_pcm_equivalent_to_unsplit_source();
  rates_channels_seek_and_block_invariance();
  failures_are_isolated_and_generation_invalidates_pcm();
  empty_mix_uses_software_clock_and_bounds_are_enforced();
  benchmark_mix_scaling();
}
