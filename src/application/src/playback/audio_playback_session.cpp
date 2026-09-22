#include <pulsar/application/playback/audio_playback_session.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pulsar::application {
namespace {

constexpr std::uint64_t kStartBufferMilliseconds = 100;
constexpr std::uint64_t kRefillBufferMilliseconds = 40;
constexpr std::uint64_t kMaximumBufferMilliseconds = 250;

[[nodiscard]] std::size_t checked_samples(std::size_t frames, int channels) {
  if (channels <= 0 || frames > std::numeric_limits<std::size_t>::max() /
                            static_cast<std::size_t>(channels)) {
    throw std::overflow_error{"audio silence size overflow"};
  }
  return frames * static_cast<std::size_t>(channels);
}

} // namespace

AudioPlaybackSession::AudioPlaybackSession(AudioOutput* output,
                                           AudioPlaybackSourceFactory source_factory)
    : output_{output}, source_factory_{std::move(source_factory)} {}

AudioPlaybackSession::~AudioPlaybackSession() { reset(); }

void AudioPlaybackSession::configure(const ResolvedClip& clip,
                                     domain::MediaTime timeline_position) {
  ++generation_;
  clip_ = clip;
  configured_timeline_ = timeline_position;
  source_eof_ = false;
  error_message_.reset();
  condition_ = AudioPlaybackCondition::Inactive;
  stream_origin_.reset();
  drain_requested_ = false;
  pending_block_.reset();
  pending_silence_frames_ = 0;
  expected_source_timestamp_.reset();
  observed_underruns_ = 0;
  processed_anchor_ = 0;
  audio_clock_anchor_ = timeline_position;
  gain_ = ClipGain::from_decibels(clip.audio_gain_db, clip.muted);

  if (!clip.audio.has_value() || output_ == nullptr || !source_factory_) {
    close_output();
    if (clip.audio.has_value() && output_ == nullptr) {
      state_ = AudioPlaybackState::DeviceUnavailable;
      condition_ = AudioPlaybackCondition::DeviceUnavailable;
    } else {
      state_ = AudioPlaybackState::IntentionalSilence;
      condition_ = AudioPlaybackCondition::IntentionalSilence;
    }
    return;
  }

  state_ = AudioPlaybackState::Preparing;
  condition_ = AudioPlaybackCondition::Recovering;
  if (!prepare_audio_at(timeline_position)) {
    if (state_ == AudioPlaybackState::Preparing) {
      state_ = AudioPlaybackState::Error;
      condition_ = AudioPlaybackCondition::Error;
    }
  }
}

void AudioPlaybackSession::seek(domain::MediaTime timeline_position) {
  if (!clip_.has_value()) {
    return;
  }
  ++generation_;
  configured_timeline_ = timeline_position;
  source_eof_ = false;
  error_message_.reset();
  condition_ = AudioPlaybackCondition::Inactive;
  stream_origin_.reset();
  drain_requested_ = false;
  pending_block_.reset();
  pending_silence_frames_ = 0;
  expected_source_timestamp_.reset();
  observed_underruns_ = 0;
  processed_anchor_ = 0;
  audio_clock_anchor_ = timeline_position;
  gain_ = ClipGain::from_decibels(clip_->audio_gain_db, clip_->muted);
  if (!clip_->audio.has_value() || output_ == nullptr || !source_factory_) {
    close_output();
    if (clip_->audio.has_value() && output_ == nullptr) {
      state_ = AudioPlaybackState::DeviceUnavailable;
      condition_ = AudioPlaybackCondition::DeviceUnavailable;
    } else {
      state_ = AudioPlaybackState::IntentionalSilence;
      condition_ = AudioPlaybackCondition::IntentionalSilence;
    }
    return;
  }
  state_ = AudioPlaybackState::Preparing;
  condition_ = AudioPlaybackCondition::Recovering;
  if (!prepare_audio_at(timeline_position) && state_ == AudioPlaybackState::Preparing) {
    state_ = AudioPlaybackState::Error;
    condition_ = AudioPlaybackCondition::Error;
  }
}

void AudioPlaybackSession::play() {
  if (state_ == AudioPlaybackState::IntentionalSilence ||
      state_ == AudioPlaybackState::DeviceUnavailable || state_ == AudioPlaybackState::EndedEarly) {
    return;
  }
  if (state_ == AudioPlaybackState::Paused) {
    seek(configured_timeline_);
  }
  if (state_ != AudioPlaybackState::Ready || output_ == nullptr) {
    return;
  }
  if (!output_->start()) {
    set_device_unavailable();
    return;
  }
  const auto metrics = output_->metrics();
  processed_anchor_ = metrics.device_frames_processed;
  audio_clock_anchor_ = configured_timeline_;
  observed_underruns_ = metrics.underrun_count;
  state_ = AudioPlaybackState::Running;
  condition_ = AudioPlaybackCondition::Present;
  if (source_eof_) {
    if (!output_->request_drain()) {
      set_device_unavailable();
      return;
    }
    drain_requested_ = true;
  }
}

void AudioPlaybackSession::pause() {
  if (state_ == AudioPlaybackState::Running && output_ != nullptr) {
    const auto position = clock_position();
    if (position.has_value()) {
      configured_timeline_ = *position;
    }
    if (!output_->stop()) {
      set_device_unavailable();
      return;
    }
    state_ = AudioPlaybackState::Paused;
  }
}

void AudioPlaybackSession::update(domain::MediaTime timeline_position) {
  configured_timeline_ = timeline_position;
  if (output_ == nullptr || state_ == AudioPlaybackState::IntentionalSilence ||
      state_ == AudioPlaybackState::DeviceUnavailable || state_ == AudioPlaybackState::Error ||
      state_ == AudioPlaybackState::EndedEarly || state_ == AudioPlaybackState::Paused) {
    return;
  }
  const auto output_state = output_->state();
  if (output_state == AudioOutputState::Error) {
    set_device_unavailable();
    return;
  }
  if (source_eof_ && !drain_requested_ && output_state == AudioOutputState::Running) {
    if (!output_->request_drain()) {
      set_device_unavailable();
      return;
    }
    drain_requested_ = true;
  }
  const auto metrics = output_->metrics();
  if (metrics.underrun_count > observed_underruns_) {
    observed_underruns_ = metrics.underrun_count;
    state_ = AudioPlaybackState::Starved;
    condition_ = AudioPlaybackCondition::Starved;
    (void)output_->stop();
    return;
  }
  if (state_ == AudioPlaybackState::Running) {
    const auto target = output_->device_format().has_value()
                            ? static_cast<std::uint64_t>(output_->device_format()->sample_rate) *
                                  kRefillBufferMilliseconds / 1000
                            : 0;
    if (!fill_until(target)) {
      return;
    }
  }
  if (state_ == AudioPlaybackState::Preparing || state_ == AudioPlaybackState::Starved) {
    state_ = AudioPlaybackState::Recovering;
    condition_ = AudioPlaybackCondition::Recovering;
    if (fill_until(static_cast<std::uint64_t>(output_->device_format()->sample_rate) *
                   kStartBufferMilliseconds / 1000)) {
      state_ = AudioPlaybackState::Ready;
      condition_ = AudioPlaybackCondition::Present;
    }
  }
  if (source_eof_ && !drain_requested_ && state_ == AudioPlaybackState::Running) {
    if (!output_->request_drain()) {
      set_device_unavailable();
      return;
    }
    drain_requested_ = true;
  }
  if (drain_requested_ && output_->state() == AudioOutputState::Completed) {
    state_ = AudioPlaybackState::EndedEarly;
    condition_ = AudioPlaybackCondition::SourceEndedEarly;
  }
}

void AudioPlaybackSession::reset() {
  ++generation_;
  close_output();
  source_.reset();
  pending_block_.reset();
  pending_silence_frames_ = 0;
  expected_source_timestamp_.reset();
  clip_.reset();
  stream_origin_.reset();
  state_ = AudioPlaybackState::Inactive;
  condition_ = AudioPlaybackCondition::Inactive;
  error_message_.reset();
}

bool AudioPlaybackSession::has_device_clock() const noexcept {
  return state_ == AudioPlaybackState::Running;
}

std::optional<domain::MediaTime> AudioPlaybackSession::clock_position() const {
  if (output_ == nullptr || !has_device_clock() || !output_->device_format().has_value()) {
    return std::nullopt;
  }
  const auto metrics = output_->metrics();
  const auto elapsed = metrics.device_frames_processed >= processed_anchor_
                           ? metrics.device_frames_processed - processed_anchor_
                           : 0;
  return audio_clock_anchor_ + domain::MediaTime{
                                  static_cast<std::int64_t>(elapsed),
                                  domain::Rational{1, output_->device_format()->sample_rate}};
}

bool AudioPlaybackSession::open_source_at(domain::MediaTime timeline_position) {
  if (!clip_.has_value() || !clip_->audio.has_value() || output_ == nullptr) {
    return false;
  }
  try {
    source_ = source_factory_(clip_->audio->source_locator, clip_->audio->metadata.stream_index);
  } catch (...) {
    set_decode_error("audio source creation failed");
    return false;
  }
  if (!source_) {
    set_decode_error("audio source factory returned no source");
    return false;
  }
  stream_origin_ = source_->stream_origin();
  expected_source_timestamp_.reset();
  if (!source_->seek_to(audio_target_for(timeline_position))) {
    set_decode_error("audio source seek failed");
    return false;
  }
  if (!output_->open({source_->sample_rate(), source_->channel_count()})) {
    state_ = AudioPlaybackState::DeviceUnavailable;
    condition_ = AudioPlaybackCondition::DeviceUnavailable;
    if (output_->error().has_value()) {
      error_message_ = output_->error()->message;
    }
    return false;
  }
  configured_timeline_ = timeline_position;
  return true;
}

bool AudioPlaybackSession::prepare_audio_at(domain::MediaTime timeline_position) {
  close_output();
  source_.reset();
  if (!open_source_at(timeline_position)) {
    return false;
  }
  while (!pending_block_.has_value() && !source_eof_) {
    AudioPlaybackSource::ReadResult result{AudioPlaybackSource::ReadState::Ended, std::nullopt};
    try {
      result = source_->read_next();
    } catch (...) {
      set_decode_error("audio decoder failed while preparing audio");
      return false;
    }
    if (result.state == AudioPlaybackSource::ReadState::Starved) {
      state_ = AudioPlaybackState::Starved;
      condition_ = AudioPlaybackCondition::Starved;
      return false;
    }
    if (result.state == AudioPlaybackSource::ReadState::Ended || !result.block.has_value()) {
      source_eof_ = true;
      break;
    }
      pending_block_ = align_block(std::move(*result.block));
  }
  if (!fill_until(static_cast<std::uint64_t>(output_->device_format()->sample_rate) *
                  kStartBufferMilliseconds / 1000)) {
    return false;
  }
  if (source_eof_ && buffered_device_frames() == 0) {
    state_ = AudioPlaybackState::EndedEarly;
    condition_ = AudioPlaybackCondition::SourceEndedEarly;
    return true;
  }
  state_ = AudioPlaybackState::Ready;
  condition_ = AudioPlaybackCondition::Present;
  return true;
}

std::optional<AudioBlock> AudioPlaybackSession::align_block(AudioBlock block) {
  if (!clip_.has_value()) {
    return std::nullopt;
  }
  const domain::MediaTime target = audio_target_for(configured_timeline_);
  if (!block.presentation_timestamp().has_value()) {
    return block;
  }
  const auto block_start = *block.presentation_timestamp();
  const auto block_duration = domain::MediaTime{
      static_cast<std::int64_t>(block.frame_count()), domain::Rational{1, block.sample_rate()}};
  const auto block_end = block_start + block_duration;
  const auto alignment_target = expected_source_timestamp_.value_or(target);
  if (expected_source_timestamp_.has_value()) {
    if (block_end <= alignment_target) {
      return std::nullopt;
    }
    if (block_start < alignment_target) {
      const auto offset = std::min(block.frame_count(), source_frames_for(alignment_target - block_start));
      const auto begin = offset * static_cast<std::size_t>(block.channel_count());
      std::vector<float> samples(block.samples().begin() + static_cast<std::ptrdiff_t>(begin),
                                 block.samples().end());
      expected_source_timestamp_ = block_end;
      return AudioBlock{block.sample_rate(), block.channel_count(),
                        block.frame_count() - offset, std::move(samples), block.time_base(),
                        std::nullopt};
    }
    if (block_start > alignment_target) {
      pending_silence_frames_ = source_frames_for(block_start - alignment_target);
    }
    expected_source_timestamp_ = block_end;
    return block;
  }
  if (block_end <= target) {
    return std::nullopt;
  }
  if (block_start > target) {
    pending_silence_frames_ = source_frames_for(block_start - target);
    expected_source_timestamp_ = block_end;
    return block;
  }
  const auto offset = std::min(block.frame_count(), source_frames_for(target - block_start));
  const auto begin = offset * static_cast<std::size_t>(block.channel_count());
  std::vector<float> samples(block.samples().begin() + static_cast<std::ptrdiff_t>(begin),
                             block.samples().end());
  expected_source_timestamp_ = block_end;
  return AudioBlock{block.sample_rate(), block.channel_count(),
                    block.frame_count() - offset, std::move(samples), block.time_base(),
                    std::nullopt};
}

bool AudioPlaybackSession::enqueue_block(const AudioBlock& block) {
  if (output_ == nullptr) {
    return false;
  }
  AudioBlock processed = block;
  apply_clip_gain(processed, gain_);
  const auto result = output_->enqueue(processed);
  if (result == AudioEnqueueResult::Accepted) {
    return true;
  }
  if (result == AudioEnqueueResult::Backpressure) {
    return false;
  }
  set_device_unavailable();
  return false;
}

bool AudioPlaybackSession::enqueue_silence(std::size_t frames) {
  if (source_ == nullptr || frames == 0) {
    return true;
  }
  const auto channels = source_->channel_count();
  const std::size_t chunk = std::min<std::size_t>(frames, 4096);
  AudioBlock silence{source_->sample_rate(), channels, chunk,
                     std::vector<float>(checked_samples(chunk, channels), 0.0F),
                     domain::Rational{1, source_->sample_rate()}, std::nullopt};
  return enqueue_block(silence);
}

bool AudioPlaybackSession::fill_until(std::uint64_t target_device_frames) {
  if (output_ != nullptr && output_->device_format().has_value()) {
    const auto max_buffer = static_cast<std::uint64_t>(output_->device_format()->sample_rate) *
                            kMaximumBufferMilliseconds / 1000;
    target_device_frames = std::min(target_device_frames, max_buffer);
  }
  // Refill against the adapter queue, not processedUSecs().  A QAudioSink
  // may still be consuming backend-buffered frames while its processed
  // counter is unchanged; waiting on that counter lets the sink drain before
  // the next PCM block is queued.
  while (output_->metrics().buffered_device_frames < target_device_frames &&
         !drain_requested_) {
    if (pending_silence_frames_ > 0) {
      const std::size_t chunk = std::min<std::size_t>(pending_silence_frames_, 4096);
      if (!enqueue_silence(chunk)) {
        return true;
      }
      pending_silence_frames_ -= chunk;
      continue;
    }
    if (!pending_block_.has_value()) {
      if (source_eof_) {
        break;
      }
      AudioPlaybackSource::ReadResult result{AudioPlaybackSource::ReadState::Ended, std::nullopt};
      try {
        result = source_->read_next();
      } catch (...) {
        set_decode_error("audio decoder failed while refilling audio");
        return false;
      }
      if (result.state == AudioPlaybackSource::ReadState::Starved) {
        state_ = AudioPlaybackState::Starved;
        condition_ = AudioPlaybackCondition::Starved;
        return false;
      }
      if (result.state == AudioPlaybackSource::ReadState::Ended || !result.block.has_value()) {
        source_eof_ = true;
        break;
      }
    pending_block_ = align_block(std::move(*result.block));
      if (!pending_block_.has_value()) {
        continue;
      }
    }
    if (!enqueue_block(*pending_block_)) {
      return true;
    }
    pending_block_.reset();
  }
  return true;
}

std::size_t AudioPlaybackSession::source_frames_for(domain::MediaTime duration) const {
  if (source_ == nullptr || duration <= domain::MediaTime::zero()) {
    return 0;
  }
  const auto seconds = duration.seconds();
  const auto numerator = static_cast<__int128_t>(seconds.numerator()) * source_->sample_rate();
  const auto denominator = static_cast<__int128_t>(seconds.denominator());
  return static_cast<std::size_t>(numerator / denominator);
}

domain::MediaTime AudioPlaybackSession::audio_target_for(
    domain::MediaTime timeline_position) const {
  return audio_source_position(*clip_, timeline_position, stream_origin_);
}

std::uint64_t AudioPlaybackSession::buffered_device_frames() const {
  if (output_ == nullptr) {
    return 0;
  }
  const auto metrics = output_->metrics();
  const auto in_backend = metrics.device_frames_submitted >= metrics.device_frames_processed
                              ? metrics.device_frames_submitted - metrics.device_frames_processed
                              : 0;
  return in_backend + metrics.buffered_device_frames;
}

void AudioPlaybackSession::close_output() {
  if (output_ == nullptr) {
    return;
  }
  const auto state = output_->state();
  if (state != AudioOutputState::Closed) {
    if (state == AudioOutputState::Running || state == AudioOutputState::Draining) {
      (void)output_->stop();
    }
    (void)output_->close();
  }
}

void AudioPlaybackSession::set_device_unavailable() {
  state_ = AudioPlaybackState::DeviceUnavailable;
  condition_ = AudioPlaybackCondition::DeviceUnavailable;
  if (output_ != nullptr && output_->error().has_value()) {
    error_message_ = output_->error()->message;
  }
}

void AudioPlaybackSession::set_decode_error(const char* message) {
  state_ = AudioPlaybackState::Error;
  condition_ = AudioPlaybackCondition::Error;
  error_message_ = message;
}

} // namespace pulsar::application
