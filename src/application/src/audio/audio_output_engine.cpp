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
  {
    std::lock_guard lock{mutex_};
    if (state_ != AudioOutputState::Closed) {
      return false;
    }
    if (!valid_source(source) || limits_.queue_frame_capacity == 0) {
      enter_error(AudioOutputError::Code::UnsupportedConversion, "invalid audio output source");
      return false;
    }
    transition_ = true;
  }
  const auto offered = backend_.query_device_format(source);
  const bool backend_opened = offered.has_value() && valid_device(*offered) &&
                              backend_.open_device(*offered);
  const auto backend_message = backend_.backend_error();
  bool backend_cleanup = false;
  {
    std::lock_guard lock{mutex_};
    transition_ = false;
    if (state_ != AudioOutputState::Closed) {
      backend_cleanup = backend_opened;
    } else if (!offered.has_value() || !valid_device(*offered)) {
      enter_error(AudioOutputError::Code::NoDevice, "no usable audio output device");
    } else if (!channels_negotiable(source.channel_count, offered->channel_count)) {
      backend_cleanup = true;
      enter_error(AudioOutputError::Code::UnsupportedConversion,
                  "audio channel mapping is outside A2 scope");
    } else if (!backend_opened) {
      enter_error(AudioOutputError::Code::BackendFailure,
                  backend_message.value_or("audio backend refused to open"));
    } else {
      source_format_ = source;
      device_format_ = offered;
      converter_.emplace(source.sample_rate, source.channel_count, *offered);
      latched_error_.reset();
      queue_.clear();
      queue_head_ = 0;
      metrics_ = AudioOutputMetrics{};
      drain_requested_ = false;
      in_underrun_ = false;
      ++epoch_;
      state_ = AudioOutputState::Opened;
      return true;
    }
  }
  if (backend_cleanup) {
    backend_.close_device();
  }
  return false;
}

void AudioOutputEngine::close() {
  {
    std::lock_guard lock{mutex_};
    transition_ = true;
  }
  backend_.close_device();
  {
    std::lock_guard lock{mutex_};
    transition_ = false;
    queue_.clear();
    queue_head_ = 0;
    device_format_.reset();
    converter_.reset();
    latched_error_.reset();
    metrics_ = AudioOutputMetrics{};
    drain_requested_ = false;
    in_underrun_ = false;
    ++epoch_;
    state_ = AudioOutputState::Closed;
  }
}

AudioEnqueueResult AudioOutputEngine::enqueue(const AudioBlock& block) {
  std::lock_guard lock{mutex_};
  if (state_ != AudioOutputState::Opened && state_ != AudioOutputState::Running) {
    return AudioEnqueueResult::InvalidState;
  }
  if (!converter_.has_value()) {
    return AudioEnqueueResult::InvalidState;
  }
  if (block.sample_rate() != source_format_.sample_rate ||
      block.channel_count() != source_format_.channel_count) {
    return AudioEnqueueResult::FormatMismatch;
  }
  if (!converter_->can_convert(block.channel_count())) {
    return AudioEnqueueResult::UnsupportedChannels;
  }
  // Transactional trial: convert on a copy so a rejected block leaves the
  // streaming phase/history untouched.
  StreamingAudioConverter trial = *converter_;
  ConvertedAudioBlock converted{};
  try {
    converted = trial.convert(block);
  } catch (const AudioConversionError&) {
    return AudioEnqueueResult::UnsupportedChannels;
  }
  if (converted.frame_count > limits_.queue_frame_capacity) {
    return AudioEnqueueResult::BlockTooLarge;
  }
  if (buffered_frames() + converted.frame_count > limits_.queue_frame_capacity) {
    return AudioEnqueueResult::Backpressure;
  }
  *converter_ = std::move(trial);
  queue_.insert(queue_.end(), converted.frames.begin(), converted.frames.end());
  metrics_.source_frames_accepted += block.frame_count();
  return AudioEnqueueResult::Accepted;
}

bool AudioOutputEngine::start() {
  {
    std::lock_guard lock{mutex_};
    if (state_ != AudioOutputState::Opened || buffered_frames() == 0) {
      return false;
    }
    transition_ = true;
  }
  const bool backend_started = backend_.start_device();
  const auto backend_message = backend_.backend_error();
  bool backend_cleanup = false;
  {
    std::lock_guard lock{mutex_};
    transition_ = false;
    if (state_ != AudioOutputState::Opened) {
      backend_cleanup = backend_started;
    } else if (!backend_started) {
      enter_error(AudioOutputError::Code::BackendFailure,
                  backend_message.value_or("audio backend refused to start"));
    } else {
      metrics_.device_frames_submitted = 0;
      metrics_.device_frames_processed = 0;
      in_underrun_ = false;
      state_ = AudioOutputState::Running;
      return true;
    }
  }
  if (backend_cleanup) {
    backend_.stop_device();
  }
  return false;
}

bool AudioOutputEngine::request_drain() {
  std::lock_guard lock{mutex_};
  if (state_ != AudioOutputState::Running || !converter_.has_value()) {
    return false;
  }
  // The converter holds back trailing accepted input for want of a
  // successor sample; emit it now so no accepted tail is dropped. The tail
  // is bounded to a few device frames. Full queue + tail overflow rejects
  // deterministically (caller frees capacity and retries); nothing mutates.
  const ConvertedAudioBlock tail = converter_->flush_tail();
  if (buffered_frames() + tail.frame_count > limits_.queue_frame_capacity) {
    return false;
  }
  queue_.insert(queue_.end(), tail.frames.begin(), tail.frames.end());
  converter_->reset();
  drain_requested_ = true;
  state_ = AudioOutputState::Draining;
  return true;
}

bool AudioOutputEngine::stop() {
  {
    std::lock_guard lock{mutex_};
    if (state_ == AudioOutputState::Closed || state_ == AudioOutputState::Error) {
      return false;
    }
    transition_ = true;
  }
  backend_.stop_device();
  {
    std::lock_guard lock{mutex_};
    transition_ = false;
    if (state_ == AudioOutputState::Closed || state_ == AudioOutputState::Error) {
      return false;
    }
    queue_.clear();
    queue_head_ = 0;
    reset_session_counters();
    if (converter_.has_value()) {
      converter_->reset();
    }
    drain_requested_ = false;
    in_underrun_ = false;
    ++epoch_;
    state_ = AudioOutputState::Opened;
    return true;
  }
}

bool AudioOutputEngine::flush() {
  {
    std::lock_guard lock{mutex_};
    if (state_ != AudioOutputState::Opened && state_ != AudioOutputState::Running &&
        state_ != AudioOutputState::Draining) {
      return false;
    }
    transition_ = true;
  }
  backend_.reset_device();
  {
    std::lock_guard lock{mutex_};
    transition_ = false;
    if (state_ != AudioOutputState::Opened && state_ != AudioOutputState::Running &&
        state_ != AudioOutputState::Draining) {
      return false;
    }
    queue_.clear();
    queue_head_ = 0;
    reset_session_counters();
    if (converter_.has_value()) {
      converter_->reset();
    }
    in_underrun_ = false;
    ++epoch_;
    return true;
  }
}

bool AudioOutputEngine::transfer_to_backend() {
  AudioOutputState snapshot = AudioOutputState::Closed;
  std::size_t buffered = 0;
  std::uint64_t observed_epoch = 0;
  {
    std::lock_guard lock{mutex_};
    if (state_ == AudioOutputState::Closed || state_ == AudioOutputState::Error ||
        state_ == AudioOutputState::Completed) {
      return false;
    }
    if (state_ == AudioOutputState::Opened) {
      return true;
    }
    if (transition_) {
      return true;
    }
    snapshot = state_;
    buffered = buffered_frames();
    observed_epoch = epoch_;
  }

  if (const auto failure = backend_.backend_error(); failure.has_value()) {
    std::lock_guard lock{mutex_};
    if (state_ != AudioOutputState::Closed && state_ != AudioOutputState::Error &&
        state_ != AudioOutputState::Completed) {
      enter_error(AudioOutputError::Code::BackendFailure, *failure);
    }
    return false;
  }

  const std::size_t writable = backend_.writable_frames();
  std::size_t transfer = buffered > writable ? writable : buffered;
  std::vector<std::uint8_t> chunk;
  {
    std::lock_guard lock{mutex_};
    if (transition_ || epoch_ != observed_epoch || state_ != snapshot) {
      return true;
    }
    if (transfer > buffered_frames()) {
      transfer = buffered_frames();
    }
    chunk.assign(queue_.begin() + static_cast<std::ptrdiff_t>(queue_head_),
                 queue_.begin() + static_cast<std::ptrdiff_t>(queue_head_) +
                     static_cast<std::ptrdiff_t>(transfer * frame_size_bytes()));
    queue_head_ += transfer * frame_size_bytes();
    if (queue_head_ == queue_.size()) {
      queue_.clear();
      queue_head_ = 0;
    }
  }

  std::size_t written = 0;
  if (transfer > 0) {
    written = backend_.write_frames(chunk.data(), transfer);
    if (written > transfer) {
      written = transfer;
    }
  }
  const std::uint64_t backend_processed = backend_.processed_frames();
  const bool exhausted = backend_.is_playback_exhausted();

  bool just_completed = false;
  {
    std::lock_guard lock{mutex_};
    if (transition_ || epoch_ != observed_epoch || state_ != snapshot) {
      return true;
    }
    if (written > 0) {
      // Return unwritten tail bytes ahead of the queue head.
      const std::size_t unwritten = transfer - written;
      if (unwritten > 0) {
        queue_.insert(queue_.begin() + static_cast<std::ptrdiff_t>(queue_head_ - transfer * frame_size_bytes()),
                      chunk.end() - static_cast<std::ptrdiff_t>(unwritten * frame_size_bytes()),
                      chunk.end());
        queue_head_ -= written * frame_size_bytes();
      }
      metrics_.device_frames_submitted += written;
    }
    // Backend progress only: clamp into [processed, submitted].
    const std::uint64_t bounded = backend_processed > metrics_.device_frames_submitted
                                      ? metrics_.device_frames_submitted
                                      : backend_processed;
    if (bounded > metrics_.device_frames_processed) {
      metrics_.device_frames_processed = bounded;
    }
    metrics_.buffered_device_frames = buffered_frames();

    if (state_ == AudioOutputState::Running && !drain_requested_ && written == 0 &&
        buffered_frames() == 0) {
      if (!in_underrun_) {
        ++metrics_.underrun_count;
        in_underrun_ = true;
      }
    } else {
      in_underrun_ = false;
    }

    // Drain completes only on true backend exhaustion: adapter queue empty,
    // everything submitted, and the backend holding no unplayed audio.
    // processedUSecs() remains a metric; it is never the sole authority.
    if (state_ == AudioOutputState::Draining && buffered_frames() == 0 && exhausted &&
        (metrics_.device_frames_submitted == 0 || metrics_.device_frames_processed > 0)) {
      state_ = AudioOutputState::Completed;
      just_completed = true;
    }
  }
  if (just_completed) {
    backend_.stop_device();
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
