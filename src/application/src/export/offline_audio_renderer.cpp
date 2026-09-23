#include <pulsar/application/export/offline_audio_renderer.hpp>

#include <pulsar/application/audio/clip_gain.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

namespace pulsar::application::exporting {
namespace {

[[nodiscard]] domain::MediaTime source_sample_time(std::int64_t index, int rate) {
  if (rate <= 0 || index < 0) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::InvalidSourceFormat,
                                  "source sample time has invalid rate or index"};
  }
  return domain::MediaTime::from_frame(domain::FrameNumber{index},
                                       domain::FrameRate{rate, 1});
}

[[nodiscard]] std::size_t checked_sample_count(std::size_t frames, int channels) {
  if (channels <= 0 || frames > std::numeric_limits<std::size_t>::max() /
                                  static_cast<std::size_t>(channels)) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::InvalidSourceFormat,
                                  "audio sample buffer size is invalid"};
  }
  return frames * static_cast<std::size_t>(channels);
}

[[nodiscard]] long double interpolation_fraction(domain::MediaTime time,
                                                 domain::MediaTime sample_time,
                                                 int sample_rate) {
  const auto delta = time - sample_time;
  const auto seconds = delta.seconds();
  return static_cast<long double>(seconds.numerator()) /
         static_cast<long double>(seconds.denominator()) * static_cast<long double>(sample_rate);
}

} // namespace

struct OfflineAudioRenderer::SourceBlock {
  int sample_rate{0};
  int channel_count{0};
  domain::MediaTime start{domain::MediaTime::zero()};
  domain::MediaTime end{domain::MediaTime::zero()};
  std::vector<float> samples;
};

struct OfflineAudioRenderer::ContributorState {
  AudioContributor contributor;
  std::unique_ptr<OfflineAudioSource> source;
  domain::MediaTime stream_origin{domain::MediaTime::zero()};
  std::deque<SourceBlock> blocks;
  bool eof{false};
  std::optional<domain::MediaTime> previous_end;
  ClipGain gain;
};

OfflineAudioRenderer::OfflineAudioRenderer(const ExportPlan& plan,
                                           OfflineAudioSourceFactory& source_factory,
                                           std::size_t max_render_frames)
    : plan_{plan}, source_factory_{source_factory}, max_render_frames_{max_render_frames} {
  if (max_render_frames_ == 0) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::InternalInvariant,
                                  "offline audio render block size must be positive"};
  }
}

OfflineAudioRenderer::~OfflineAudioRenderer() = default;

OfflineAudioRenderer::ContributorState& OfflineAudioRenderer::state_for(
    const AudioContributor& contributor) {
  const auto key = contributor.clip_id.value();
  const auto found = states_.find(key);
  if (found != states_.end()) return *found->second;

  std::unique_ptr<OfflineAudioSource> source;
  try {
    source = source_factory_.open(contributor.source_locator, contributor.stream_index);
  } catch (const OfflineAudioRenderError&) {
    throw;
  } catch (const std::exception& error) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::SourceOpenFailed, error.what()};
  }
  if (!source) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::SourceOpenFailed,
                                  "audio source factory returned no source"};
  }
  if (source->sample_rate() <= 0 || source->channel_count() <= 0) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::InvalidSourceFormat,
                                  "audio source reported invalid format"};
  }
  if (source->channel_count() > 2) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::UnsupportedChannels,
                                  "audio sources with more than two channels are unsupported"};
  }

  const auto origin = source->stream_origin().value_or(domain::MediaTime::zero());
  auto state = std::make_unique<ContributorState>(ContributorState{
      contributor, std::move(source), origin, {}, false, std::nullopt,
      ClipGain::from_decibels(contributor.gain_db, contributor.muted)});
  const auto raw_source_start = contributor_source_position(
      contributor, contributor.timeline_range.start(), state->stream_origin);
  try {
    state->source->seek_to(raw_source_start);
  } catch (const OfflineAudioRenderError&) {
    throw;
  } catch (const std::exception& error) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::SeekFailed, error.what()};
  }
  ++metrics_.source_opens;
  ++metrics_.seeks;
  auto* result = state.get();
  states_.emplace(key, std::move(state));
  return *result;
}

void OfflineAudioRenderer::load_until(ContributorState& state, domain::MediaTime relative_time) {
  while (!state.eof &&
         (state.blocks.empty() || state.blocks.back().end <= relative_time)) {
    std::optional<OfflineAudioSourceBlock> decoded;
    try {
      decoded = state.source->decode_next();
    } catch (const OfflineAudioRenderError&) {
      throw;
    } catch (const std::exception& error) {
      throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::DecodeFailed, error.what()};
    }
    if (!decoded.has_value()) {
      state.eof = true;
      break;
    }
    ++metrics_.decoded_blocks;
    metrics_.decoded_source_frames += decoded->frame_count;
    if (!decoded->presentation_timestamp.has_value()) {
      throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::TimestampMissing,
                                    "audio source block has no presentation timestamp"};
    }
    if (decoded->sample_rate != state.source->sample_rate() ||
        decoded->channel_count != state.source->channel_count() ||
        decoded->sample_rate <= 0 || decoded->channel_count <= 0 ||
        decoded->channel_count > 2 ||
        decoded->frame_count == 0 ||
        decoded->samples.size() != checked_sample_count(decoded->frame_count,
                                                         decoded->channel_count)) {
      throw OfflineAudioRenderError{decoded->channel_count > 2
                                        ? OfflineAudioRenderErrorCode::UnsupportedChannels
                                        : OfflineAudioRenderErrorCode::InvalidSourceFormat,
                                    "audio source block has invalid format or sample count"};
    }

    const auto start = *decoded->presentation_timestamp - state.stream_origin;
    auto end = start + source_sample_time(static_cast<std::int64_t>(decoded->frame_count),
                                          decoded->sample_rate);
    std::vector<float> samples = std::move(decoded->samples);
    auto effective_start = start;
    if (state.previous_end.has_value() && start < *state.previous_end) {
      const auto overlap = *state.previous_end - start;
      const auto trim = domain::frame_at(overlap,
                                         domain::FrameRate{decoded->sample_rate, 1},
                                         domain::RoundingMode::Ceil).value();
      if (trim >= static_cast<std::int64_t>(decoded->frame_count)) {
        metrics_.trimmed_overlap_frames += decoded->frame_count;
        continue;
      }
      const auto trim_count = static_cast<std::size_t>(std::max<std::int64_t>(0, trim));
      samples.erase(samples.begin(), samples.begin() +
                                    static_cast<std::ptrdiff_t>(checked_sample_count(
                                        trim_count, decoded->channel_count)));
      effective_start = start + source_sample_time(trim, decoded->sample_rate);
      metrics_.trimmed_overlap_frames += trim_count;
    }
    end = effective_start + source_sample_time(
                              static_cast<std::int64_t>(samples.size() /
                                                        static_cast<std::size_t>(decoded->channel_count)),
                              decoded->sample_rate);
    if (state.previous_end.has_value() && effective_start > *state.previous_end) {
      // The gap is represented by absence between two bounded blocks. It is
      // counted when samples are requested from that interval.
    }
    state.previous_end = end;
    state.blocks.push_back(SourceBlock{decoded->sample_rate, decoded->channel_count,
                                       effective_start, end, std::move(samples)});
    update_buffer_metrics();
  }
  while (state.blocks.size() > 2 && state.blocks.front().end <= relative_time) {
    state.blocks.pop_front();
  }
  update_buffer_metrics();
}

float OfflineAudioRenderer::channel_sample(const SourceBlock& block, std::int64_t index,
                                           int channel) {
  if (index < 0 || channel < 0 || channel > 1 ||
      static_cast<std::size_t>(index) >= block.samples.size() /
                                           static_cast<std::size_t>(block.channel_count)) {
    return 0.0F;
  }
  const auto offset = static_cast<std::size_t>(index) *
                      static_cast<std::size_t>(block.channel_count);
  if (block.channel_count == 1) return block.samples[offset];
  if (block.channel_count <= channel) return 0.0F;
  return block.samples[offset + static_cast<std::size_t>(channel)];
}

float OfflineAudioRenderer::sample_at_channel(ContributorState& state,
                                              domain::MediaTime relative_time, int channel,
                                              bool allow_outside_source_range) {
  if (relative_time < state.contributor.source_range.start() ||
      relative_time >= state.contributor.source_range.end()) {
    if (allow_outside_source_range) return 0.0F;
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::InternalInvariant,
                                  "audio sample request escaped source range"};
  }
  load_until(state, relative_time);
  for (const auto& block : state.blocks) {
    if (relative_time < block.start || relative_time >= block.end) continue;
    const auto offset = relative_time - block.start;
    const auto index = domain::frame_at(offset, domain::FrameRate{block.sample_rate, 1},
                                        domain::RoundingMode::Floor).value();
    return channel_sample(block, index, channel);
  }
  if (state.eof && relative_time < state.contributor.source_range.end()) {
    if (channel == 0) {
      throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::SourceEndedEarly,
                                    "audio source ended before the promised source range"};
    }
  }
  if (channel == 0) ++metrics_.inserted_source_gap_silence;
  return 0.0F;
}

std::array<float, 2> OfflineAudioRenderer::contributor_sample(
    ContributorState& state, domain::MediaTime timeline_time) {
  if (!state.contributor.timeline_range.contains(timeline_time)) return {0.0F, 0.0F};
  const auto source_time = contributor_source_position(state.contributor, timeline_time,
                                                       state.stream_origin) - state.stream_origin;
  const auto source_rate = state.source->sample_rate();
  const auto source_index = domain::frame_at(
      source_time, domain::FrameRate{source_rate, 1}, domain::RoundingMode::Floor).value();
  const auto source_grid_time = source_sample_time(source_index, source_rate);
  const auto fraction = interpolation_fraction(source_time, source_grid_time, source_rate);
  if (state.gain.muted) return {0.0F, 0.0F};
  const auto next_grid_time = source_grid_time + source_sample_time(1, source_rate);
  std::array<float, 2> result{};
  for (int channel = 0; channel < 2; ++channel) {
    const auto first = sample_at_channel(state, source_grid_time, channel, false);
    const auto second = sample_at_channel(
        state, next_grid_time, channel, next_grid_time >= state.contributor.source_range.end());
    result[static_cast<std::size_t>(channel)] = static_cast<float>(
        (static_cast<long double>(first) * (1.0L - fraction) +
         static_cast<long double>(second) * fraction) * state.gain.linear);
  }
  return result;
}

void OfflineAudioRenderer::update_buffer_metrics() noexcept {
  std::uint64_t buffered = 0;
  for (const auto& [_, state] : states_) {
    for (const auto& block : state->blocks) buffered += block.samples.size() /
                                                        static_cast<std::size_t>(
                                                            std::max(1, block.channel_count));
  }
  metrics_.peak_buffered_source_frames = std::max(metrics_.peak_buffered_source_frames, buffered);
}

void OfflineAudioRenderer::prune_inactive_states(const std::vector<AudioContributor>& active) {
  std::unordered_map<std::uint64_t, bool> keep;
  keep.reserve(active.size());
  for (const auto& contributor : active) keep.emplace(contributor.clip_id.value(), true);
  for (auto iterator = states_.begin(); iterator != states_.end();) {
    if (keep.find(iterator->first) == keep.end()) iterator = states_.erase(iterator);
    else ++iterator;
  }
}

std::optional<OfflineAudioBlock> OfflineAudioRenderer::render_next_block() {
  if (next_sample_index_ >= plan_.audio_sample_count()) return std::nullopt;
  const auto remaining = plan_.audio_sample_count() - next_sample_index_;
  const auto frame_count = static_cast<std::size_t>(std::min<std::int64_t>(
      remaining, static_cast<std::int64_t>(max_render_frames_)));
  const auto first = next_sample_index_;
  const auto start_time = audio_sample_time(first);
  const auto end_time = audio_sample_time(first + static_cast<std::int64_t>(frame_count));
  auto resolution = resolve_audio_contributors(plan_.snapshot(),
                                               domain::TimeRange{start_time, end_time});
  // Export video follows the single master video track frozen by ExportPlan.
  // Ignore embedded audio attached to any auxiliary video track so the same
  // source cannot enter the mix twice if a project contains one later.
  const auto master_track = plan_.snapshot().video_track_id;
  resolution.contributors.erase(
      std::remove_if(resolution.contributors.begin(), resolution.contributors.end(),
                     [master_track](const AudioContributor& contributor) {
                       return contributor.track_kind == domain::TrackKind::Video &&
                              contributor.track_id != master_track;
                     }),
      resolution.contributors.end());
  resolution.unavailable.erase(
      std::remove_if(resolution.unavailable.begin(), resolution.unavailable.end(),
                     [master_track](const UnavailableAudioContributor& contributor) {
                       return contributor.track_id != master_track;
                     }),
      resolution.unavailable.end());
  if (!resolution.unavailable.empty()) {
    throw OfflineAudioRenderError{OfflineAudioRenderErrorCode::MissingMedia,
                                  "audio timeline contains an unavailable contributor"};
  }
  prune_inactive_states(resolution.contributors);
  metrics_.peak_active_sources = std::max<std::uint64_t>(metrics_.peak_active_sources,
                                                         resolution.contributors.size());
  std::vector<ContributorState*> states;
  states.reserve(resolution.contributors.size());
  for (const auto& contributor : resolution.contributors) states.push_back(&state_for(contributor));

  std::vector<float> samples(frame_count * 2, 0.0F);
  for (std::size_t offset = 0; offset < frame_count; ++offset) {
    const auto timeline_time = audio_sample_time(first + static_cast<std::int64_t>(offset));
    long double left = 0.0L;
    long double right = 0.0L;
    for (auto* state : states) {
      const auto value = contributor_sample(*state, timeline_time);
      left += value[0];
      right += value[1];
    }
    samples[offset * 2] = static_cast<float>(left);
    samples[offset * 2 + 1] = static_cast<float>(right);
  }
  next_sample_index_ += static_cast<std::int64_t>(frame_count);
  metrics_.output_samples += frame_count;
  return OfflineAudioBlock{first, frame_count, 48'000, 2, std::move(samples)};
}

} // namespace pulsar::application::exporting
