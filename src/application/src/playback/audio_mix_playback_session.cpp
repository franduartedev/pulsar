#include <pulsar/application/playback/audio_mix_playback_session.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <limits>
#include <stdexcept>

namespace pulsar::application {
namespace {

[[nodiscard]] std::uint64_t frame_delta(const AudioOutputMetrics& metrics) noexcept {
  const auto submitted = metrics.device_frames_submitted;
  const auto processed = metrics.device_frames_processed;
  return submitted >= processed ? submitted - processed : 0;
}

} // namespace

AudioMixPlaybackSession::AudioMixPlaybackSession(AudioOutput* output,
                                                 AudioPlaybackSourceFactory source_factory)
    : output_{output}, mixer_{std::move(source_factory)} {}

AudioMixPlaybackSession::~AudioMixPlaybackSession() { close_output(); }

void AudioMixPlaybackSession::set_condition(AudioPlaybackCondition condition) noexcept {
  condition_ = condition;
  switch (condition) {
  case AudioPlaybackCondition::Present:
    state_ = output_ != nullptr && output_->state() == AudioOutputState::Running
                 ? AudioPlaybackState::Running
                 : AudioPlaybackState::Ready;
    break;
  case AudioPlaybackCondition::IntentionalSilence:
    state_ = AudioPlaybackState::IntentionalSilence;
    break;
  case AudioPlaybackCondition::Starved:
    state_ = AudioPlaybackState::Starved;
    break;
  case AudioPlaybackCondition::Recovering:
    state_ = AudioPlaybackState::Recovering;
    break;
  case AudioPlaybackCondition::DeviceUnavailable:
    state_ = AudioPlaybackState::DeviceUnavailable;
    break;
  case AudioPlaybackCondition::Error:
    state_ = AudioPlaybackState::Error;
    break;
  case AudioPlaybackCondition::SourceEndedEarly:
    state_ = AudioPlaybackState::EndedEarly;
    break;
  case AudioPlaybackCondition::Inactive:
    state_ = AudioPlaybackState::Inactive;
    break;
  }
}

void AudioMixPlaybackSession::configure(ProjectSnapshot snapshot) {
  const bool was_playing = requested_playing_;
  close_output();
  mixer_.configure(snapshot);
  snapshot_ = std::move(snapshot);
  render_position_.reset();
  output_buffer_start_.reset();
  audio_ahead_ = false;
  last_usable_contributors_ = 0;
  observed_underruns_ = 0;
  error_message_.reset();
  requested_playing_ = was_playing;
  ++generation_;
  set_condition(was_playing ? AudioPlaybackCondition::Recovering
                            : AudioPlaybackCondition::Inactive);
  refresh_metrics();
}

void AudioMixPlaybackSession::seek(domain::MediaTime position) {
  if (position < domain::MediaTime::zero()) {
    throw std::invalid_argument{"audio mix seek position cannot be negative"};
  }
  const bool was_playing = requested_playing_;
  close_output();
  mixer_.reset();
  if (snapshot_.has_value()) mixer_.configure(*snapshot_);
  render_position_ = position;
  audio_ahead_ = false;
  last_usable_contributors_ = 0;
  observed_underruns_ = 0;
  ++generation_;
  requested_playing_ = was_playing;
  set_condition(AudioPlaybackCondition::Recovering);
  refresh_metrics();
}

void AudioMixPlaybackSession::play() {
  requested_playing_ = true;
  if (!snapshot_.has_value()) return;
  if (output_ != nullptr && output_->state() == AudioOutputState::Opened &&
      buffered_device_frames() != 0) {
    if (!output_->start()) {
      error_message_ = output_->error().has_value() ? output_->error()->message
                                                     : "audio output start failed";
      set_condition(AudioPlaybackCondition::DeviceUnavailable);
      return;
    }
    const auto before = output_->metrics();
    audio_clock_anchor_ = output_buffer_start_.value_or(render_position_.value_or(
        domain::MediaTime::zero()));
    processed_anchor_ = before.device_frames_processed;
    ++metrics_.clock_transitions;
    set_condition(AudioPlaybackCondition::Present);
  } else if (state_ == AudioPlaybackState::Paused || state_ == AudioPlaybackState::Inactive ||
             state_ == AudioPlaybackState::Recovering) {
    set_condition(AudioPlaybackCondition::Recovering);
    update(render_position_.value_or(domain::MediaTime::zero()));
  }
}

void AudioMixPlaybackSession::pause() {
  if (output_ != nullptr && output_->state() == AudioOutputState::Running) {
    if (const auto position = clock_position(); position.has_value()) render_position_ = *position;
    output_->stop();
  }
  requested_playing_ = false;
  audio_ahead_ = false;
  // The mixer may have prefetched beyond the device cursor. Rebuild its
  // source cursor at the exact paused editorial position so resume cannot
  // replay or skip queued PCM that was discarded with the output.
  mixer_.reset();
  if (snapshot_.has_value()) mixer_.configure(*snapshot_);
  state_ = AudioPlaybackState::Paused;
  set_condition(AudioPlaybackCondition::Inactive);
  state_ = AudioPlaybackState::Paused;
  refresh_metrics();
}

void AudioMixPlaybackSession::update(domain::MediaTime timeline_position) {
  if (!snapshot_.has_value() || !requested_playing_) return;
  bool recovered_from_underrun = false;
  if (output_ != nullptr) {
    const auto output_metrics = output_->metrics();
    if (output_metrics.underrun_count > observed_underruns_) {
      observed_underruns_ = output_metrics.underrun_count;
      output_->stop();
      // stop() owns the output queue flush. Keep the mixer cursor: its source
      // PCM is already sequentially aligned with render_position_, whereas
      // reopening a compressed source here would force an expensive codec
      // seek/preroll on every underrun. A global seek/edit still resets the
      // mixer through seek()/configure() and remains the discontinuity boundary.
      output_buffer_start_ = render_position_.value_or(timeline_position);
      audio_ahead_ = false;
      last_usable_contributors_ = 0;
      // AudioOutputEngine resets session counters as part of stop().
      observed_underruns_ = 0;
      ++metrics_.clock_transitions;
      set_condition(AudioPlaybackCondition::Recovering);
      recovered_from_underrun = true;
    }
  }
  if (!recovered_from_underrun && !audio_ahead_ && output_ != nullptr &&
      output_->state() != AudioOutputState::Running) {
    render_position_ = timeline_position;
  }
  if (!render_position_.has_value()) render_position_ = timeline_position;
  if (!audio_ahead_ && output_ != nullptr && output_->state() == AudioOutputState::Running) {
    if (buffered_device_frames() == 0) {
      output_->stop();
      ++metrics_.clock_transitions;
    }
  }
  const auto target = output_ != nullptr && output_->state() == AudioOutputState::Running
                          ? refill_buffer_frames
                          : start_buffer_frames;
  const auto buffer_start = render_position_.value_or(timeline_position);
  if (output_ == nullptr || output_->state() == AudioOutputState::Closed ||
      (output_->state() == AudioOutputState::Opened && buffered_device_frames() == 0)) {
    output_buffer_start_ = buffer_start;
  }
  if (!fill_until(target)) {
    refresh_metrics();
    return;
  }
  if (output_ != nullptr && output_->state() == AudioOutputState::Opened &&
      buffered_device_frames() != 0) {
    if (!output_->start()) {
      error_message_ = output_->error().has_value() ? output_->error()->message
                                                     : "audio output start failed";
      set_condition(AudioPlaybackCondition::DeviceUnavailable);
      refresh_metrics();
      return;
    }
    audio_clock_anchor_ = output_buffer_start_.value_or(buffer_start);
    processed_anchor_ = output_->metrics().device_frames_processed;
    ++metrics_.clock_transitions;
  }
  if (output_ != nullptr) {
    const auto underruns = output_->metrics().underrun_count;
    if (underruns > observed_underruns_) set_condition(AudioPlaybackCondition::Starved);
    observed_underruns_ = underruns;
  }
  refresh_metrics();
}

bool AudioMixPlaybackSession::fill_until(std::uint64_t target_frames) {
  if (output_ == nullptr) {
    set_condition(AudioPlaybackCondition::DeviceUnavailable);
    return false;
  }
  if (output_->state() == AudioOutputState::Closed) {
    if (!output_->open(AudioSourceFormat{mixer_.format().sample_rate,
                                         mixer_.format().channel_count})) {
      error_message_ = output_->error().has_value() ? output_->error()->message
                                                     : "audio output open failed";
      set_condition(AudioPlaybackCondition::DeviceUnavailable);
      return false;
    }
    state_ = AudioPlaybackState::Preparing;
  }
  while (buffered_device_frames() < target_frames &&
         buffered_device_frames() < maximum_buffer_frames) {
    const auto frames = static_cast<std::size_t>(std::min<std::uint64_t>(1024,
        maximum_buffer_frames - buffered_device_frames()));
    const auto start = std::chrono::steady_clock::now();
    const auto render_position = *render_position_;
    std::optional<AudioMixRenderResult> rendered;
    try {
      rendered.emplace(mixer_.render(render_position, frames));
    } catch (const std::exception& error) {
      error_message_ = error.what();
      set_condition(AudioPlaybackCondition::Error);
      return false;
    } catch (...) {
      error_message_ = "audio mix render failed";
      set_condition(AudioPlaybackCondition::Error);
      return false;
    }
    auto result = std::move(*rendered);
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count();
    record_render(static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsed)));
    render_position_ = *render_position_ + domain::MediaTime{
        static_cast<std::int64_t>(frames), domain::Rational{1, mixer_.format().sample_rate}};
    last_usable_contributors_ = result.usable_contributors;
    if (!result.issues.empty() && result.usable_contributors == 0) {
      set_condition(AudioPlaybackCondition::Starved);
    } else if (result.usable_contributors == 0) {
      set_condition(AudioPlaybackCondition::IntentionalSilence);
    } else {
      set_condition(AudioPlaybackCondition::Present);
    }
    if (result.usable_contributors == 0) {
      audio_ahead_ = false;
      if (output_->state() == AudioOutputState::Opened && buffered_device_frames() == 0) {
        output_->stop();
      }
      return true;
    }
    const auto enqueue = output_->enqueue(result.block);
    if (enqueue != AudioEnqueueResult::Accepted) {
      if (enqueue == AudioEnqueueResult::Backpressure) return true;
      error_message_ = "audio mix output enqueue failed";
      set_condition(AudioPlaybackCondition::Error);
      return false;
    }
    audio_ahead_ = true;
  }
  return true;
}

std::uint64_t AudioMixPlaybackSession::buffered_device_frames() const {
  if (output_ == nullptr) return 0;
  const auto metrics = output_->metrics();
  return frame_delta(metrics) + metrics.buffered_device_frames;
}

bool AudioMixPlaybackSession::has_device_clock() const noexcept {
  return output_ != nullptr && output_->state() == AudioOutputState::Running &&
         last_usable_contributors_ != 0;
}

std::optional<domain::MediaTime> AudioMixPlaybackSession::clock_position() const {
  if (!has_device_clock() || output_ == nullptr) return std::nullopt;
  const auto device = output_->device_format();
  if (!device.has_value() || device->sample_rate <= 0) return std::nullopt;
  const auto processed = output_->metrics().device_frames_processed;
  if (processed < processed_anchor_) return audio_clock_anchor_;
  return audio_clock_anchor_ + domain::MediaTime{
      static_cast<std::int64_t>(processed - processed_anchor_),
      domain::Rational{1, device->sample_rate}};
}

void AudioMixPlaybackSession::close_output() noexcept {
  if (output_ == nullptr) return;
  const auto state = output_->state();
  if (state == AudioOutputState::Running || state == AudioOutputState::Draining) {
    output_->stop();
  }
  if (output_->state() != AudioOutputState::Closed) output_->close();
  audio_ahead_ = false;
}

void AudioMixPlaybackSession::reset() noexcept {
  close_output();
  mixer_.reset();
  snapshot_.reset();
  render_position_.reset();
  output_buffer_start_.reset();
  requested_playing_ = false;
  last_usable_contributors_ = 0;
  error_message_.reset();
  ++generation_;
  set_condition(AudioPlaybackCondition::Inactive);
  refresh_metrics();
}

void AudioMixPlaybackSession::record_render(std::uint64_t micros) noexcept {
  render_samples_[render_sample_cursor_] = micros;
  render_sample_cursor_ = (render_sample_cursor_ + 1) % render_samples_.size();
  render_sample_count_ = std::min(render_sample_count_ + 1, render_samples_.size());
  ++metrics_.render_count;
  metrics_.render_max_us = std::max(metrics_.render_max_us, micros);
  std::array<std::uint64_t, 128> ordered{};
  std::copy_n(render_samples_.begin(), render_sample_count_, ordered.begin());
  std::sort(ordered.begin(), ordered.begin() + static_cast<std::ptrdiff_t>(render_sample_count_));
  metrics_.render_p50_us = ordered[(render_sample_count_ - 1) * 50 / 100];
  metrics_.render_p95_us = ordered[(render_sample_count_ - 1) * 95 / 100];
}

void AudioMixPlaybackSession::refresh_metrics() noexcept {
  metrics_.mixer = mixer_.metrics();
  if (output_ != nullptr) metrics_.output = output_->metrics();
}

} // namespace pulsar::application
