#include <pulsar/application/audio/audio_output_engine.hpp>

#include <pulsar/application/audio/audio_converter.hpp>

#include <utility>

namespace pulsar::application {
namespace {

[[nodiscard]] bool valid_source(const AudioSourceFormat& source) {
  return source.sample_rate > 0 && source.channel_count > 0;
}

[[nodiscard]] bool valid_device(const AudioDeviceFormat& device) {
  return device.sample_rate > 0 && device.channel_count > 0;
}

[[nodiscard]] bool channels_negotiable(int source_channels, int device_channels) {
  if (source_channels == device_channels) {
    return true;
  }
  return (source_channels == 1 || source_channels == 2) &&
         (device_channels == 1 || device_channels == 2);
}

} // namespace

AudioOutputEngine::AudioOutputEngine(AudioDeviceBackend& backend, AudioEngineLimits limits)
    : backend_{backend}, limits_{limits} {}

bool AudioOutputEngine::open(const AudioSourceFormat& source) {
  std::lock_guard lock{mutex_};
  if (state_ != AudioOutputState::Closed) {
    return false;
  }
  if (!valid_source(source) || limits_.queue_frame_capacity == 0) {
    enter_error(AudioOutputError::Code::UnsupportedConversion, "invalid audio output source");
    return false;
  }
  const auto offered = backend_.query_device_format(source);
  if (!offered.has_value() || !valid_device(*offered)) {
    enter_error(AudioOutputError::Code::NoDevice, "no usable audio output device");
    return false;
  }
  if (!channels_negotiable(source.channel_count, offered->channel_count)) {
    enter_error(AudioOutputError::Code::UnsupportedConversion,
                "audio channel mapping is outside A2 scope");
    return false;
  }
  if (!backend_.open_device(*offered)) {
    const auto backend_message = backend_.backend_error();
    enter_error(AudioOutputError::Code::BackendFailure,
                backend_message.value_or("audio backend refused to open"));
    return false;
  }
  source_format_ = source;
  device_format_ = offered;
  latched_error_.reset();
  queue_.clear();
  queue_head_ = 0;
  metrics_ = AudioOutputMetrics{};
  drain_requested_ = false;
  in_underrun_ = false;
  state_ = AudioOutputState::Opened;
  return true;
}

void AudioOutputEngine::close() {
  std::lock_guard lock{mutex_};
  backend_.close_device();
  queue_.clear();
  queue_head_ = 0;
  device_format_.reset();
  latched_error_.reset();
  metrics_ = AudioOutputMetrics{};
  drain_requested_ = false;
  in_underrun_ = false;
  state_ = AudioOutputState::Closed;
}

AudioEnqueueResult AudioOutputEngine::enqueue(const AudioBlock& block) {
  std::lock_guard lock{mutex_};
  if (state_ != AudioOutputState::Opened && state_ != AudioOutputState::Running) {
    return AudioEnqueueResult::InvalidState;
  }
  if (block.sample_rate() != source_format_.sample_rate ||
      block.channel_count() != source_format_.channel_count) {
    return AudioEnqueueResult::FormatMismatch;
  }
  if (!channels_negotiable(block.channel_count(), device_format_->channel_count)) {
    return AudioEnqueueResult::UnsupportedChannels;
  }
  ConvertedAudioBlock converted{};
  try {
    converted = convert_block_for_device(block, *device_format_);
  } catch (const AudioConversionError&) {
    return AudioEnqueueResult::UnsupportedChannels;
  }
  if (converted.frame_count > limits_.queue_frame_capacity) {
    return AudioEnqueueResult::BlockTooLarge;
  }
    if (buffered_frames() + converted.frame_count > limits_.queue_frame_capacity) {
      return AudioEnqueueResult::Backpressure;
    }
  queue_.insert(queue_.end(), converted.frames.begin(), converted.frames.end());
  metrics_.source_frames_accepted += block.frame_count();
  return AudioEnqueueResult::Accepted;
}

bool AudioOutputEngine::start() {
  std::lock_guard lock{mutex_};
  if (state_ != AudioOutputState::Opened || buffered_frames() == 0) {
    return false;
  }
  if (!backend_.start_device()) {
    const auto backend_message = backend_.backend_error();
    enter_error(AudioOutputError::Code::BackendFailure,
                backend_message.value_or("audio backend refused to start"));
    return false;
  }
  metrics_.device_frames_submitted = 0;
  metrics_.device_frames_processed = 0;
  in_underrun_ = false;
  state_ = AudioOutputState::Running;
  return true;
}

bool AudioOutputEngine::request_drain() {
  std::lock_guard lock{mutex_};
  if (state_ != AudioOutputState::Running) {
    return false;
  }
  drain_requested_ = true;
  state_ = AudioOutputState::Draining;
  return true;
}

bool AudioOutputEngine::stop() {
  std::lock_guard lock{mutex_};
  if (state_ == AudioOutputState::Closed || state_ == AudioOutputState::Error) {
    return false;
  }
  backend_.stop_device();
  queue_.clear();
  queue_head_ = 0;
  reset_session_counters();
  drain_requested_ = false;
  in_underrun_ = false;
  state_ = AudioOutputState::Opened;
  return true;
}

bool AudioOutputEngine::flush() {
  std::lock_guard lock{mutex_};
  if (state_ != AudioOutputState::Opened && state_ != AudioOutputState::Running &&
      state_ != AudioOutputState::Draining) {
    return false;
  }
  backend_.reset_device();
  queue_.clear();
  queue_head_ = 0;
  reset_session_counters();
  in_underrun_ = false;
  return true;
}

bool AudioOutputEngine::transfer_to_backend() {
  std::lock_guard lock{mutex_};
  if (state_ == AudioOutputState::Closed || state_ == AudioOutputState::Error ||
      state_ == AudioOutputState::Completed) {
    return false;
  }
  if (const auto failure = backend_.backend_error(); failure.has_value()) {
    enter_error(AudioOutputError::Code::BackendFailure, *failure);
    return false;
  }
  if (state_ == AudioOutputState::Opened) {
    return true;
  }
  const std::size_t writable = backend_.writable_frames();
  std::size_t transfer = buffered_frames();
  if (transfer > writable) {
    transfer = writable;
  }
  if (transfer > 0) {
    const std::size_t taken = backend_.write_frames(queue_.data() + queue_head_, transfer);
    const std::size_t consumed = taken > transfer ? transfer : taken;
    queue_head_ += consumed * frame_size_bytes();
    if (queue_head_ == queue_.size()) {
      queue_.clear();
      queue_head_ = 0;
    }
    metrics_.device_frames_submitted += consumed;
  }
  // Backend progress only: clamp into [processed, submitted] so rounding
  // or over-reporting can neither move it backward nor past submissions.
  const std::uint64_t backend_processed = backend_.processed_frames();
  const std::uint64_t bounded = backend_processed > metrics_.device_frames_submitted
                                    ? metrics_.device_frames_submitted
                                    : backend_processed;
  if (bounded > metrics_.device_frames_processed) {
    metrics_.device_frames_processed = bounded;
  }
  metrics_.buffered_device_frames = buffered_frames();

  if (state_ == AudioOutputState::Running && !drain_requested_ && transfer == 0 &&
      buffered_frames() == 0) {
    if (!in_underrun_) {
      ++metrics_.underrun_count;
      in_underrun_ = true;
    }
  } else {
    in_underrun_ = false;
  }

  // Backend timestamps quantize (microseconds, device-side rounding), so the
  // trailing processed count may stall up to ~1ms short of submissions.
  // Completion tolerates one millisecond of device frames.
  const std::uint64_t completion_tolerance =
      device_format_.has_value()
          ? std::max<std::uint64_t>(
                1, static_cast<std::uint64_t>(device_format_->sample_rate) / 1000ULL)
          : 1ULL;
  if (state_ == AudioOutputState::Draining && buffered_frames() == 0 &&
      metrics_.device_frames_processed + completion_tolerance >=
          metrics_.device_frames_submitted) {
    backend_.stop_device();
    state_ = AudioOutputState::Completed;
  }
  return true;
}

AudioOutputState AudioOutputEngine::state() const {
  std::lock_guard lock{mutex_};
  return state_;
}

std::optional<AudioDeviceFormat> AudioOutputEngine::device_format() const {
  std::lock_guard lock{mutex_};
  return device_format_;
}

AudioOutputMetrics AudioOutputEngine::metrics() const {
  std::lock_guard lock{mutex_};
  auto snapshot = metrics_;
  snapshot.buffered_device_frames = buffered_frames();
  return snapshot;
}

std::optional<AudioOutputError> AudioOutputEngine::error() const {
  std::lock_guard lock{mutex_};
  return latched_error_;
}

void AudioOutputEngine::enter_error(AudioOutputError::Code code, std::string message) {
  latched_error_ = AudioOutputError{code, std::move(message)};
  state_ = AudioOutputState::Error;
}

void AudioOutputEngine::reset_session_counters() noexcept {
  metrics_.source_frames_accepted = 0;
  metrics_.device_frames_submitted = 0;
  metrics_.device_frames_processed = 0;
  metrics_.buffered_device_frames = 0;
}

std::size_t AudioOutputEngine::frame_size_bytes() const noexcept {
  if (!device_format_.has_value()) {
    return 0;
  }
  return static_cast<std::size_t>(device_format_->channel_count) *
         device_sample_bytes(device_format_->sample_format);
}

std::size_t AudioOutputEngine::buffered_frames() const noexcept {
  const std::size_t size = frame_size_bytes();
  if (size == 0) {
    return 0;
  }
  return (queue_.size() - queue_head_) / size;
}

} // namespace pulsar::application
