#include <pulsar/application/audio/audio_mix_session.hpp>

#include <pulsar/application/audio/audio_converter.hpp>
#include <pulsar/application/audio/clip_gain.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace pulsar::application {
namespace {

[[nodiscard]] std::size_t checked_sample_count(std::size_t frames, int channels) {
  if (channels <= 0 || frames > std::numeric_limits<std::size_t>::max() /
                                  static_cast<std::size_t>(channels)) {
    throw std::overflow_error{"audio mix sample count overflow"};
  }
  return frames * static_cast<std::size_t>(channels);
}

[[nodiscard]] std::size_t frames_ceil(domain::MediaTime duration, int sample_rate) {
  if (duration <= domain::MediaTime::zero()) {
    return 0;
  }
  const auto seconds = duration.seconds();
  const auto numerator = static_cast<__int128_t>(seconds.numerator()) * sample_rate;
  const auto denominator = static_cast<__int128_t>(seconds.denominator());
  const auto frames = (numerator + denominator - 1) / denominator;
  if (frames > static_cast<__int128_t>(std::numeric_limits<std::size_t>::max())) {
    throw std::overflow_error{"audio mix frame conversion overflow"};
  }
  return static_cast<std::size_t>(frames);
}

[[nodiscard]] std::size_t frames_floor(domain::MediaTime duration, int sample_rate) {
  if (duration <= domain::MediaTime::zero()) {
    return 0;
  }
  const auto seconds = duration.seconds();
  const auto numerator = static_cast<__int128_t>(seconds.numerator()) * sample_rate;
  const auto denominator = static_cast<__int128_t>(seconds.denominator());
  const auto frames = numerator / denominator;
  if (frames > static_cast<__int128_t>(std::numeric_limits<std::size_t>::max())) {
    throw std::overflow_error{"audio source frame conversion overflow"};
  }
  return static_cast<std::size_t>(frames);
}

[[nodiscard]] bool same_identity(const AudioContributor& lhs,
                                 const AudioContributor& rhs) noexcept {
  return lhs.track_id == rhs.track_id && lhs.clip_id == rhs.clip_id &&
         lhs.media_id == rhs.media_id;
}

[[nodiscard]] bool same_source_mapping(const AudioContributor& lhs,
                                       const AudioContributor& rhs) noexcept {
  return same_identity(lhs, rhs) && lhs.track_kind == rhs.track_kind &&
         lhs.track_order == rhs.track_order && lhs.source_locator == rhs.source_locator &&
         lhs.stream_index == rhs.stream_index &&
         lhs.timeline_range.start() == rhs.timeline_range.start() &&
         lhs.timeline_range.end() == rhs.timeline_range.end() &&
         lhs.source_range.start() == rhs.source_range.start() &&
         lhs.source_range.end() == rhs.source_range.end() &&
         lhs.stream_origin == rhs.stream_origin && lhs.sample_rate == rhs.sample_rate &&
         lhs.channel_count == rhs.channel_count && lhs.time_base == rhs.time_base;
}

[[nodiscard]] bool seamless_contiguous_successor(const AudioContributor& previous,
                                                 const AudioContributor& next) noexcept {
  return previous.track_id == next.track_id &&
         previous.track_kind == next.track_kind &&
         previous.track_order == next.track_order &&
         previous.media_id == next.media_id &&
         previous.source_locator == next.source_locator &&
         previous.stream_index == next.stream_index &&
         previous.timeline_range.end() == next.timeline_range.start() &&
         previous.source_range.end() == next.source_range.start() &&
         previous.stream_origin == next.stream_origin &&
         previous.sample_rate == next.sample_rate &&
         previous.channel_count == next.channel_count &&
         previous.time_base == next.time_base;
}

[[nodiscard]] bool same_unavailable_mapping(const UnavailableAudioContributor& lhs,
                                            const UnavailableAudioContributor& rhs) noexcept {
  return lhs.track_id == rhs.track_id && lhs.clip_id == rhs.clip_id &&
         lhs.media_id == rhs.media_id &&
         lhs.timeline_range.start() == rhs.timeline_range.start() &&
         lhs.timeline_range.end() == rhs.timeline_range.end() && lhs.reason == rhs.reason;
}

[[nodiscard]] AudioMixIssue issue_for(const AudioContributor& contributor,
                                      AudioContributorFailure failure,
                                      std::string message) {
  return {contributor.track_id, contributor.clip_id, contributor.media_id, failure,
          std::move(message)};
}

[[nodiscard]] bool timeline_order(const AudioContributor& lhs,
                                  const AudioContributor& rhs) noexcept {
  if (lhs.timeline_range.start() != rhs.timeline_range.start()) {
    return lhs.timeline_range.start() < rhs.timeline_range.start();
  }
  if (lhs.track_order != rhs.track_order) return lhs.track_order < rhs.track_order;
  return lhs.clip_id.value() < rhs.clip_id.value();
}

[[nodiscard]] bool mix_order(const AudioContributor& lhs,
                             const AudioContributor& rhs) noexcept {
  if (lhs.track_kind != rhs.track_kind) {
    return lhs.track_kind == domain::TrackKind::Video;
  }
  if (lhs.track_order != rhs.track_order) return lhs.track_order < rhs.track_order;
  if (lhs.timeline_range.start() != rhs.timeline_range.start()) {
    return lhs.timeline_range.start() < rhs.timeline_range.start();
  }
  return lhs.clip_id.value() < rhs.clip_id.value();
}

[[nodiscard]] bool unavailable_order(const UnavailableAudioContributor& lhs,
                                     const UnavailableAudioContributor& rhs) noexcept {
  if (lhs.timeline_range.start() != rhs.timeline_range.start()) {
    return lhs.timeline_range.start() < rhs.timeline_range.start();
  }
  if (lhs.track_id.value() != rhs.track_id.value()) {
    return lhs.track_id.value() < rhs.track_id.value();
  }
  return lhs.clip_id.value() < rhs.clip_id.value();
}

void trace_timestamp(const AudioContributor& contributor, domain::MediaTime block_start,
                     domain::MediaTime expected, std::size_t block_frames, int sample_rate,
                     std::size_t inserted_silence, std::size_t trimmed,
                     std::optional<domain::MediaTime> stream_origin,
                     domain::MediaTime initial_seek_target, std::uint64_t& trace_count) {
  const auto* enabled = std::getenv("PULSAR_AUDIO_TIMESTAMP_TRACE");
  if (enabled == nullptr || *enabled == '\0' || trace_count >= 32) return;
  ++trace_count;
  const auto delta = block_start - expected;
  const auto print_time = [](domain::MediaTime time) {
    const auto seconds = time.seconds();
    return std::to_string(seconds.numerator()) + "/" +
           std::to_string(seconds.denominator());
  };
  std::clog << "audio_timestamp contributor=" << contributor.clip_id.value()
            << " block_start=" << print_time(block_start)
            << " expected=" << print_time(expected)
            << " delta=" << print_time(delta) << " block_frames=" << block_frames
            << " sample_rate=" << sample_rate
            << " inserted_silence_frames=" << inserted_silence
            << " trimmed_frames=" << trimmed
            << " stream_origin="
            << (stream_origin.has_value() ? print_time(*stream_origin) : "none")
            << " initial_seek_target=" << print_time(initial_seek_target) << '\n';
}

} // namespace

class AudioMixSession::SourceState final {
public:
  SourceState(AudioContributor identity, std::unique_ptr<AudioPlaybackSource> opened_source,
              std::unique_ptr<StreamingAudioConverter> opened_converter,
              domain::MediaTime timeline_position,
              domain::MediaTime source_target,
              std::optional<domain::MediaTime> origin)
      : contributor{std::move(identity)}, source{std::move(opened_source)},
        converter{std::move(opened_converter)}, next_timeline{timeline_position},
        expected_source_timestamp{source_target}, stream_origin{origin},
        initial_seek_target{source_target} {}

  [[nodiscard]] std::size_t queued_frames(int channels) const noexcept {
    return (pcm.size() - pcm_offset) / static_cast<std::size_t>(channels);
  }

  void append(ConvertedPcmBlock converted, int channels) {
    if (converted.channel_count != channels) {
      throw std::logic_error{"mix converter produced unexpected channels"};
    }
    if (pcm_offset != 0 && pcm_offset * 2 >= pcm.size()) {
      pcm.erase(pcm.begin(), pcm.begin() + static_cast<std::ptrdiff_t>(pcm_offset));
      pcm_offset = 0;
    }
    pcm.insert(pcm.end(), converted.samples.begin(), converted.samples.end());
  }

  AudioContributor contributor;
  std::unique_ptr<AudioPlaybackSource> source;
  std::unique_ptr<StreamingAudioConverter> converter;
  domain::MediaTime next_timeline;
  std::optional<domain::MediaTime> expected_source_timestamp;
  std::optional<domain::MediaTime> stream_origin;
  domain::MediaTime initial_seek_target;
  std::vector<float> pcm;
  std::size_t pcm_offset{0};
  bool eof{false};
  bool tail_flushed{false};
  bool failed{false};
};

AudioMixSession::AudioMixSession(AudioPlaybackSourceFactory source_factory, AudioMixFormat format)
    : source_factory_{std::move(source_factory)}, format_{format} {
  if (format_.sample_rate <= 0 || format_.channel_count != 2) {
    throw std::invalid_argument{"preview mix format must be positive-rate stereo"};
  }
}

AudioMixSession::~AudioMixSession() = default;

void AudioMixSession::configure(ProjectSnapshot snapshot) {
  AudioContributorResolution resolution;
  const auto duration = snapshot.timeline.duration();
  if (duration > domain::MediaTime::zero()) {
    resolution = resolve_audio_contributors(
        snapshot, domain::TimeRange{domain::MediaTime::zero(), duration});
  }
  std::sort(resolution.contributors.begin(), resolution.contributors.end(), timeline_order);
  std::sort(resolution.unavailable.begin(), resolution.unavailable.end(), unavailable_order);
  const bool presentation_only_change =
      snapshot_.has_value() && next_timeline_position_.has_value() &&
      contributors_.size() == resolution.contributors.size() &&
      unavailable_.size() == resolution.unavailable.size() &&
      std::equal(contributors_.begin(), contributors_.end(), resolution.contributors.begin(),
                 same_source_mapping) &&
      std::equal(unavailable_.begin(), unavailable_.end(), resolution.unavailable.begin(),
                 same_unavailable_mapping);
  std::optional<ProjectSnapshot> next_snapshot{std::move(snapshot)};
  snapshot_.swap(next_snapshot);
  contributors_.swap(resolution.contributors);
  unavailable_.swap(resolution.unavailable);
  active_contributors_.clear();
  active_unavailable_.clear();
  next_contributor_ = 0;
  next_unavailable_ = 0;
  if (presentation_only_change) {
    for (auto& source : sources_) {
      const auto replacement = std::find_if(
          contributors_.begin(), contributors_.end(), [&](const auto& contributor) {
            return same_identity(source->contributor, contributor);
          });
      if (replacement != contributors_.end()) {
        source->contributor = *replacement;
      }
    }
    select_range(*next_timeline_position_, *next_timeline_position_, true);
  } else {
    sources_.clear();
    next_timeline_position_.reset();
  }
  ++generation_;
  update_metrics();
}

void AudioMixSession::reset() noexcept {
  sources_.clear();
  snapshot_.reset();
  contributors_.clear();
  unavailable_.clear();
  active_contributors_.clear();
  active_unavailable_.clear();
  next_contributor_ = 0;
  next_unavailable_ = 0;
  next_timeline_position_.reset();
  ++generation_;
  metrics_.live_sources = 0;
  metrics_.queued_frames = 0;
}

AudioMixSession::SourceState* AudioMixSession::find_source(
    const AudioContributor& contributor) noexcept {
  const auto found = std::find_if(sources_.begin(), sources_.end(),
                                  [&contributor](const auto& source) {
                                    return same_identity(source->contributor, contributor);
                                  });
  return found == sources_.end() ? nullptr : found->get();
}

AudioMixSession::SourceState* AudioMixSession::transfer_contiguous_source(
    const AudioContributor& contributor) noexcept {
  const auto found = std::find_if(sources_.begin(), sources_.end(),
                                  [&contributor](const auto& source) {
                                    return seamless_contiguous_successor(
                                        source->contributor, contributor);
                                  });
  if (found == sources_.end()) return nullptr;
  (*found)->contributor = contributor;
  return found->get();
}

AudioMixSession::SourceState* AudioMixSession::open_source(
    const AudioContributor& contributor, domain::MediaTime timeline_position,
    std::vector<AudioMixIssue>& issues) {
  std::unique_ptr<AudioPlaybackSource> source;
  try {
    source = source_factory_ ? source_factory_(contributor.source_locator,
                                               contributor.stream_index)
                             : nullptr;
  } catch (const std::exception& error) {
    issues.push_back(issue_for(contributor, AudioContributorFailure::SourceOpen, error.what()));
    return nullptr;
  } catch (...) {
    issues.push_back(issue_for(contributor, AudioContributorFailure::SourceOpen,
                               "audio source creation failed"));
    return nullptr;
  }
  if (!source) {
    issues.push_back(issue_for(contributor, AudioContributorFailure::SourceOpen,
                               "audio source factory returned no source"));
    return nullptr;
  }
  if (source->channel_count() < 1 || source->channel_count() > 2) {
    issues.push_back(issue_for(contributor, AudioContributorFailure::UnsupportedChannels,
                               "preview mixer supports mono or stereo contributors"));
    return nullptr;
  }
  std::optional<domain::MediaTime> origin;
  domain::MediaTime target = domain::MediaTime::zero();
  try {
    origin = source->stream_origin();
    target = contributor_source_position(contributor, timeline_position, origin);
    if (!source->seek_to(target)) {
      issues.push_back(issue_for(contributor, AudioContributorFailure::SourceSeek,
                                 "audio source seek failed"));
      return nullptr;
    }
  } catch (const std::exception& error) {
    issues.push_back(issue_for(contributor, AudioContributorFailure::SourceSeek, error.what()));
    return nullptr;
  } catch (...) {
    issues.push_back(issue_for(contributor, AudioContributorFailure::SourceSeek,
                               "audio source seek threw an unknown exception"));
    return nullptr;
  }

  std::unique_ptr<StreamingAudioConverter> converter;
  try {
    converter = std::make_unique<StreamingAudioConverter>(
        source->sample_rate(), source->channel_count(),
        AudioDeviceFormat{format_.sample_rate, format_.channel_count,
                          AudioSampleFormat::Float32});
  } catch (const AudioConversionError& error) {
    issues.push_back(issue_for(contributor, AudioContributorFailure::UnsupportedChannels,
                               error.what()));
    return nullptr;
  }
  auto state = std::make_unique<SourceState>(contributor, std::move(source),
                                             std::move(converter), timeline_position, target,
                                             origin);
  auto* result = state.get();
  sources_.push_back(std::move(state));
  ++metrics_.source_opens;
  return result;
}

void AudioMixSession::prune_sources(const std::vector<AudioContributor>& contributors) {
  std::erase_if(sources_, [&contributors](const auto& source) {
    return std::none_of(contributors.begin(), contributors.end(), [&](const auto& contributor) {
      return same_identity(source->contributor, contributor);
    });
  });
}

void AudioMixSession::select_range(domain::MediaTime start, domain::MediaTime end,
                                   bool discontinuity) {
  bool contributor_set_changed = discontinuity;
  if (discontinuity) {
    active_contributors_.clear();
    next_contributor_ = 0;
    while (next_contributor_ < contributors_.size() &&
           contributors_[next_contributor_].timeline_range.start() < end) {
      const auto& contributor = contributors_[next_contributor_++];
      if (contributor.timeline_range.end() > start) {
        active_contributors_.push_back(contributor);
      }
    }
    active_unavailable_.clear();
    next_unavailable_ = 0;
    while (next_unavailable_ < unavailable_.size() &&
           unavailable_[next_unavailable_].timeline_range.start() < end) {
      const auto& unavailable = unavailable_[next_unavailable_++];
      if (unavailable.timeline_range.end() > start) {
        active_unavailable_.push_back(unavailable);
      }
    }
  } else {
    const auto removed = std::erase_if(active_contributors_, [start](const auto& contributor) {
      return contributor.timeline_range.end() <= start;
    });
    contributor_set_changed = removed != 0;
    while (next_contributor_ < contributors_.size() &&
           contributors_[next_contributor_].timeline_range.start() < end) {
      active_contributors_.push_back(contributors_[next_contributor_++]);
      contributor_set_changed = true;
    }
    std::erase_if(active_unavailable_, [start](const auto& unavailable) {
      return unavailable.timeline_range.end() <= start;
    });
    while (next_unavailable_ < unavailable_.size() &&
           unavailable_[next_unavailable_].timeline_range.start() < end) {
      active_unavailable_.push_back(unavailable_[next_unavailable_++]);
    }
  }
  if (contributor_set_changed) {
    std::sort(active_contributors_.begin(), active_contributors_.end(), mix_order);
  }
}

void AudioMixSession::update_metrics() noexcept {
  metrics_.live_sources = sources_.size();
  metrics_.peak_live_sources = std::max(metrics_.peak_live_sources, metrics_.live_sources);
  metrics_.queued_frames = 0;
  for (const auto& source : sources_) {
    metrics_.queued_frames += source->queued_frames(format_.channel_count);
  }
  metrics_.peak_queued_frames = std::max(metrics_.peak_queued_frames, metrics_.queued_frames);
}

AudioMixRenderResult AudioMixSession::render(domain::MediaTime timeline_position,
                                             std::size_t frame_count) {
  if (!snapshot_.has_value()) {
    throw std::logic_error{"audio mix session has no project snapshot"};
  }
  if (timeline_position < domain::MediaTime::zero()) {
    throw std::invalid_argument{"audio mix position cannot be negative"};
  }
  if (frame_count == 0 || frame_count > maximum_render_frames) {
    throw std::invalid_argument{"audio mix render size is outside bounded range"};
  }
  const bool discontinuity = !next_timeline_position_.has_value() ||
                             *next_timeline_position_ != timeline_position;
  if (next_timeline_position_.has_value() && discontinuity) {
    sources_.clear();
  }

  const auto render_end = timeline_position + domain::MediaTime{
                                                  static_cast<std::int64_t>(frame_count),
                                                  domain::Rational{1, format_.sample_rate}};
  select_range(timeline_position, render_end, discontinuity);
  AudioMixRenderResult result{
      AudioBlock{format_.sample_rate, format_.channel_count, frame_count,
                 std::vector<float>(checked_sample_count(frame_count, format_.channel_count), 0.0F),
                 domain::Rational{1, format_.sample_rate}, std::nullopt},
      timeline_position, {}, 0, 0, 0, AudioMixClockRecommendation::Software};
  result.resolved_contributors = active_contributors_.size();
  for (const auto& missing : active_unavailable_) {
    result.issues.push_back({missing.track_id, missing.clip_id, missing.media_id,
                             AudioContributorFailure::Unavailable,
                             missing.reason.value_or("audio contributor is unavailable")});
  }

  for (const auto& contributor : active_contributors_) {
    const auto overlap_start = std::max(timeline_position, contributor.timeline_range.start());
    const auto overlap_end = std::min(render_end, contributor.timeline_range.end());
    const auto output_begin = std::min(frame_count,
                                       frames_ceil(overlap_start - timeline_position,
                                                   format_.sample_rate));
    const auto output_end = std::min(frame_count,
                                     frames_ceil(overlap_end - timeline_position,
                                                 format_.sample_rate));
    if (output_begin >= output_end) {
      continue;
    }
    const auto source_timeline = timeline_position + domain::MediaTime{
                                                         static_cast<std::int64_t>(output_begin),
                                                         domain::Rational{1, format_.sample_rate}};
    auto* state = find_source(contributor);
    if (state == nullptr) {
      // A split successor keeps the decoder/converter/queued PCM. clip_id is
      // editorial identity, not decoder continuity identity.
      state = transfer_contiguous_source(contributor);
    }
    if (state != nullptr && state->next_timeline != source_timeline) {
      std::erase_if(sources_, [state](const auto& candidate) { return candidate.get() == state; });
      state = nullptr;
    }
    if (state == nullptr) {
      state = open_source(contributor, source_timeline, result.issues);
    }
    if (state == nullptr) {
      continue;
    }

    const auto required = output_end - output_begin;
    while (state->queued_frames(format_.channel_count) < required && !state->failed) {
      if (state->eof) {
        if (!state->tail_flushed) {
          try {
            state->append(state->converter->flush_tail_pcm(), format_.channel_count);
          } catch (const AudioConversionError& error) {
            result.issues.push_back(issue_for(contributor, AudioContributorFailure::Decode,
                                              error.what()));
            state->failed = true;
          }
          state->tail_flushed = true;
        }
        break;
      }
      AudioPlaybackSource::ReadResult read{AudioPlaybackSource::ReadState::Ended, std::nullopt};
      try {
        read = state->source->read_next();
      } catch (const std::exception& error) {
        result.issues.push_back(issue_for(contributor, AudioContributorFailure::Decode,
                                          error.what()));
        state->failed = true;
        break;
      } catch (...) {
        result.issues.push_back(issue_for(contributor, AudioContributorFailure::Decode,
                                          "audio decoder threw an unknown exception"));
        state->failed = true;
        break;
      }
      if (read.state == AudioPlaybackSource::ReadState::Starved) {
        result.issues.push_back(issue_for(contributor, AudioContributorFailure::Starved,
                                          "audio contributor is temporarily starved"));
        break;
      }
      if (read.state == AudioPlaybackSource::ReadState::Ended || !read.block.has_value()) {
        state->eof = true;
        continue;
      }
      auto block = std::move(*read.block);
      ++metrics_.decoded_blocks;
      if (block.frame_count() > maximum_source_block_frames) {
        result.issues.push_back(issue_for(contributor, AudioContributorFailure::Decode,
                                          "decoded audio block exceeds bounded mixer input"));
        state->failed = true;
        break;
      }
      if (block.sample_rate() != state->source->sample_rate() ||
          block.channel_count() != state->source->channel_count()) {
        result.issues.push_back(issue_for(contributor, AudioContributorFailure::Decode,
                                          "decoded audio format changed inside contributor"));
        state->failed = true;
        break;
      }

      if (block.presentation_timestamp().has_value() &&
          state->expected_source_timestamp.has_value()) {
        const auto block_start = *block.presentation_timestamp();
        const auto block_end = block_start + domain::MediaTime{
                                                 static_cast<std::int64_t>(block.frame_count()),
                                                 domain::Rational{1, block.sample_rate()}};
        const auto target = *state->expected_source_timestamp;
        ++metrics_.timestamp_observations;
        const auto delta = block_start - target;
        if (delta > domain::MediaTime::zero()) {
          ++metrics_.timestamp_positive_deltas;
        } else if (delta < domain::MediaTime::zero()) {
          ++metrics_.timestamp_negative_deltas;
        }
        if (block_end <= target) {
          trace_timestamp(contributor, block_start, target, block.frame_count(),
                          block.sample_rate(), 0, block.frame_count(), state->stream_origin,
                          state->initial_seek_target, timestamp_trace_count_);
          continue;
        }
        std::size_t inserted_silence = 0;
        std::size_t trimmed_frames = 0;
        if (block_start > target) {
          const auto silence_frames = frames_floor(block_start - target, block.sample_rate());
          inserted_silence = silence_frames;
          metrics_.timestamp_inserted_silence_frames += silence_frames;
          if (silence_frames > 0) {
            AudioBlock silence{block.sample_rate(), block.channel_count(), silence_frames,
                               std::vector<float>(checked_sample_count(
                                                      silence_frames, block.channel_count()),
                                                  0.0F),
                               block.time_base(), std::nullopt};
            try {
              state->append(state->converter->convert_pcm(silence), format_.channel_count);
            } catch (const AudioConversionError& error) {
              result.issues.push_back(issue_for(contributor, AudioContributorFailure::Decode,
                                                error.what()));
              state->failed = true;
              break;
            }
          }
        } else if (block_start < target) {
          const auto trim = std::min(block.frame_count(),
                                     frames_floor(target - block_start, block.sample_rate()));
          trimmed_frames = trim;
          metrics_.timestamp_trimmed_frames += trim;
          const auto first = trim * static_cast<std::size_t>(block.channel_count());
          block = AudioBlock{block.sample_rate(), block.channel_count(),
                             block.frame_count() - trim,
                             std::vector<float>(block.samples().begin() +
                                                    static_cast<std::ptrdiff_t>(first),
                                                block.samples().end()),
                                                  block.time_base(), std::nullopt};
        }
        trace_timestamp(contributor, block_start, target, block.frame_count(),
                        block.sample_rate(), inserted_silence, trimmed_frames,
                        state->stream_origin, state->initial_seek_target, timestamp_trace_count_);
        state->expected_source_timestamp = block_end;
      }
      if (block.frame_count() != 0) {
        try {
          state->append(state->converter->convert_pcm(block), format_.channel_count);
        } catch (const AudioConversionError& error) {
          result.issues.push_back(issue_for(contributor, AudioContributorFailure::Decode,
                                            error.what()));
          state->failed = true;
          break;
        }
      }
      if (state->queued_frames(format_.channel_count) > maximum_queued_frames_per_source) {
        result.issues.push_back(issue_for(contributor, AudioContributorFailure::Decode,
                                          "audio contributor exceeded bounded PCM queue"));
        state->failed = true;
      }
    }

    const auto available = std::min(required, state->queued_frames(format_.channel_count));
    if (available < required && state->eof) {
      result.issues.push_back(issue_for(contributor, AudioContributorFailure::EndedEarly,
                                        "audio contributor ended before its clip boundary"));
    }
    if (available == 0) {
      continue;
    }
    ++result.usable_contributors;
    const auto gain = ClipGain::from_decibels(contributor.gain_db, contributor.muted);
    auto& mixed = result.block.samples_mutable();
    const auto channels = static_cast<std::size_t>(format_.channel_count);
    if (!gain.muted) {
      for (std::size_t frame = 0; frame < available; ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
          const auto destination = (output_begin + frame) * channels + channel;
          const auto source_index = state->pcm_offset + frame * channels + channel;
          mixed[destination] += state->pcm[source_index] * gain.linear;
        }
      }
      ++result.mixed_contributors;
    }
    state->pcm_offset += available * channels;
    state->next_timeline = source_timeline + domain::MediaTime{
                                                static_cast<std::int64_t>(available),
                                                domain::Rational{1, format_.sample_rate}};
  }

  // Preview policy: deterministic hard clipping only after summation. There
  // is no hidden per-track normalization, AGC, limiter, or contributor-count
  // attenuation.
  for (float& sample : result.block.samples_mutable()) {
    sample = std::clamp(sample, -1.0F, 1.0F);
  }
  result.clock = result.usable_contributors == 0 ? AudioMixClockRecommendation::Software
                                                  : AudioMixClockRecommendation::Device;
  prune_sources(active_contributors_);
  next_timeline_position_ = render_end;
  update_metrics();
  return result;
}

} // namespace pulsar::application
