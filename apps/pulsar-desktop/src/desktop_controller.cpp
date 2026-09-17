#include "desktop_controller.hpp"

#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>

#include <QCoreApplication>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <sstream>
#include <utility>

namespace {

using pulsar::domain::MediaTime;
using pulsar::domain::Rational;
using pulsar::media::ffmpeg::FFmpegMediaProbe;
using pulsar::media::ffmpeg::FFmpegPreviewConverter;
using pulsar::media::ffmpeg::FFmpegPreviewFrame;
using pulsar::media::ffmpeg::FFmpegVideoDecodeSession;

constexpr double kLatePreviewSeconds = 0.25;

[[nodiscard]] QString rationalText(Rational value) {
  return QString::number(value.numerator()) + QStringLiteral("/") +
         QString::number(value.denominator());
}

[[nodiscard]] QImage imageFromPreview(const FFmpegPreviewFrame& preview) {
  QImage image{preview.width, preview.height, QImage::Format_RGBA8888};
  if (image.isNull()) {
    throw std::runtime_error{"Qt could not allocate preview image"};
  }

  const auto source_stride = static_cast<std::size_t>(preview.stride_bytes);
  for (int y = 0; y < preview.height; ++y) {
    const auto offset = static_cast<std::size_t>(y) * source_stride;
    std::memcpy(image.scanLine(y), preview.pixels.data() + offset, source_stride);
  }
  return image;
}

[[nodiscard]] QString metadataText(const pulsar::application::MediaProbeResult& probe,
                                   const pulsar::application::VideoStreamInfo& stream,
                                   const FFmpegPreviewFrame& preview) {
  QStringList lines;
  lines << QStringLiteral("Nombre: %1").arg(QString::fromStdString(probe.path.filename().string()));
  if (probe.container_name.has_value()) {
    lines << QStringLiteral("Contenedor: %1").arg(QString::fromStdString(*probe.container_name));
  }
  lines << QStringLiteral("Stream: #%1").arg(stream.stream_index);
  lines << QStringLiteral("Codec: %1").arg(QString::fromStdString(stream.codec_name));
  lines << QStringLiteral("Resolución: %1 × %2").arg(stream.width).arg(stream.height);
  if (stream.nominal_frame_rate.has_value()) {
    lines << QStringLiteral("FPS informativo: %1")
                 .arg(rationalText(stream.nominal_frame_rate->rational()));
  }
  lines << QStringLiteral("Time base: %1").arg(rationalText(stream.time_base));
  if (preview.source_image.pixel_format_name.has_value()) {
    lines << QStringLiteral("Pixel format: %1")
                 .arg(QString::fromStdString(*preview.source_image.pixel_format_name));
  }
  lines << QStringLiteral("Color range: %1")
               .arg(QString::fromStdString(preview.source_image.color_range));
  lines << QStringLiteral("Primaries: %1")
               .arg(QString::fromStdString(preview.source_image.color_primaries));
  lines << QStringLiteral("Transfer: %1")
               .arg(QString::fromStdString(preview.source_image.color_transfer));
  lines << QStringLiteral("Matrix: %1")
               .arg(QString::fromStdString(preview.source_image.color_matrix));
  lines << QStringLiteral("Preview: CPU RGBA");
  return lines.join(QLatin1Char('\n'));
}

} // namespace

struct DesktopController::OpenedMedia {
  QString media_name;
  QString metadata;
  QImage image;
  MediaTime stream_time;
  std::optional<MediaTime> duration;
  std::chrono::nanoseconds conversion_time;
};

struct DesktopController::WorkerState {
  std::unique_ptr<FFmpegVideoDecodeSession> session;
  std::optional<pulsar::application::VideoStreamInfo> stream;
  std::optional<MediaTime> origin;
  std::optional<MediaTime> duration;
  FFmpegPreviewConverter converter;
};

DesktopController::DesktopController(PreviewImageProvider& image_provider, QObject* parent)
    : QObject{parent}, image_provider_{image_provider}, transport_{clock_} {
  tick_timer_.setInterval(16);
  tick_timer_.setTimerType(Qt::PreciseTimer);
  connect(&tick_timer_, &QTimer::timeout, this, &DesktopController::updateTick);
  tick_timer_.start();
  worker_ = std::thread{&DesktopController::workerLoop, this};
}

DesktopController::~DesktopController() {
  {
    std::lock_guard lock{mutex_};
    stop_worker_ = true;
    pending_command_.reset();
  }
  wake_.notify_one();
  if (worker_.joinable()) {
    worker_.join();
  }
}

void DesktopController::openPath(const QString& path) {
  const QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }
  postOpen(std::filesystem::path{trimmed.toStdString()}, ++generation_);
}

void DesktopController::openUrl(const QUrl& url) {
  if (!url.isLocalFile()) {
    setError(QStringLiteral("Solo se admiten archivos locales en M8"));
    return;
  }
  openPath(url.toLocalFile());
}

void DesktopController::importRequested() {
  emit focusImportPathRequested();
}

void DesktopController::togglePlayback() {
  if (playing_) {
    pause();
  } else {
    play();
  }
}

void DesktopController::play() {
  if (!has_media_ || busy_) {
    return;
  }
  playing_ = true;
  transport_.play();
  emit playbackChanged();
  setStatus(QStringLiteral("Reproduciendo"));
  requestReadIfNeeded();
}

void DesktopController::pause() {
  if (!has_media_) {
    return;
  }
  transport_.pause();
  playing_ = false;
  emit playbackChanged();
  setStatus(QStringLiteral("Pausado"));
  updatePositionProperties();
}

void DesktopController::seekNormalized(double value) {
  if (!has_media_ || !duration_.has_value()) {
    return;
  }
  const double bounded = std::clamp(value, 0.0, 1.0);
  const MediaTime target_relative = mediaTimeFromSeconds(secondsAsDouble(*duration_) * bounded);
  const MediaTime target_stream = streamFromRelative(target_relative);
  ++generation_;
  ++seek_count_;
  read_in_flight_ = false;
  pending_frame_.reset();
  transport_.pause();
  setBusy(true);
  setStatus(QStringLiteral("Buscando…"));
  setPreviewLate(false);
  seek_latency_timer_.restart();
  postSeek(target_stream, generation_);
}

void DesktopController::seekToStart() {
  seekNormalized(0.0);
}

void DesktopController::seekToEnd() {
  seekNormalized(1.0);
}

void DesktopController::closeMedia() {
  ++generation_;
  transport_.pause();
  playing_ = false;
  has_media_ = false;
  read_in_flight_ = false;
  pending_frame_.reset();
  origin_stream_time_.reset();
  duration_.reset();
  visible_stream_time_.reset();
  image_provider_.clear();
  preview_source_.clear();
  media_name_.clear();
  metadata_.clear();
  duration_text_ = QStringLiteral("--:--:--.---");
  progress_ = 0.0;
  position_text_ = QStringLiteral("00:00:00.000");
  setBusy(false);
  setStatus(QStringLiteral("Vacío"));
  setTechnicalStatus(QStringLiteral("Preview CPU"));
  clearError();
  emit previewChanged();
  emit mediaChanged();
  emit playbackChanged();
  emit positionChanged();
  postClose(generation_);
}

void DesktopController::postOpen(std::filesystem::path path, std::uint64_t generation) {
  transport_.pause();
  playing_ = false;
  read_in_flight_ = false;
  pending_frame_.reset();
  image_provider_.clear();
  preview_source_.clear();
  origin_stream_time_.reset();
  duration_.reset();
  visible_stream_time_.reset();
  setBusy(true);
  setStatus(QStringLiteral("Preparando…"));
  setTechnicalStatus(QStringLiteral("Opening media"));
  clearError();
  emit playbackChanged();
  emit previewChanged();
  enqueue(WorkerCommand{WorkerCommand::Kind::Open, std::move(path), std::nullopt, generation});
}

void DesktopController::postSeek(MediaTime target, std::uint64_t generation) {
  enqueue(WorkerCommand{WorkerCommand::Kind::Seek, {}, target, generation});
}

void DesktopController::postRead(std::uint64_t generation) {
  enqueue(WorkerCommand{WorkerCommand::Kind::Read, {}, std::nullopt, generation});
}

void DesktopController::postClose(std::uint64_t generation) {
  enqueue(WorkerCommand{WorkerCommand::Kind::Close, {}, std::nullopt, generation});
}

void DesktopController::enqueue(WorkerCommand command) {
  {
    std::lock_guard lock{mutex_};
    if (command.kind == WorkerCommand::Kind::Read && pending_command_.has_value()) {
      return;
    }
    pending_command_ = std::move(command);
  }
  wake_.notify_one();
}

void DesktopController::workerLoop() {
  WorkerState state;
  while (true) {
    WorkerCommand command;
    {
      std::unique_lock lock{mutex_};
      wake_.wait(lock, [this] { return stop_worker_ || pending_command_.has_value(); });
      if (stop_worker_) {
        return;
      }
      command = std::move(*pending_command_);
      pending_command_.reset();
      worker_busy_ = true;
    }

    try {
      if (command.kind == WorkerCommand::Kind::Close) {
        state = WorkerState{};
      } else if (command.kind == WorkerCommand::Kind::Open) {
        const auto open_started = std::chrono::steady_clock::now();
        FFmpegMediaProbe probe;
        const auto probe_result = probe.probe(command.path);
        if (probe_result.video_streams.empty()) {
          throw std::runtime_error{"media has no video stream"};
        }
        const auto stream = probe_result.video_streams.front();
        auto session = std::make_unique<FFmpegVideoDecodeSession>(probe_result.path, stream.stream_index);
        auto visual = session->decode_next_visual();
        if (!visual.has_value() || !visual->presentation_timestamp.has_value()) {
          throw std::runtime_error{"could not decode an initial timestamped video frame"};
        }
        const auto preview = state.converter.convert(*visual);
        OpenedMedia opened{QString::fromStdString(probe_result.path.filename().string()),
                           metadataText(probe_result, stream, preview),
                           imageFromPreview(preview),
                           *visual->presentation_timestamp,
                           stream.duration.has_value() ? stream.duration : probe_result.duration,
                           preview.conversion_time};
        state.session = std::move(session);
        state.stream = stream;
        state.origin = opened.stream_time;
        state.duration = opened.duration;
        QMetaObject::invokeMethod(
            this,
            [this, generation = command.generation, opened = std::move(opened), open_started]() mutable {
              if (generation != generation_) {
                ++stale_visual_frames_;
                updateDiagnostics();
                return;
              }
              media_name_ = opened.media_name;
              metadata_ = opened.metadata;
              origin_stream_time_ = opened.stream_time;
              duration_ = opened.duration;
              duration_text_ = duration_.has_value() ? formatTime(*duration_)
                                                     : QStringLiteral("--:--:--.---");
              has_media_ = true;
              last_conversion_time_ = opened.conversion_time;
              const auto open_latency = std::chrono::steady_clock::now() - open_started;
              setTechnicalStatus(QStringLiteral("Ready · first frame %1 ms")
                                     .arg(std::chrono::duration_cast<std::chrono::milliseconds>(
                                              open_latency)
                                              .count()));
              displayFrame(DecodedFrame{std::move(opened.image), opened.stream_time,
                                        opened.conversion_time},
                           true);
              setBusy(false);
              setStatus(QStringLiteral("Pausado"));
              emit mediaChanged();
              emit playbackChanged();
            },
            Qt::QueuedConnection);
      } else if (command.kind == WorkerCommand::Kind::Seek) {
        if (state.session == nullptr || !command.target.has_value()) {
          throw std::runtime_error{"seek requested before media was open"};
        }
        auto result = state.session->seek_to_presentation_timestamp_visual(*command.target);
        if (!result.frame.has_value()) {
          throw std::runtime_error{"seek target is before the first presentable frame"};
        }
        const auto preview = state.converter.convert(*result.frame);
        DecodedFrame decoded{imageFromPreview(preview), *result.frame->presentation_timestamp,
                             preview.conversion_time};
        QMetaObject::invokeMethod(
            this,
            [this, generation = command.generation, decoded = std::move(decoded)]() mutable {
              if (generation != generation_) {
                ++stale_visual_frames_;
                updateDiagnostics();
                return;
              }
              if (seek_latency_timer_.isValid()) {
                last_seek_visible_latency_ = std::chrono::milliseconds{seek_latency_timer_.elapsed()};
              }
              displayFrame(std::move(decoded), true);
              setBusy(false);
              setStatus(playing_ ? QStringLiteral("Reproduciendo") : QStringLiteral("Pausado"));
              if (playing_) {
                transport_.play();
              }
              requestReadIfNeeded();
            },
            Qt::QueuedConnection);
      } else if (command.kind == WorkerCommand::Kind::Read) {
        if (state.session == nullptr) {
          throw std::runtime_error{"read requested before media was open"};
        }
        auto visual = state.session->decode_next_visual();
        if (!visual.has_value()) {
          QMetaObject::invokeMethod(
              this,
              [this, generation = command.generation] {
                if (generation != generation_) {
                  ++stale_visual_frames_;
                  updateDiagnostics();
                  return;
                }
                read_in_flight_ = false;
                transport_.pause();
                playing_ = false;
                setStatus(QStringLiteral("Fin"));
                emit playbackChanged();
              },
              Qt::QueuedConnection);
        } else if (visual->presentation_timestamp.has_value()) {
          const auto preview = state.converter.convert(*visual);
          DecodedFrame decoded{imageFromPreview(preview), *visual->presentation_timestamp,
                               preview.conversion_time};
          QMetaObject::invokeMethod(
              this,
              [this, generation = command.generation, decoded = std::move(decoded)]() mutable {
                if (generation != generation_) {
                  ++stale_visual_frames_;
                  updateDiagnostics();
                  return;
                }
                read_in_flight_ = false;
                last_conversion_time_ = decoded.conversion_time;
                const MediaTime target = currentStreamTarget();
                if (decoded.stream_time <= target) {
                  displayFrame(std::move(decoded), false);
                  requestReadIfNeeded();
                } else {
                  pending_frame_ = std::move(decoded);
                }
              },
              Qt::QueuedConnection);
        }
      }
    } catch (const std::exception& error) {
      QMetaObject::invokeMethod(
          this,
          [this, generation = command.generation, message = QString::fromUtf8(error.what())] {
            if (generation != generation_) {
              ++stale_visual_frames_;
              updateDiagnostics();
              return;
            }
            read_in_flight_ = false;
            setBusy(false);
            setError(message);
            setStatus(QStringLiteral("Error"));
            transport_.pause();
            playing_ = false;
            emit playbackChanged();
          },
          Qt::QueuedConnection);
    }

    {
      std::lock_guard lock{mutex_};
      worker_busy_ = false;
    }
  }
}

void DesktopController::requestReadIfNeeded() {
  if (!has_media_ || !playing_ || busy_ || read_in_flight_ || pending_frame_.has_value()) {
    return;
  }
  ++read_count_;
  read_in_flight_ = true;
  postRead(generation_);
}

void DesktopController::setBusy(bool busy) {
  if (busy_ == busy) {
    return;
  }
  busy_ = busy;
  emit busyChanged();
}

void DesktopController::setStatus(QString status) {
  if (status_ == status) {
    return;
  }
  status_ = std::move(status);
  emit statusChanged();
}

void DesktopController::setTechnicalStatus(QString status) {
  if (technical_status_ == status) {
    return;
  }
  technical_status_ = std::move(status);
  emit technicalStatusChanged();
}

void DesktopController::setPreviewLate(bool late) {
  if (preview_late_ == late) {
    return;
  }
  preview_late_ = late;
  emit technicalStatusChanged();
}

void DesktopController::setError(QString message) {
  error_message_ = std::move(message);
  emit errorChanged();
}

void DesktopController::clearError() {
  if (error_message_.isEmpty()) {
    return;
  }
  error_message_.clear();
  emit errorChanged();
}

void DesktopController::displayFrame(DecodedFrame frame, bool from_seek) {
  image_provider_.setImage(std::move(frame.image));
  visible_stream_time_ = frame.stream_time;
  last_conversion_time_ = frame.conversion_time;
  ++displayed_frames_;
  ++preview_revision_;
  preview_source_ = QStringLiteral("image://pulsar-preview/current?rev=%1").arg(preview_revision_);
  if (from_seek) {
    const MediaTime relative = relativeFromStream(frame.stream_time);
    transport_.seek(pulsar::application::PlaybackPosition{relative});
  }
  updatePositionProperties();
  updateDiagnostics();
  emit previewChanged();
}

void DesktopController::updateTick() {
  if (!has_media_) {
    return;
  }

  if (playing_ && pending_frame_.has_value()) {
    const MediaTime target = currentStreamTarget();
    if (pending_frame_->stream_time <= target) {
      auto frame = std::move(*pending_frame_);
      pending_frame_.reset();
      displayFrame(std::move(frame), false);
      requestReadIfNeeded();
    }
  }

  if (playing_) {
    requestReadIfNeeded();
  }

  updatePositionProperties();
  if (visible_stream_time_.has_value()) {
    const double lateness = secondsAsDouble(currentStreamTarget() - *visible_stream_time_);
    setPreviewLate(lateness > kLatePreviewSeconds);
    if (preview_late_) {
      setTechnicalStatus(QStringLiteral("Preview atrasada · CPU"));
    }
  }
}

void DesktopController::updatePositionProperties() {
  const MediaTime position = currentPlaybackPosition();
  position_text_ = formatTime(position);
  if (duration_.has_value() && secondsAsDouble(*duration_) > 0.0) {
    progress_ = std::clamp(secondsAsDouble(position) / secondsAsDouble(*duration_), 0.0, 1.0);
  } else {
    progress_ = 0.0;
  }
  emit positionChanged();
}

void DesktopController::updateDiagnostics() {
  debug_metrics_ = QStringLiteral(
                       "displayed=%1 dropped_visual=%2 stale=%3 reads=%4 seeks=%5 conversion_ms=%6 seek_visible_ms=%7")
                       .arg(displayed_frames_)
                       .arg(dropped_visual_frames_)
                       .arg(stale_visual_frames_)
                       .arg(read_count_)
                       .arg(seek_count_)
                       .arg(std::chrono::duration_cast<std::chrono::milliseconds>(
                                last_conversion_time_)
                                .count())
                       .arg(std::chrono::duration_cast<std::chrono::milliseconds>(
                                last_seek_visible_latency_)
                                .count());
  emit diagnosticsChanged();
}

MediaTime DesktopController::currentPlaybackPosition() const {
  if (!has_media_) {
    return MediaTime::zero();
  }
  MediaTime position = transport_.position().value();
  if (duration_.has_value() && position > *duration_) {
    return *duration_;
  }
  return position;
}

MediaTime DesktopController::currentStreamTarget() const {
  return streamFromRelative(currentPlaybackPosition());
}

MediaTime DesktopController::relativeFromStream(MediaTime stream_time) const {
  if (!origin_stream_time_.has_value() || stream_time < *origin_stream_time_) {
    return MediaTime::zero();
  }
  return stream_time - *origin_stream_time_;
}

MediaTime DesktopController::streamFromRelative(MediaTime relative) const {
  if (!origin_stream_time_.has_value()) {
    return relative;
  }
  return *origin_stream_time_ + relative;
}

MediaTime DesktopController::durationOrZero() const {
  if (duration_.has_value()) {
    return *duration_;
  }
  return MediaTime::zero();
}

QString DesktopController::formatTime(MediaTime time) {
  double total_seconds = std::max(0.0, secondsAsDouble(time));
  const auto hours = static_cast<int>(total_seconds / 3600.0);
  total_seconds -= static_cast<double>(hours) * 3600.0;
  const auto minutes = static_cast<int>(total_seconds / 60.0);
  total_seconds -= static_cast<double>(minutes) * 60.0;
  const auto seconds = static_cast<int>(total_seconds);
  const auto millis = static_cast<int>(std::round((total_seconds - seconds) * 1000.0));
  return QStringLiteral("%1:%2:%3.%4")
      .arg(hours, 2, 10, QLatin1Char('0'))
      .arg(minutes, 2, 10, QLatin1Char('0'))
      .arg(seconds, 2, 10, QLatin1Char('0'))
      .arg(millis, 3, 10, QLatin1Char('0'));
}

MediaTime DesktopController::mediaTimeFromSeconds(double seconds) {
  if (!std::isfinite(seconds) || seconds <= 0.0) {
    return MediaTime::zero();
  }
  constexpr double scale = 1'000'000'000.0;
  const double raw = seconds * scale;
  const double clamped = std::min(raw, static_cast<double>(std::numeric_limits<std::int64_t>::max()));
  return MediaTime{static_cast<std::int64_t>(std::llround(clamped)), Rational{1, 1'000'000'000}};
}

double DesktopController::secondsAsDouble(MediaTime time) {
  return static_cast<double>(time.seconds());
}
