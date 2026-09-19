#include <pulsar/media/qt_audio/qt_audio_output.hpp>

#include <pulsar/application/audio/audio_converter.hpp>
#include <pulsar/application/audio/audio_output_engine.hpp>
#include <pulsar/application/media/audio_block.hpp>

#include <QAudioDevice>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <QMetaObject>
#include <QObject>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace pulsar::media::qt_audio {
namespace {

using application::AudioDeviceBackend;
using application::AudioDeviceFormat;
using application::AudioEnqueueResult;
using application::AudioOutputError;
using application::AudioOutputMetrics;
using application::AudioOutputState;
using application::AudioSampleFormat;
using application::AudioSourceFormat;

[[nodiscard]] QAudioFormat to_qt_format(const AudioDeviceFormat& format) {
  QAudioFormat result;
  result.setSampleRate(format.sample_rate);
  result.setChannelCount(format.channel_count);
  switch (format.sample_format) {
  case AudioSampleFormat::Float32:
    result.setSampleFormat(QAudioFormat::Float);
    break;
  case AudioSampleFormat::Int16:
    result.setSampleFormat(QAudioFormat::Int16);
    break;
  case AudioSampleFormat::UInt8:
    result.setSampleFormat(QAudioFormat::UInt8);
    break;
  }
  return result;
}

[[nodiscard]] std::optional<AudioDeviceFormat> from_qt_format(const QAudioFormat& format) {
  AudioSampleFormat sample_format = AudioSampleFormat::Float32;
  switch (format.sampleFormat()) {
  case QAudioFormat::Float:
    sample_format = AudioSampleFormat::Float32;
    break;
  case QAudioFormat::Int16:
    sample_format = AudioSampleFormat::Int16;
    break;
  case QAudioFormat::UInt8:
    sample_format = AudioSampleFormat::UInt8;
    break;
  default:
    return std::nullopt;
  }
  if (format.sampleRate() <= 0 || format.channelCount() <= 0) {
    return std::nullopt;
  }
  return AudioDeviceFormat{format.sampleRate(), format.channelCount(), sample_format};
}

[[nodiscard]] std::string sink_error_text(QAudio::Error error) {
  switch (error) {
  case QAudio::NoError:
    return "no error";
  case QAudio::OpenError:
    return "device open error";
  case QAudio::IOError:
    return "device I/O error";
  case QAudio::UnderrunError:
    return "device underrun";
  case QAudio::FatalError:
    return "fatal device error";
  }
  return "unknown device error";
}

// Helper living on the owner thread so cross-thread device calls can block
// on it without touching QAudioSink off-thread.
class OwnerGate final : public QObject {
public:
  explicit OwnerGate(QObject* parent = nullptr) : QObject{parent} {}
};

} // namespace

class QtAudioOutput::Impl final : public AudioDeviceBackend {
public:
  Impl(QtAudioOutput::Options options, QThread* owner)
      : options_{std::move(options)}, owner_{owner}, gate_{std::make_unique<OwnerGate>()} {
    pump_.setInterval(10);
    pump_.setSingleShot(false);
    QObject::connect(&pump_, &QTimer::timeout, gate_.get(), [this] { pump_once(); });
  }

  ~Impl() override = default;

  // Non-blocking destruction path: invalidate first so stray pump events
  // and in-flight marshaled calls become safe no-ops, then release Qt
  // resources without ever waiting on the owner thread.
  void teardown_for_destruction() {
    invalidated_.store(true);
    if (QThread::currentThread() == owner_) {
      pump_.stop();
      teardown_device();
      return;
    }
    QMetaObject::invokeMethod(gate_.get(), [this] { pump_.stop(); }, Qt::QueuedConnection);
    if (sink_ != nullptr) {
      sink_->deleteLater();
      sink_ = nullptr;
      io_ = nullptr;
    }
  }

  void bind_engine() {
    engine_ = std::make_unique<application::AudioOutputEngine>(*this, options_.limits);
  }

  [[nodiscard]] application::AudioOutputEngine& engine() noexcept { return *engine_; }

  template <typename Fn>
  auto on_owner(Fn&& action, const char* what)
      -> std::optional<decltype(std::declval<Fn>()())> {
    using Result = decltype(std::declval<Fn>()());
    if (QThread::currentThread() == owner_) {
      return action();
    }
    std::optional<Result> outcome;
    const bool delivered = QMetaObject::invokeMethod(
        gate_.get(), [&] { outcome.emplace(action()); }, Qt::BlockingQueuedConnection);
    if (!delivered) {
      std::lock_guard lock{mutex_};
      latched_error_ = std::string{"audio output owner thread is gone during "} + what;
      return std::nullopt;
    }
    return outcome;
  }

  void pump_once() {
    if (invalidated_.load() || engine_ == nullptr) {
      pump_.stop();
      return;
    }
    engine_->transfer_to_backend();
    const auto state = engine_->state();
    if (state == AudioOutputState::Completed || state == AudioOutputState::Error ||
        state == AudioOutputState::Closed) {
      pump_.stop();
    }
  }

  [[nodiscard]] bool engine_ready() const noexcept { return engine_ != nullptr; }

  void wake_pump() {
    QMetaObject::invokeMethod(gate_.get(), [this] { pump_once(); }, Qt::QueuedConnection);
  }

  // AudioDeviceBackend: every method marshals to the owner thread.
  // After invalidate() (destruction), all methods fail fast without
  // touching Qt or waiting on any thread.
  std::optional<AudioDeviceFormat>
  query_device_format(const AudioSourceFormat& requested) override {
    if (invalidated_.load()) {
      return std::nullopt;
    }
    const auto outcome = on_owner(
        [&]() -> std::optional<AudioDeviceFormat> {
          const auto device = select_device();
          if (device.isNull()) {
            return std::nullopt;
          }
          QAudioFormat exact;
          exact.setSampleRate(requested.sample_rate);
          exact.setChannelCount(requested.channel_count);
          exact.setSampleFormat(QAudioFormat::Float);
          if (device.isFormatSupported(exact)) {
            return AudioDeviceFormat{requested.sample_rate, requested.channel_count,
                                     AudioSampleFormat::Float32};
          }
          return from_qt_format(device.preferredFormat());
        },
        "device query");
    if (!outcome.has_value()) {
      return std::nullopt;
    }
    return *outcome;
  }

  bool open_device(const AudioDeviceFormat& format) override {
    if (invalidated_.load()) {
      return false;
    }
    const auto outcome = on_owner(
        [&]() -> bool {
          teardown_device();
          baseline_processed_ = 0;
          const auto device = select_device();
          if (device.isNull()) {
            set_error("no audio output device");
            return false;
          }
          format_ = format;
          sink_ = new QAudioSink{device, to_qt_format(format)};
          QObject::connect(sink_, &QAudioSink::stateChanged, gate_.get(),
                           [this](QAudio::State) { poll_sink_error(); });
          sink_->setBufferSize(static_cast<quint32>(
              options_.limits.queue_frame_capacity * frame_size_bytes()));
          clear_error();
          return true;
        },
        "device open");
    return outcome.value_or(false);
  }

  bool close_device() override {
    if (invalidated_.load()) {
      return true;
    }
    const auto outcome = on_owner(
        [&]() -> bool {
          pump_.stop();
          teardown_device();
          baseline_processed_ = 0;
          clear_error();
          return true;
        },
        "device close");
    return outcome.value_or(false);
  }

  bool start_device() override {
    if (invalidated_.load()) {
      return false;
    }
    const auto outcome = on_owner(
        [&]() -> bool {
          if (sink_ == nullptr) {
            set_error("audio output is not configured");
            return false;
          }
          // A sink (re)start opens a new processed-progress epoch:
          // processedUSecs() restarts near zero, so the previous baseline
          // must not leak forward or fresh audio looks already processed.
          io_ = sink_->start();
          if (io_ == nullptr) {
            set_error("audio device refused to start: " + sink_error_text(sink_->error()));
            return false;
          }
          baseline_processed_ = 0;
          clear_error();
          pump_.start();
          return true;
        },
        "device start");
    return outcome.value_or(false);
  }

  bool stop_device() override {
    if (invalidated_.load()) {
      return true;
    }
    const auto outcome = on_owner(
        [&]() -> bool {
          pump_.stop();
          if (sink_ != nullptr) {
            sink_->stop();
          }
          io_ = nullptr;
          return true;
        },
        "device stop");
    return outcome.value_or(false);
  }

  bool reset_device() override {
    if (invalidated_.load()) {
      return true;
    }
    const auto outcome = on_owner(
        [&]() -> bool {
          if (sink_ == nullptr) {
            return true;
          }
          // Verified against the real backend: reset() halts to Stopped and
          // zeroes counters, so the stream is re-acquired to keep playing.
          // The re-acquired stream is a new processed-progress epoch.
          sink_->reset();
          baseline_processed_ = 0;
          io_ = sink_->start();
          if (io_ == nullptr) {
            set_error("audio device refused to restart after flush: " +
                      sink_error_text(sink_->error()));
            return false;
          }
          return true;
        },
        "device reset");
    return outcome.value_or(false);
  }

  std::size_t writable_frames() override {
    if (invalidated_.load()) {
      return 0;
    }
    const auto outcome = on_owner(
        [&]() -> std::size_t {
          if (sink_ == nullptr || io_ == nullptr) {
            return 0;
          }
          const std::size_t frame_size = frame_size_bytes();
          if (frame_size == 0) {
            return 0;
          }
          return static_cast<std::size_t>(sink_->bytesFree()) / frame_size;
        },
        "writable query");
    return outcome.value_or(0);
  }

  std::size_t write_frames(const void* frames, std::size_t frame_count) override {
    if (invalidated_.load()) {
      return 0;
    }
    struct Written {
      std::size_t frames{0};
    };
    if (QThread::currentThread() == owner_) {
      return write_on_owner(frames, frame_count);
    }
    Written result;
    const bool delivered = QMetaObject::invokeMethod(
        gate_.get(),
        [&] {
          result.frames = write_on_owner(frames, frame_count);
        },
        Qt::BlockingQueuedConnection);
    if (!delivered) {
      std::lock_guard lock{mutex_};
      latched_error_ = "audio output owner thread is gone during device write";
      return 0;
    }
    return result.frames;
  }

  std::uint64_t processed_frames() override {
    if (invalidated_.load()) {
      return baseline_processed_;
    }
    const auto outcome = on_owner(
        [&]() -> std::uint64_t {
          if (sink_ == nullptr) {
            return baseline_processed_;
          }
          const std::uint64_t microseconds =
              static_cast<std::uint64_t>(sink_->processedUSecs());
          const std::uint64_t rate = static_cast<std::uint64_t>(format_.sample_rate);
          const std::uint64_t frames =
              (microseconds / 1'000'000ULL) * rate + ((microseconds % 1'000'000ULL) * rate) /
                                                              1'000'000ULL;
          if (frames > baseline_processed_) {
            baseline_processed_ = frames;
          }
          return baseline_processed_;
        },
        "progress query");
    return outcome.value_or(baseline_processed_);
  }

  // Backend exhaustion authority for drain completion: the sink holds no
  // unplayed audio once it returns to Idle after submissions. Never the
  // microsecond counter alone.
  bool is_playback_exhausted() override {
    if (invalidated_.load()) {
      return true;
    }
    const auto outcome = on_owner(
        [&]() -> bool {
          return sink_ == nullptr || sink_->state() == QAudio::IdleState;
        },
        "exhaustion query");
    return outcome.value_or(true);
  }

  std::optional<std::string> backend_error() override {
    if (invalidated_.load()) {
      return std::nullopt;
    }
    std::lock_guard lock{mutex_};
    if (latched_error_.has_value()) {
      return latched_error_;
    }
    if (QThread::currentThread() == owner_) {
      poll_sink_error_locked();
    }
    return latched_error_;
  }

private:
  [[nodiscard]] std::size_t frame_size_bytes() const noexcept {
    return static_cast<std::size_t>(format_.channel_count) *
           application::device_sample_bytes(format_.sample_format);
  }

  [[nodiscard]] QAudioDevice select_device() const {
    if (!options_.device_id.empty()) {
      for (const auto& candidate : QMediaDevices::audioOutputs()) {
        if (candidate.id().toStdString() == options_.device_id) {
          return candidate;
        }
      }
      return QAudioDevice{};
    }
    return QMediaDevices::defaultAudioOutput();
  }

  void set_error(std::string message) {
    std::lock_guard lock{mutex_};
    latched_error_ = std::move(message);
  }

  void clear_error() {
    std::lock_guard lock{mutex_};
    latched_error_.reset();
  }

  // Assumes mutex_ is already held by the caller.
  void poll_sink_error_locked() {
    if (sink_ != nullptr && sink_->error() != QAudio::NoError && !latched_error_.has_value()) {
      latched_error_ = "audio device failure: " + sink_error_text(sink_->error());
    }
  }

  // Acquires mutex_ exactly once; never call while holding it.
  void poll_sink_error() {
    std::lock_guard lock{mutex_};
    poll_sink_error_locked();
  }

  void teardown_device() {
    pump_.stop();
    io_ = nullptr;
    delete sink_;
    sink_ = nullptr;
  }

  std::size_t write_on_owner(const void* frames, std::size_t frame_count) {
    if (sink_ == nullptr || io_ == nullptr || frame_count == 0) {
      return 0;
    }
    const std::size_t frame_size = frame_size_bytes();
    if (frame_size == 0) {
      return 0;
    }
    std::size_t allowed = static_cast<std::size_t>(sink_->bytesFree()) / frame_size;
    if (allowed > frame_count) {
      allowed = frame_count;
    }
    if (allowed == 0) {
      return 0;
    }
    const qint64 written =
        io_->write(static_cast<const char*>(frames), static_cast<qint64>(allowed * frame_size));
    if (written < 0) {
      set_error("audio device write failed");
      return 0;
    }
    // Complete frames only: a short write keeps the remainder queued.
    return static_cast<std::size_t>(written) / frame_size;
  }

  std::unique_ptr<application::AudioOutputEngine> engine_;
  QtAudioOutput::Options options_;
  QThread* owner_;
  std::unique_ptr<OwnerGate> gate_;
  QTimer pump_;
  QAudioSink* sink_{nullptr};
  QIODevice* io_{nullptr};
  AudioDeviceFormat format_{0, 0, AudioSampleFormat::Float32};
  std::uint64_t baseline_processed_{0};
  mutable std::mutex mutex_;
  std::optional<std::string> latched_error_;
  std::atomic<bool> invalidated_{false};
};

QtAudioOutput::QtAudioOutput(Options options)
    : impl_{std::make_unique<Impl>(std::move(options), QThread::currentThread())} {
  impl_->bind_engine();
}

// Destruction contract: no in-flight lifecycle operation, and (like most
// Qt-bound objects) prefer the owner thread. Never blocks: invalidates the
// backend so stray pump events and marshaled calls become safe no-ops,
// then releases Qt resources without waiting on any thread. Prefer an
// explicit close() that returned true before destroying.
QtAudioOutput::~QtAudioOutput() {
  if (impl_ != nullptr) {
    impl_->teardown_for_destruction();
  }
}

bool QtAudioOutput::open(const AudioSourceFormat& source) {
  return impl_->engine().open(source);
}

bool QtAudioOutput::close() {
  return impl_->engine().close();
}

AudioEnqueueResult QtAudioOutput::enqueue(const application::AudioBlock& block) {
  const auto result = impl_->engine().enqueue(block);
  if (result == AudioEnqueueResult::Accepted) {
    // Wake the owner-thread pump without blocking the producer.
    impl_->wake_pump();
  }
  return result;
}

bool QtAudioOutput::start() {
  return impl_->engine().start();
}

bool QtAudioOutput::request_drain() {
  return impl_->engine().request_drain();
}

bool QtAudioOutput::stop() {
  return impl_->engine().stop();
}

bool QtAudioOutput::flush() {
  return impl_->engine().flush();
}

AudioOutputState QtAudioOutput::state() const {
  return impl_->engine().state();
}

std::optional<AudioDeviceFormat> QtAudioOutput::device_format() const {
  return impl_->engine().device_format();
}

AudioOutputMetrics QtAudioOutput::metrics() const {
  return impl_->engine().metrics();
}

std::optional<AudioOutputError> QtAudioOutput::error() const {
  return impl_->engine().error();
}

} // namespace pulsar::media::qt_audio
