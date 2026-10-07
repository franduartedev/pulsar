#include "desktop_controller.hpp"
#include "builtin_sound_catalog.hpp"
#include <QFileInfo>
#include <QDir>
#include <pulsar/domain/build_info.hpp>

#include "project_path_resolution.hpp"
#include "waveform_quick_item.hpp"

#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_audio_playback_source.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_playback_worker.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_video_source.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_audio_source.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_export_media_writer.hpp>
#include <pulsar/application/export/export_job.hpp>
#if defined(PULSAR_HAS_QT_AUDIO)
#include <pulsar/media/qt_audio/qt_audio_output.hpp>
#endif
#include <pulsar/project_file/file_project_repository.hpp>
#include <pulsar/project_file/filesystem_export_output_publisher.hpp>

#include <QMetaObject>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace {
// Nearest-rank percentile over a sorted copy; on-demand reporting only.
std::chrono::nanoseconds percentile_sorted(const std::vector<std::chrono::nanoseconds>& sorted,
                                           double rank) {
  if (sorted.empty()) {
    return std::chrono::nanoseconds{0};
  }
  std::size_t index = static_cast<std::size_t>(rank * static_cast<double>(sorted.size()));
  if (index >= sorted.size()) {
    index = sorted.size() - 1;
  }
  return sorted[index];
}

std::int64_t to_microseconds(std::chrono::nanoseconds value) {
  return std::chrono::duration_cast<std::chrono::microseconds>(value).count();
}
} // namespace

namespace {
using pulsar::domain::MediaTime;
using pulsar::domain::Rational;
using pulsar::media::ffmpeg::FFmpegMediaProbe;
using pulsar::media::ffmpeg::FFmpegPlaybackWorker;
using pulsar::media::ffmpeg::FFmpegPreviewConverter;
using pulsar::media::ffmpeg::FFmpegPreviewFrame;

std::filesystem::path desktop_runtime_media_path(const std::filesystem::path& project_file,
                                                  const std::filesystem::path& authored) {
  return pulsar::desktop::is_builtin_sound_locator(authored)
             ? pulsar::desktop::builtin_sound_runtime_path(authored)
             : pulsar::desktop::resolve_runtime_media_path(project_file, authored);
}

class ResolvingExportVideoSourceFactory final
    : public pulsar::application::exporting::OfflineVideoSourceFactory {
public:
  explicit ResolvingExportVideoSourceFactory(std::filesystem::path project_file)
      : project_file_{std::move(project_file)} {}

  [[nodiscard]] std::unique_ptr<pulsar::application::exporting::OfflineVideoSource> open(
      const std::filesystem::path& logical_locator, int video_stream_index) override {
    return std::make_unique<pulsar::media::ffmpeg::FFmpegOfflineVideoSource>(
        desktop_runtime_media_path(project_file_, logical_locator), video_stream_index);
  }

private:
  std::filesystem::path project_file_;
};

class DesktopExportWriterFactory final
    : public pulsar::application::exporting::ExportMediaWriterFactory {
public:
  [[nodiscard]] std::unique_ptr<pulsar::application::exporting::ExportMediaWriter> open(
      const pulsar::application::exporting::ExportPlan& plan,
      const std::filesystem::path& temporary_output) override {
    return std::make_unique<pulsar::media::ffmpeg::FFmpegExportMediaWriter>(plan,
                                                                              temporary_output);
  }
};

QString exportErrorMessage(const pulsar::application::exporting::ExportError& error) {
  using Code = pulsar::application::exporting::ExportErrorCode;
  switch (error.code()) {
  case Code::MissingMedia: return QStringLiteral("Falta un archivo multimedia del proyecto.");
  case Code::EncoderUnavailable:
    return QStringLiteral("El codificador de vídeo/audio requerido no está disponible.");
  case Code::OutputOpenFailed:
    return QStringLiteral("No se pudo crear el archivo de exportación.");
  case Code::PublishFailed: return QStringLiteral("No se pudo guardar el archivo final.");
  case Code::Cancelled: return QStringLiteral("Exportación cancelada");
  case Code::SourceOpenFailed: return QStringLiteral("No se pudo abrir un medio del proyecto.");
  case Code::DecodeFailed: return QStringLiteral("No se pudo decodificar un medio del proyecto.");
  case Code::RenderFailed: return QStringLiteral("No se pudo renderizar la timeline.");
  case Code::MuxFailed: return QStringLiteral("No se pudo combinar vídeo y audio.");
  case Code::FinalizeFailed: return QStringLiteral("No se pudo finalizar el archivo de exportación.");
  case Code::InvalidProject: return QStringLiteral("El proyecto no se puede exportar.");
  case Code::InternalInvariant: return QStringLiteral("Ocurrió un error interno de exportación.");
  }
  return QStringLiteral("Ocurrió un error de exportación.");
}

// User-facing messages. The technical cause (what()) goes to stderr only: it can
// contain internal ids, temporary paths or codec details nobody can act on.
void logCause(const char* what, const std::exception& error) {
  std::cerr << "[pulsar] " << what << ": " << error.what() << '\n';
}
QString projectOpenMessage(const std::exception& error) {
  const std::string text = error.what();
  if (text.find("cannot read") != std::string::npos || text.find("cannot open") != std::string::npos ||
      text.find("does not exist") != std::string::npos) {
    return QStringLiteral("No se pudo leer el archivo del proyecto. Revisá que exista y que tengas permiso para abrirlo.");
  }
  return QStringLiteral("No se pudo abrir el proyecto: el archivo está dañado o no es compatible con esta versión de PULSAR. El proyecto actual no se modificó.");
}
QString projectSaveMessage() {
  return QStringLiteral("No se pudo guardar el proyecto. Revisá que la carpeta exista y que tengas permiso de escritura. Tus cambios siguen en la ventana.");
}
QString importMessage(const std::filesystem::path& path) {
  return QStringLiteral("No se pudo abrir «%1»: el formato no es compatible o el archivo está dañado.")
      .arg(QString::fromStdString(path.filename().string()));
}

const char* exportErrorCodeText(pulsar::application::exporting::ExportErrorCode code) noexcept {
  using Code = pulsar::application::exporting::ExportErrorCode;
  switch (code) {
  case Code::InvalidProject: return "InvalidProject";
  case Code::MissingMedia: return "MissingMedia";
  case Code::SourceOpenFailed: return "SourceOpenFailed";
  case Code::DecodeFailed: return "DecodeFailed";
  case Code::RenderFailed: return "RenderFailed";
  case Code::EncoderUnavailable: return "EncoderUnavailable";
  case Code::OutputOpenFailed: return "OutputOpenFailed";
  case Code::MuxFailed: return "MuxFailed";
  case Code::FinalizeFailed: return "FinalizeFailed";
  case Code::PublishFailed: return "PublishFailed";
  case Code::Cancelled: return "Cancelled";
  case Code::InternalInvariant: return "InternalInvariant";
  }
  return "Unknown";
}

QString rationalText(Rational value) { return QString::number(value.numerator()) + "/" + QString::number(value.denominator()); }
QImage imageFromPreview(const FFmpegPreviewFrame& preview) {
  QImage image{preview.width, preview.height, QImage::Format_RGBA8888};
  if (image.isNull()) throw std::runtime_error{"Qt could not allocate preview image"};
  for (int y = 0; y < preview.height; ++y) std::memcpy(image.scanLine(y), preview.pixels.data() + static_cast<std::size_t>(y) * preview.stride_bytes, static_cast<std::size_t>(preview.stride_bytes));
  return image;
}
class StaticProbe final : public pulsar::application::MediaProbe {
public:
  explicit StaticProbe(pulsar::application::MediaProbeResult result) : result_{std::move(result)} {}
  pulsar::application::MediaProbeResult probe(const std::filesystem::path&) const override { return result_; }
private: pulsar::application::MediaProbeResult result_;
};
class BuiltinSoundProbe final : public pulsar::application::MediaProbe {
public:
  explicit BuiltinSoundProbe(std::int64_t duration_frames) : duration_frames_{duration_frames} {}
  pulsar::application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    const auto duration = pulsar::domain::MediaTime{duration_frames_,
                                                     pulsar::domain::Rational{1, 48'000}};
    return {path, std::string{"WAV"}, duration, {},
            {{0, "pcm_s16le", 48'000, 1, pulsar::domain::Rational{1, 48'000}, duration}}};
  }
private:
  std::int64_t duration_frames_;
};
QString metadataText(const pulsar::application::MediaProbeResult& probe, const pulsar::application::VideoStreamInfo& stream) {
  QStringList lines;
  lines << QStringLiteral("Nombre: %1").arg(QString::fromStdString(probe.path.filename().string()));
  if (probe.container_name) lines << QStringLiteral("Contenedor: %1").arg(QString::fromStdString(*probe.container_name));
  lines << QStringLiteral("Stream: #%1").arg(stream.stream_index)
        << QStringLiteral("Codec: %1").arg(QString::fromStdString(stream.codec_name))
        << QStringLiteral("Resolución: %1 × %2").arg(stream.width).arg(stream.height);
  if (stream.nominal_frame_rate) lines << QStringLiteral("FPS informativo: %1").arg(rationalText(stream.nominal_frame_rate->rational()));
  lines << QStringLiteral("Time base: %1").arg(rationalText(stream.time_base))
        << QStringLiteral("Preview: CPU RGBA");
  return lines.join('\n');
}
QString audioMetadataText(const pulsar::application::MediaProbeResult& probe,
                          const pulsar::application::AudioStreamInfo& stream) {
  QStringList lines;
  lines << QStringLiteral("Nombre: %1").arg(QString::fromStdString(probe.path.filename().string()));
  if (probe.container_name) lines << QStringLiteral("Contenedor: %1").arg(QString::fromStdString(*probe.container_name));
  lines << QStringLiteral("Audio stream: #%1").arg(stream.stream_index)
        << QStringLiteral("Audio codec: %1").arg(QString::fromStdString(stream.codec_name))
        << QStringLiteral("Audio sample rate: %1 Hz").arg(stream.sample_rate)
        << QStringLiteral("Audio channels: %1").arg(stream.channel_count)
        << QStringLiteral("Audio time base: %1").arg(rationalText(stream.time_base));
  return lines.join('\n');
}
QString assetMetadataText(const pulsar::application::MediaAsset& asset) {
  QStringList lines;
  lines << QStringLiteral("Nombre: %1").arg(QString::fromStdString(asset.source_locator().filename().string()));
  if (asset.has_video()) {
    const auto& video = asset.metadata();
    lines << QStringLiteral("Codec: %1").arg(QString::fromStdString(video.codec_name))
          << QStringLiteral("Resolución: %1 × %2").arg(video.width).arg(video.height)
          << QStringLiteral("Time base: %1").arg(rationalText(video.time_base));
  }
  if (asset.audio().has_value()) {
    const auto& audio = *asset.audio();
    lines << QStringLiteral("Audio stream: #%1").arg(audio.stream_index)
          << QStringLiteral("Audio codec: disponible")
          << QStringLiteral("Audio sample rate: %1 Hz").arg(audio.sample_rate)
          << QStringLiteral("Audio channels: %1").arg(audio.channel_count)
          << QStringLiteral("Audio time base: %1").arg(rationalText(audio.time_base));
  }
  return lines.join('\n');
}
}

DesktopController::DesktopController(PreviewImageProvider& image_provider,
                                     WaveformSourceCreator waveform_source_creator,
                                     QObject* parent)
    : QObject{parent}, image_provider_{image_provider},
      visual_mailbox_{std::make_shared<pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox>(8)},
      waveform_source_creator_{std::move(waveform_source_creator)} {
  loadSoundLibrary();
  waveform_cache_directory_ =
      std::filesystem::path{QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                                .toStdString()} /
      "waveforms";
  active_session_ = makeEditorSession();
  publishTimeline();
  tick_timer_.setInterval(16); tick_timer_.setTimerType(Qt::PreciseTimer);
  connect(&tick_timer_, &QTimer::timeout, this, &DesktopController::updateTick); tick_timer_.start();
  waveform_timer_.setInterval(100);
  connect(&waveform_timer_, &QTimer::timeout, this, &DesktopController::refreshWaveformItems);
  waveform_timer_.start();
  error_timer_.setSingleShot(true);
  error_timer_.setInterval(7000);
  connect(&error_timer_, &QTimer::timeout, this, &DesktopController::clearError);
  worker_ = std::thread{&DesktopController::workerLoop, this};
}
QString DesktopController::appVersion() const {
  const auto version = pulsar::domain::product_version();
  return QStringLiteral("PULSAR ") + QString::fromUtf8(version.data(), static_cast<qsizetype>(version.size()));
}

void DesktopController::loadSoundLibrary() {
  const auto assets = pulsar::project_file::assets_directory();
  if (assets.empty()) return;  // pack not shipped: the Sonidos tab explains it
  const auto manifest = assets / "sounds" / "index.json";
  QFile file(QString::fromStdString(manifest.string()));
  if (!file.open(QIODevice::ReadOnly)) return;
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) return;
  for (const auto& value : document.object().value("sounds").toArray()) {
    const auto object = value.toObject();
    const auto id = object.value("id").toString().toStdString();
    const auto duration = object.value("duration_frames").toInteger();
    if (id.empty() || duration <= 0 || builtin_sounds_.contains(id)) continue;
    BuiltinSound sound{id, object.value("name").toString().toStdString(),
                       object.value("category").toString().toStdString(), duration};
    builtin_sounds_.emplace(id, sound);
    QVariantMap row;
    row.insert("id", QString::fromStdString(sound.id));
    row.insert("name", QString::fromStdString(sound.name));
    row.insert("category", QString::fromStdString(sound.category));
    sound_library_.push_back(row);
  }
  emit soundLibraryChanged();
}
DesktopController::~DesktopController() {
  tick_timer_.stop();
  waveform_timer_.stop();
  export_cancel_requested_.store(true, std::memory_order_release);
  if (export_thread_.joinable()) export_thread_.join();
  waveform_items_.clear();
  waveform_item_stamps_.clear();
  { std::lock_guard lock{mutex_}; stop_worker_ = true; pending_command_.reset(); }
  wake_.notify_one(); if (worker_.joinable()) worker_.join();
  // QAudioSink can deliver a final queued callback while it is being
  // destroyed.  Keep the controller's QML-facing properties alive until the
  // audio session has completed teardown.
  active_session_.reset();
}
void DesktopController::openPath(const QString& path) { const auto trimmed = path.trimmed(); if (!trimmed.isEmpty()) postOpen(std::filesystem::path{trimmed.toStdString()}, ++import_request_generation_); }
void DesktopController::openUrl(const QUrl& url) { if (!url.isLocalFile()) { setError("Solo se admiten archivos locales"); return; } openPath(url.toLocalFile()); }
void DesktopController::importRequested() { emit focusImportPathRequested(); }
void DesktopController::togglePlayback() { playing_ ? pause() : play(); }
void DesktopController::play() { if (!has_media_ || busy_ || !active_session_) return; active_session_->play(); playing_ = true; emit playbackChanged(); setStatus("Reproduciendo"); }
void DesktopController::pause() { if (!active_session_) return; active_session_->pause(); playing_ = false; emit playbackChanged(); setStatus("Pausado"); }
void DesktopController::seekNormalized(double value) {
  if (!active_session_ || !std::isfinite(value) || active_session_->timeline_duration() <= MediaTime::zero()) return;
  const auto target = active_session_->time_at(std::clamp(value, 0.0, 1.0), 1.0); if (!target) return;
  ++seek_count_; active_session_->seek(*target); clearProgram(has_video_media_ ? "loading" : "empty"); updatePositionProperties();
}
void DesktopController::seekToStart() { seekNormalized(0.0); }
void DesktopController::seekToEnd() { seekNormalized(1.0); }
void DesktopController::closeMedia() {
  ++import_request_generation_;
  pause();
  active_session_ = makeEditorSession();
  const bool had_active_project = !active_project_file_.empty();
  active_project_file_.clear();
  if (had_active_project) emit activeProjectPathChanged();
  waveform_clip_identities_.clear();
  waveform_requests_.clear();
  waveform_item_stamps_.clear();
  ++editor_session_epoch_; selected_asset_id_.clear();
  visual_mailbox_->clear();
  has_media_ = false; media_name_.clear(); metadata_.clear(); duration_text_ = "--:--:--.---";
  progress_ = 0.0; position_text_ = "00:00:00.000";
  clearProgram("empty"); publishTimeline();
  setBusy(false); setStatus("Vacío"); emit mediaChanged(); emit playbackChanged(); emit positionChanged();
}

void DesktopController::publishTimeline() {
  const bool old_has_media = has_media_;
  const QString old_duration = duration_text_;
  if (!active_session_) { selected_clip_key_.clear(); emit timelineChanged(); publishProjectDirty(); return; }
  refreshAssetsIfChanged();
  recomputeTimelineViewport();
  selected_clip_key_ = active_session_->selected_clip() ? QString::number(active_session_->selected_clip()->value()) : QString{};
  timeline_model_.refresh(*active_session_, *timeline_view_start_, *timeline_view_duration_);
  timeline_tracks_.clear();
  for (const auto& track : active_session_->track_views()) {
    QVariantMap value;
    value.insert("id", static_cast<qulonglong>(track.id.value()));
    value.insert("index", static_cast<int>(track.index));
    value.insert("kind", track.kind == pulsar::domain::TrackKind::Audio ? "audio" : "video");
    value.insert("label", track.kind == pulsar::domain::TrackKind::Audio
                              ? QStringLiteral("AUDIO %1").arg(static_cast<int>(track.index))
                              : QStringLiteral("VIDEO 1"));
    timeline_tracks_.push_back(value);
  }
  const auto duration = active_session_->timeline_duration();
  has_media_ = duration > MediaTime::zero();
  has_video_media_ = false;
  if (const auto* video_track = active_session_->document().timeline().find_track(
          active_session_->document().video_track_id()); video_track != nullptr) {
    has_video_media_ = !video_track->clips().empty();
  }
  timeline_duration_text_ = formatTime(duration);
  duration_text_ = formatTime(duration);
  rebuildRulerTicks();
  const auto playhead = active_session_->playback_position();
  timeline_playhead_progress_ = playhead >= *timeline_view_start_ &&
                                       playhead <= *timeline_view_start_ + *timeline_view_duration_
                                   ? timelinePixelAt(playhead, 1.0) : -1.0;
  emit timelineChanged();
  emit editActionStateChanged();
  publishProjectDirty();
  if (old_has_media != has_media_ || old_duration != duration_text_) emit mediaChanged();
  refreshWaveformItems();
}

void DesktopController::recomputeTimelineViewport() {
  const auto total = active_session_ ? active_session_->timeline_duration() : MediaTime::zero();
  if (total <= MediaTime::zero()) {
    timeline_view_start_ = MediaTime::zero();
    timeline_view_duration_ = MediaTime::zero();
    timeline_scroll_ = 0.0;
    return;
  }
  if (timeline_zoom_ <= 1.0) {
    timeline_view_start_ = MediaTime::zero();
    timeline_view_duration_ = total;
    timeline_scroll_ = 0.0;
    return;
  }
  const auto total_seconds = total.seconds();
  const long double total_value = static_cast<long double>(total_seconds.numerator()) /
                                  static_cast<long double>(total_seconds.denominator());
  const long double visible_seconds = total_value / static_cast<long double>(timeline_zoom_);
  const long double visible_microseconds = std::max(1.0L, std::round(visible_seconds * 1'000'000.0L));
  if (visible_microseconds > static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
    timeline_view_duration_ = total;
  } else {
    timeline_view_duration_ = MediaTime{static_cast<std::int64_t>(visible_microseconds),
                                        Rational{1, 1'000'000}};
  }
  if (*timeline_view_duration_ > total) timeline_view_duration_ = total;
  const auto visible_value = secondsAsDouble(*timeline_view_duration_);
  const long double maximum_start = std::max(0.0L, total_value - visible_value);
  const long double start_seconds = maximum_start * static_cast<long double>(timeline_scroll_);
  const long double bounded_start_microseconds = std::clamp(
      start_seconds * 1'000'000.0L, 0.0L,
      static_cast<long double>(std::numeric_limits<std::int64_t>::max()));
  const auto start_microseconds = static_cast<std::int64_t>(
      std::floor(bounded_start_microseconds + 0.5L));
  timeline_view_start_ = MediaTime{start_microseconds, Rational{1, 1'000'000}};
}

std::optional<MediaTime> DesktopController::timelineTimeAt(double pixel, double width) const {
  if (!timeline_view_start_.has_value() || !timeline_view_duration_.has_value())
    return std::nullopt;
  return pulsar::desktop::TimelineViewport{*timeline_view_start_, *timeline_view_duration_}
      .time_at(pixel, width);
}

double DesktopController::timelinePixelAt(MediaTime time, double width) const {
  if (!timeline_view_start_.has_value() || !timeline_view_duration_.has_value()) return 0.0;
  return pulsar::desktop::TimelineViewport{*timeline_view_start_, *timeline_view_duration_}
      .pixel_at(time, width);
}

// Ruler: "nice" steps (1/2/5 x 10^k) so labels land on round times at any zoom,
// with minor subdivisions. Rebuilt only when the viewport/program changes.
void DesktopController::rebuildRulerTicks() {
  timeline_ruler_ticks_.clear();
  if (!timeline_view_start_ || !timeline_view_duration_ || *timeline_view_duration_ <= MediaTime::zero()) return;
  const double view_seconds = secondsAsDouble(*timeline_view_duration_);
  const double start_seconds = secondsAsDouble(*timeline_view_start_);
  if (!(view_seconds > 0.0)) return;
  // Aim for ~7 major labels across the view.
  const double raw = view_seconds / 7.0;
  const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
  const double mantissa = raw / magnitude;
  const double nice = mantissa <= 1.0 ? 1.0 : (mantissa <= 2.0 ? 2.0 : (mantissa <= 5.0 ? 5.0 : 10.0));
  const double major_step = std::max(nice * magnitude, 1e-6);
  const int subdivisions = nice == 2.0 ? 4 : 5;
  const double minor_step = major_step / subdivisions;
  const auto first_index = static_cast<long long>(std::ceil(start_seconds / minor_step - 1e-9));
  const auto last_index = static_cast<long long>(std::floor((start_seconds + view_seconds) / minor_step + 1e-9));
  if (last_index - first_index > 400) return;  // pathological zoom: draw nothing rather than flood
  for (long long index = first_index; index <= last_index; ++index) {
    const bool major = index % subdivisions == 0;
    const double seconds = static_cast<double>(index) * minor_step;
    const double progress = (seconds - start_seconds) / view_seconds;
    if (progress < -1e-9 || progress > 1.0 + 1e-9) continue;
    QVariantMap tick;
    tick.insert("progress", std::clamp(progress, 0.0, 1.0));
    tick.insert("major", major);
    tick.insert("label", major ? rulerLabel(seconds, major_step) : QString{});
    timeline_ruler_ticks_.push_back(tick);
  }
}

QString DesktopController::rulerLabel(double seconds, double step) {
  const auto total_ms = static_cast<long long>(std::llround(std::max(0.0, seconds) * 1000.0));
  const long long hours = total_ms / 3'600'000;
  const long long minutes = (total_ms / 60'000) % 60;
  const long long secs = (total_ms / 1000) % 60;
  const long long millis = total_ms % 1000;
  QString text = hours > 0 ? QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0'))
                           : QStringLiteral("%1:%2").arg(minutes).arg(secs, 2, 10, QLatin1Char('0'));
  if (step < 1.0) {  // sub-second steps need decimals: 0.5 -> .5, 0.05 -> .05, 0.001 -> .001
    const int digits = step >= 0.1 ? 1 : (step >= 0.01 ? 2 : 3);
    text += QStringLiteral(".") + QStringLiteral("%1").arg(millis, 3, 10, QLatin1Char('0')).left(digits);
  }
  return text;
}

// Snap distance is fixed on screen (pixels), whatever the zoom: converted to
// time with the current viewport only to decide whether an edge is "near".
std::optional<MediaTime> DesktopController::snapToleranceAt(double width) const {
  constexpr double kSnapPixels = 8.0;
  const auto far = timelineTimeAt(kSnapPixels, width);
  const auto origin = timelineTimeAt(0.0, width);
  if (!far || !origin) return std::nullopt;
  return *far - *origin;
}

void DesktopController::fitTimeline() { timeline_scroll_ = 0.0; setTimelineZoom(1.0); }

QVariantMap DesktopController::previewTimelineMove(const QString& key, double press, double current, double width) {
  QVariantMap result{{"offset", 0.0}, {"guide", -1.0}};
  if (!active_session_ || !timeline_view_start_) return result;
  bool ok = false;
  const auto value = key.toULongLong(&ok);
  const auto press_time = timelineTimeAt(press, width);
  const auto current_time = timelineTimeAt(current, width);
  const auto tolerance = snapToleranceAt(width);
  if (!ok || !value || !press_time || !current_time || !tolerance) return result;
  try {
    const pulsar::domain::ClipId clip{value};
    const auto* original = active_session_->document().timeline().find_clip(clip);
    if (original == nullptr) return result;
    const auto resolution = active_session_->resolve_move(clip, *press_time, *current_time, *tolerance);
    const pulsar::desktop::TimelineViewport viewport{*timeline_view_start_, *timeline_view_duration_};
    result["offset"] = viewport.pixel_at_unclamped(resolution.new_start, width) -
                       viewport.pixel_at_unclamped(original->timeline_start(), width);
    if (resolution.snapped) result["guide"] = viewport.pixel_at_unclamped(resolution.guide, width);
  } catch (...) {}
  return result;
}

QVariantMap DesktopController::previewTimelineTrim(const QString& key, bool, double pixel, double width) {
  QVariantMap result{{"px", pixel}, {"guide", -1.0}};
  if (!active_session_ || !timeline_view_start_) return result;
  bool ok = false;
  const auto value = key.toULongLong(&ok);
  const auto target = timelineTimeAt(pixel, width);
  const auto tolerance = snapToleranceAt(width);
  if (!ok || !value || !target || !tolerance) return result;
  try {
    const auto resolution = active_session_->resolve_trim_edge(pulsar::domain::ClipId{value}, *target, *tolerance);
    if (resolution.snapped) {
      const pulsar::desktop::TimelineViewport viewport{*timeline_view_start_, *timeline_view_duration_};
      result["px"] = result["guide"] = viewport.pixel_at_unclamped(resolution.time, width);
    }
  } catch (...) {}
  return result;
}

void DesktopController::setTimelineZoom(double zoom) {
  if (!std::isfinite(zoom)) return;
  const auto value = std::clamp(zoom, 1.0, 10'000'000.0);
  if (std::abs(value - timeline_zoom_) < 1e-9) return;
  timeline_zoom_ = value;
  publishTimeline();
  emit timelineViewChanged();
}

void DesktopController::setTimelineScroll(double scroll) {
  if (!std::isfinite(scroll)) return;
  const auto value = std::clamp(scroll, 0.0, 1.0);
  if (std::abs(value - timeline_scroll_) < 1e-9) return;
  timeline_scroll_ = value;
  publishTimeline();
  emit timelineViewChanged();
}

void DesktopController::setWaveformDensity(double density) {
  if (!std::isfinite(density)) return;
  const auto value = std::clamp(density, 0.5, 2.0);
  if (std::abs(value - waveform_density_) < 1e-9) return;
  waveform_density_ = value;
  emit waveformPolicyChanged();
  refreshWaveformItems();
}

void DesktopController::attachWaveformItem(const QString& clip_key, QObject* object) {
  auto* item = qobject_cast<WaveformQuickItem*>(object);
  if (item == nullptr) return;
  item->setClipKey(clip_key);
  const auto found = std::find_if(waveform_items_.begin(), waveform_items_.end(),
                                  [item](const auto& candidate) {
                                    return candidate.data() == item;
                                  });
  if (found == waveform_items_.end()) {
    waveform_items_.push_back(QPointer<QObject>{item});
    connect(item, &WaveformQuickItem::inputChanged, this,
            [this, item] { refreshWaveformItem(item); });
  }
  refreshWaveformItem(item);
}

void DesktopController::detachWaveformItem(QObject* object) {
  auto* item = qobject_cast<WaveformQuickItem*>(object);
  if (item == nullptr) return;
  waveform_item_stamps_.erase(item);
  waveform_clip_identities_.erase(item->clipKey().toStdString());
  waveform_items_.erase(std::remove_if(waveform_items_.begin(), waveform_items_.end(),
                                      [item](const auto& candidate) {
                                        return candidate.isNull() || candidate.data() == item;
                                      }),
                           waveform_items_.end());
}

void DesktopController::refreshWaveformItems() {
  for (auto& object : waveform_items_) {
    auto* item = qobject_cast<WaveformQuickItem*>(object.data());
    if (item != nullptr) refreshWaveformItem(item);
  }
  waveform_items_.erase(std::remove_if(waveform_items_.begin(), waveform_items_.end(),
                                      [](const auto& item) { return item.isNull(); }),
                           waveform_items_.end());
}

void DesktopController::refreshWaveformItem(WaveformQuickItem* item) {
  if (item == nullptr || !item->isVisible() || !active_session_ ||
      item->width() < 10.0 || item->height() <= 0.0) return;
  bool parsed = false;
  const auto clip_value = item->clipKey().toULongLong(&parsed);
  if (!parsed || clip_value == 0) return;
  const pulsar::domain::ClipId clip_id{clip_value};
  const auto resolved = pulsar::application::resolve_clip_by_id(
      active_session_->document(), clip_id);
  const auto empty_geometry = [&] {
    auto geometry = std::make_shared<pulsar::application::WaveformGeometry>();
    geometry->width = static_cast<std::uint32_t>(std::clamp(std::ceil(item->width()), 1.0, 16384.0));
    return geometry;
  };
  if (!resolved.has_value() || !resolved->audio.has_value()) {
    auto& stamp = waveform_item_stamps_[item];
    if (stamp.initialized && stamp.clip_id == clip_id &&
        stamp.status == pulsar::application::WaveformCacheStatus::Failed) return;
    item->setPreparedGeometry(empty_geometry(), WaveformQuickItem::Failed, false);
    stamp.clip_id = clip_id;
    stamp.status = pulsar::application::WaveformCacheStatus::Failed;
    stamp.width = static_cast<std::uint32_t>(std::ceil(item->width()));
    stamp.height = static_cast<std::uint32_t>(std::ceil(item->height()));
    stamp.initialized = true;
    return;
  }

  const auto& binding = *resolved->audio;
  const auto& audio = binding.metadata;
  std::filesystem::path runtime_path;
  try {
    runtime_path = desktop_runtime_media_path(active_project_file_, binding.source_locator);
  } catch (...) {
    item->setPreparedGeometry(empty_geometry(), WaveformQuickItem::Failed, false);
    return;
  }
  const std::string clip_key = item->clipKey().toStdString();
  std::string identity;
  std::shared_ptr<pulsar::application::WaveformCacheRequest> request;
  if (const auto mapped = waveform_clip_identities_.find(clip_key);
      mapped != waveform_clip_identities_.end()) {
    identity = mapped->second;
    if (const auto found = waveform_requests_.find(identity); found != waveform_requests_.end())
      request = found->second;
  }
  if (!request) {
    try {
      const auto key = pulsar::application::make_waveform_cache_key(
          runtime_path, audio.stream_index, audio.sample_rate, audio.channel_count,
          audio.time_base, std::nullopt);
      identity = key.stable_text();
      if (const auto found = waveform_requests_.find(identity);
          found != waveform_requests_.end()) {
        request = found->second;
      } else {
        const auto source_path = runtime_path;
        const int stream_index = audio.stream_index;
        const auto source_creator = waveform_source_creator_;
        if (!source_creator) throw std::logic_error{"waveform source adapter is unavailable"};
        request = waveform_cache_manager_.request(
            waveform_cache_directory_, key,
            [source_creator, source_path, stream_index] {
              return source_creator(source_path, stream_index);
            });
        waveform_requests_.emplace(identity, request);
      }
      waveform_clip_identities_[clip_key] = identity;
    } catch (...) {
      item->setPreparedGeometry(empty_geometry(), WaveformQuickItem::Failed, false);
      return;
    }
  }

  const auto progress = request->progress();
  const auto resolved_key = request->resolved_key();
  pulsar::domain::TimeRange source_window = resolved->source_range;
  try {
    if (resolved_key.has_value())
      source_window = pulsar::application::audio_source_range(*resolved,
                                                              resolved_key->stream_origin);
  } catch (...) {
    item->setPreparedGeometry(empty_geometry(), WaveformQuickItem::Failed, false);
    return;
  }
  const auto width = static_cast<std::uint32_t>(
      std::clamp(std::ceil(item->width()), 1.0, 16384.0));
  const auto height = static_cast<std::uint32_t>(
      std::clamp(std::ceil(item->height()), 1.0, 16384.0));

  auto& stamp = waveform_item_stamps_[item];
  const bool unchanged = stamp.initialized && stamp.clip_id == resolved->clip_id &&
      stamp.media_id == resolved->media_id &&
      stamp.source_start == source_window.start() && stamp.source_end == source_window.end() &&
      stamp.stream_origin == (resolved_key.has_value() ? resolved_key->stream_origin
                                                       : std::optional<MediaTime>{}) &&
      stamp.width == width && stamp.height == height && stamp.density == item->density() &&
      stamp.status == progress.status && stamp.generated_frames == progress.generated_frames;
  if (unchanged) return;

  const auto query_start = std::chrono::steady_clock::now();
  std::optional<pulsar::application::WaveformSlice> slice;
  if (resolved_key.has_value()) {
    try {
      slice = request->query_partial({source_window.start(), source_window.end(), width,
                                      width < 20U ? std::min(0.5, item->density())
                                                  : item->density()});
      ++waveform_query_count_;
    } catch (...) {
      item->setPreparedGeometry(empty_geometry(), WaveformQuickItem::Failed, false);
      return;
    }
  }
  if (!slice.has_value()) {
    pulsar::application::WaveformSlice pending;
    pending.requested_start = source_window.start();
    pending.requested_end = source_window.end();
    pending.available_start = resolved_key.has_value() && resolved_key->stream_origin
                                  ? *resolved_key->stream_origin
                                  : source_window.start();
    pending.available_end = pending.available_start;
    pending.bucket_sample_span = 256;
    pending.channel_count = audio.channel_count;
    pending.completeness = pulsar::application::WaveformCompleteness::Missing;
    slice = std::move(pending);
  }

  std::vector<pulsar::application::WaveformSlice> boundary_refinements;
  if (slice->bucket_count > 0 && slice->bucket_sample_span > 256 &&
      slice->level > 0 && resolved_key.has_value()) {
    const auto span = slice->bucket_sample_span;
    if (span <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      const auto coarse_bucket_time = pulsar::domain::MediaTime{
          static_cast<std::int64_t>(span), pulsar::domain::Rational{1, audio.sample_rate}};
      const auto left_end = std::min(source_window.end(),
                                     source_window.start() + coarse_bucket_time);
      const auto right_start = std::max(source_window.start(),
                                        source_window.end() - coarse_bucket_time);
      std::vector<pulsar::domain::TimeRange> edge_windows;
      if (left_end >= right_start) {
        edge_windows.emplace_back(source_window.start(), source_window.end());
      } else {
        edge_windows.emplace_back(source_window.start(), left_end);
        edge_windows.emplace_back(right_start, source_window.end());
      }
      const auto detail_width = static_cast<std::uint32_t>(std::clamp(
          std::ceil(static_cast<double>(span) / 256.0) + 1.0, 1.0, 16384.0));
      for (const auto& edge : edge_windows) {
        try {
          auto refined = request->query_partial(
              {edge.start(), edge.end(), detail_width, 2.0});
          ++waveform_query_count_;
          if (refined.has_value() && refined->level < slice->level)
            boundary_refinements.push_back(std::move(*refined));
        } catch (...) {
          // Keep the valid base LOD. A refinement failure cannot poison the
          // clip or document; the L0 geometry core handles unresolved edges.
        }
      }
    }
  }
  waveform_query_durations_.push(std::chrono::steady_clock::now() - query_start);

  const auto prepare_start = std::chrono::steady_clock::now();
  const bool combine = audio.channel_count > 2 ||
      (audio.channel_count == 2 && item->height() < 64.0);
  const pulsar::application::WaveformRenderPolicy policy{
      width < 20U ? std::min(0.5, item->density()) : item->density(),
      combine, width, item->height()};
  std::shared_ptr<const pulsar::application::WaveformGeometry> geometry;
  bool preparation_failed = false;
  try {
    geometry = std::make_shared<pulsar::application::WaveformGeometry>(
        pulsar::application::prepare_waveform_geometry(*slice, boundary_refinements,
                                                       audio.sample_rate, policy));
  } catch (...) {
    geometry = empty_geometry();
    preparation_failed = true;
  }
  waveform_geometry_durations_.push(std::chrono::steady_clock::now() - prepare_start);
  waveform_segment_count_ += geometry->segments.size();

  WaveformQuickItem::State state = WaveformQuickItem::Missing;
  switch (progress.status) {
  case pulsar::application::WaveformCacheStatus::Missing:
  case pulsar::application::WaveformCacheStatus::Invalid: state = WaveformQuickItem::Missing; break;
  case pulsar::application::WaveformCacheStatus::Generating: state = WaveformQuickItem::Generating; break;
  case pulsar::application::WaveformCacheStatus::Partial: state = WaveformQuickItem::Partial; break;
  case pulsar::application::WaveformCacheStatus::Ready: state = WaveformQuickItem::Ready; break;
  case pulsar::application::WaveformCacheStatus::Failed: state = WaveformQuickItem::Failed; break;
  }
  if (preparation_failed) state = WaveformQuickItem::Failed;
  const bool available = !geometry->segments.empty();
  item->setPreparedGeometry(std::move(geometry), state, available);
  stamp.clip_id = resolved->clip_id;
  stamp.media_id = resolved->media_id;
  stamp.source_start = source_window.start();
  stamp.source_end = source_window.end();
  stamp.stream_origin = resolved_key.has_value() ? resolved_key->stream_origin
                                                 : std::optional<MediaTime>{};
  stamp.width = width;
  stamp.height = height;
  stamp.density = item->density();
  stamp.status = progress.status;
  stamp.generated_frames = progress.generated_frames;
  stamp.initialized = true;
}
void DesktopController::installTimelineAsset(const pulsar::application::MediaProbeResult& result) {
  if (!active_session_ || (result.video_streams.empty() && result.audio_streams.empty())) return;
  try {
    StaticProbe probe{result};
    const auto media = active_session_->processor().import_asset(result.path, probe);
    selected_asset_id_ = QString::number(media.value());
    if (!result.video_streams.empty()) {
      const auto& stream = result.video_streams.front();
      const auto duration = stream.duration ? stream.duration : result.duration;
      const auto* video_track = active_session_->document().timeline().find_track(
          active_session_->document().video_track_id());
      const bool video_track_empty = video_track == nullptr || video_track->clips().empty();
      // Audio tracks may keep the global timeline non-empty while VIDEO 1 is
      // empty. Auto-place imported video only according to VIDEO 1 occupancy.
      if (duration && *duration > MediaTime::zero() && video_track_empty) {
        (void)active_session_->processor().add_clip(
            media, pulsar::domain::TimeRange{MediaTime::zero(), *duration}, MediaTime::zero());
      }
      media_name_ = QString::fromStdString(result.path.filename().string());
      metadata_ = metadataText(result, stream);
      setTechnicalStatus("Ready · Program");
    } else {
      const auto& stream = result.audio_streams.front();
      const auto duration = stream.duration ? stream.duration : result.duration;
      if (!duration || *duration <= MediaTime::zero()) {
        throw std::invalid_argument{"audio-only media has no usable duration"};
      }
      const auto insert_at = active_session_->timeline_duration() > MediaTime::zero()
                                 ? active_session_->playback_position()
                                 : MediaTime::zero();
      const pulsar::domain::TimeRange inserted_range{insert_at, insert_at + *duration};
      std::optional<pulsar::domain::TrackId> audio_track;
      for (const auto& track : active_session_->document().timeline().tracks()) {
        if (track.kind() == pulsar::domain::TrackKind::Audio &&
            std::none_of(track.clips().begin(), track.clips().end(),
                         [&inserted_range](const auto& clip) {
                           return inserted_range.overlaps(clip.timeline_range());
                         })) {
          audio_track = track.id();
          break;
        }
      }
      if (!audio_track.has_value()) {
        audio_track = active_session_->processor().add_track(pulsar::domain::TrackKind::Audio);
      }
      (void)active_session_->processor().add_clip(
          *audio_track, media, pulsar::domain::TimeRange{MediaTime::zero(), *duration}, insert_at);
      media_name_ = QString::fromStdString(result.path.filename().string());
      metadata_ = audioMetadataText(result, stream);
      setTechnicalStatus("Audio listo");
    }
    active_session_->synchronize_document_state();
    publishTimeline();
    has_media_ = active_session_->timeline_duration() > MediaTime::zero();
    duration_text_ = formatTime(active_session_->timeline_duration());
    setBusy(false);
    setStatus("Pausado");
    emit mediaChanged();
  } catch (const std::exception& error) {
    logCause("import failed", error);
    setBusy(false);
    setError(importMessage(result.path));
    setStatus("Error");
  }
}
void DesktopController::refreshAssetsIfChanged() {
  if (!active_session_) return;
  const auto& document = active_session_->document();
  if (asset_model_epoch_ == editor_session_epoch_ &&
      asset_model_revision_ == document.asset_revision()) {
    return;
  }
  asset_model_epoch_ = editor_session_epoch_;
  asset_model_revision_ = document.asset_revision();
  asset_model_.refresh(document);
  if (!asset_model_.contains(selected_asset_id_)) {
    // Selection vanished (undo of an import, project switch): fall back to the
    // most recent listed asset so the Inspector never describes a stale one.
    selected_asset_id_.clear();
    for (const auto& asset : document.asset_list()) {
      if (asset.source_locator().string().rfind("pulsar-sound:", 0) == 0) continue;
      selected_asset_id_ = QString::number(asset.id().value());
    }
    if (selected_asset_id_.isEmpty()) {
      media_name_.clear();
      metadata_.clear();
    } else {
      applyAssetSelection(selected_asset_id_);
    }
  }
  emit assetsChanged();
}

void DesktopController::applyAssetSelection(const QString& media_id) {
  bool ok = false;
  const auto value = media_id.toULongLong(&ok);
  if (!ok || !active_session_) return;
  const auto asset = active_session_->document().find_asset(pulsar::domain::MediaId{value});
  if (!asset.has_value()) return;
  selected_asset_id_ = media_id;
  media_name_ = QString::fromStdString(asset->source_locator().filename().string());
  metadata_ = assetMetadataText(*asset);
  emit mediaChanged();
}

void DesktopController::selectAsset(const QString& media_id) {
  if (!asset_model_.contains(media_id) || media_id == selected_asset_id_) return;
  applyAssetSelection(media_id);
  emit assetsChanged();
}

void DesktopController::insertAssetAtPlayhead(const QString& media_id) { ingestVideoAsset(media_id, false); }
void DesktopController::appendAssetToVideoTrack(const QString& media_id) { ingestVideoAsset(media_id, true); }

void DesktopController::ingestVideoAsset(const QString& media_id, bool append) {
  if (!active_session_) return;
  bool ok = false;
  const auto value = media_id.toULongLong(&ok);
  if (!ok || value == 0) {
    setError(QStringLiteral("Medio no encontrado."));
    return;
  }
  const pulsar::domain::MediaId media{value};
  try {
    if (append) (void)active_session_->append_video_asset(media);
    else (void)active_session_->insert_video_asset_at_playhead(media);
  } catch (const pulsar::desktop::VideoIngestException& error) {
    // Nothing was changed: no history entry, no dirty change, no republish.
    switch (error.code()) {
      case pulsar::desktop::VideoIngestError::NoSpace:
        setError(QStringLiteral("No hay espacio suficiente en la pista de video."));
        break;
      case pulsar::desktop::VideoIngestError::UnavailableAsset:
        setError(QStringLiteral("El medio no está disponible."));
        break;
      case pulsar::desktop::VideoIngestError::NotVideoAsset:
        setError(QStringLiteral("Este medio no tiene video; no se puede colocar en la pista de video."));
        break;
      case pulsar::desktop::VideoIngestError::InvalidDuration:
        setError(QStringLiteral("El medio no tiene una duración utilizable."));
        break;
      case pulsar::desktop::VideoIngestError::UnknownAsset:
        setError(QStringLiteral("Medio no encontrado."));
        break;
    }
    return;
  } catch (const std::exception&) {
    setError(QStringLiteral("No se pudo agregar el clip."));
    return;
  }
  clearError();
  applyAssetSelection(media_id);
  clearProgram("loading");
  publishTimeline();
}

void DesktopController::separateSelectedAudio() {
  if (!active_session_ || !active_session_->selected_clip()) {
    setError(QStringLiteral("Seleccioná un clip de video."));
    return;
  }
  try {
    (void)active_session_->separate_audio(*active_session_->selected_clip());
  } catch (const pulsar::desktop::SeparateAudioException& error) {
    switch (error.code()) {
      case pulsar::desktop::SeparateAudioError::AlreadySilenced:
        setError(QStringLiteral("El audio de este clip ya está silenciado o separado.")); break;
      case pulsar::desktop::SeparateAudioError::NoEmbeddedAudio:
        setError(QStringLiteral("Este clip no tiene audio.")); break;
      case pulsar::desktop::SeparateAudioError::AudioShorterThanClip:
        setError(QStringLiteral("El audio del medio es más corto que el clip; no se puede separar.")); break;
      case pulsar::desktop::SeparateAudioError::UnavailableAsset:
        setError(QStringLiteral("El medio no está disponible.")); break;
      case pulsar::desktop::SeparateAudioError::NotAVideoTrackClip:
      case pulsar::desktop::SeparateAudioError::UnknownClip:
        setError(QStringLiteral("Solo se puede separar el audio de un clip de video.")); break;
    }
    return;
  } catch (const std::exception&) {
    setError(QStringLiteral("No se pudo separar el audio."));
    return;
  }
  clearError();
  clearProgram("loading");
  publishTimeline();
}

void DesktopController::selectTimelineClip(const QString& key) { if (!active_session_) return; if (key.isEmpty()) active_session_->select_clip(std::nullopt); else { bool ok=false; auto value=key.toULongLong(&ok); if (ok && value) active_session_->select_clip(pulsar::domain::ClipId{value}); } publishTimeline(); }
double DesktopController::selectedClipGainDb() const {
  if (!active_session_ || !active_session_->selected_clip()) return 0.0;
  const auto* clip = active_session_->document().timeline().find_clip(*active_session_->selected_clip());
  return clip ? clip->audio_gain_db() : 0.0;
}
bool DesktopController::selectedClipMuted() const {
  if (!active_session_ || !active_session_->selected_clip()) return false;
  const auto* clip = active_session_->document().timeline().find_clip(*active_session_->selected_clip());
  return clip != nullptr && clip->muted();
}
void DesktopController::seekTimelineAt(double pixel, double width) { if (!active_session_) return; const auto target=timelineTimeAt(pixel,width); if (!target) return; active_session_->seek(*target); clearProgram(has_video_media_ ? "loading" : "empty"); updatePositionProperties(); }
void DesktopController::moveTimelineClip(const QString& key,double press,double current,double width) { if (!active_session_) return; bool ok=false; auto value=key.toULongLong(&ok);if(!ok||!value)return;const auto press_time=timelineTimeAt(press,width);const auto current_time=timelineTimeAt(current,width);if(!press_time||!current_time)return;try{active_session_->move_clip(pulsar::domain::ClipId{value},*press_time,*current_time,snapToleranceAt(width).value_or(MediaTime::zero()));clearError();clearProgram(has_video_media_ ? "loading" : "empty");publishTimeline();}catch(...){setError("El movimiento no es válido.");publishTimeline();} }
void DesktopController::trimTimelineClipStart(const QString& key,double pixel,double width) { if(!active_session_)return;bool ok=false;auto value=key.toULongLong(&ok);if(!ok||!value)return;const auto target=timelineTimeAt(pixel,width);if(!target)return;try{active_session_->trim_clip_start(pulsar::domain::ClipId{value},*target,snapToleranceAt(width).value_or(MediaTime::zero()));clearError();clearProgram(has_video_media_ ? "loading" : "empty");publishTimeline();}catch(...){setError("El recorte no es válido.");publishTimeline();} }
void DesktopController::trimTimelineClipEnd(const QString& key,double pixel,double width) { if(!active_session_)return;bool ok=false;auto value=key.toULongLong(&ok);if(!ok||!value)return;const auto target=timelineTimeAt(pixel,width);if(!target)return;try{active_session_->trim_clip_end(pulsar::domain::ClipId{value},*target,snapToleranceAt(width).value_or(MediaTime::zero()));clearError();clearProgram(has_video_media_ ? "loading" : "empty");publishTimeline();}catch(...){setError("El recorte no es válido.");publishTimeline();} }
void DesktopController::splitSelectedClip(){if(!active_session_)return;try{(void)active_session_->split_selected_at_playhead();clearError();clearProgram(has_video_media_ ? "loading" : "empty");publishTimeline();}catch(...){setError("El punto de corte debe estar dentro del clip.");}}
void DesktopController::deleteSelectedClip(){if(!active_session_)return;try{active_session_->delete_selected();clearError();clearProgram(has_video_media_ ? "loading" : "empty");publishTimeline();}catch(...){} }
void DesktopController::insertBuiltinSound(const QString& sound_id) {
  if (!active_session_) return;
  const auto found = builtin_sounds_.find(sound_id.toStdString());
  if (found == builtin_sounds_.end()) {
    setError(QStringLiteral("Sonido no disponible en el catálogo."));
    return;
  }
  bool sound_pack_missing = false;
  try {
    const std::filesystem::path locator{"pulsar-sound:" + found->second.id};
    const auto runtime = pulsar::desktop::builtin_sound_runtime_path(locator);
    std::error_code error;
    if (!std::filesystem::is_regular_file(runtime, error)) {
      sound_pack_missing = true;
      throw std::runtime_error{"Core Sounds Pack no disponible"};
    }
    const auto media = active_session_->processor().import_asset(
        locator, BuiltinSoundProbe{found->second.duration_frames});
    const auto start = active_session_->playback_position();
    const auto duration = MediaTime{found->second.duration_frames, Rational{1, 48'000}};
    const pulsar::domain::TimeRange range{start, start + duration};
    std::optional<pulsar::domain::TrackId> track_id;
    for (const auto& track : active_session_->document().timeline().tracks()) {
      if (track.kind() != pulsar::domain::TrackKind::Audio) continue;
      const bool free = std::none_of(track.clips().begin(), track.clips().end(),
                                     [&range](const auto& clip) {
                                       return range.overlaps(clip.timeline_range());
                                     });
      if (free) { track_id = track.id(); break; }
    }
    if (!track_id.has_value()) {
      track_id = active_session_->processor().add_track(pulsar::domain::TrackKind::Audio);
    }
    const auto clip = active_session_->processor().add_clip(
        *track_id, media, pulsar::domain::TimeRange{MediaTime::zero(), duration}, start);
    active_session_->select_clip(clip);
    active_session_->synchronize_document_state();
    has_media_ = true;
    duration_text_ = formatTime(active_session_->timeline_duration());
    clearError();
    setStatus(QStringLiteral("Sonido insertado"));
    publishTimeline();
    emit mediaChanged();
  } catch (const std::exception& error) {
    logCause("insert sound failed", error);
    setError(sound_pack_missing ? QStringLiteral("No se encontró el paquete de sonidos de PULSAR. Reinstalá la aplicación.")
                                : QStringLiteral("No se pudo insertar el sonido."));
    setStatus(QStringLiteral("Error"));
  }
}
void DesktopController::setSelectedClipGainDb(double gain_db) {
  if (!active_session_ || !active_session_->selected_clip() || !std::isfinite(gain_db)) return;
  try {
    active_session_->set_clip_audio_gain(*active_session_->selected_clip(), gain_db);
    clearError();
    publishTimeline();
  } catch (const std::exception& error) {
    logCause("edit failed", error);
    setError(QStringLiteral("No se pudo aplicar el cambio."));
  }
}
void DesktopController::toggleSelectedClipMute() {
  if (!active_session_ || !active_session_->selected_clip()) return;
  try {
    active_session_->set_clip_muted(*active_session_->selected_clip(), !selectedClipMuted());
    clearError();
    publishTimeline();
  } catch (const std::exception& error) {
    logCause("edit failed", error);
    setError(QStringLiteral("No se pudo aplicar el cambio."));
  }
}
void DesktopController::undoEdit(){if(active_session_&&active_session_->undo()){clearProgram(has_video_media_ ? "loading" : "empty");publishTimeline();}}
void DesktopController::redoEdit(){if(active_session_&&active_session_->redo()){clearProgram(has_video_media_ ? "loading" : "empty");publishTimeline();}}
bool DesktopController::saveProject(const QString& path) {
  const QString trimmed = path.trimmed().isEmpty() ? activeProjectPath() : path.trimmed();
  if (trimmed.isEmpty() || !active_session_) {
    return false;
  }
  try {
    const auto result = active_session_->save_project(std::filesystem::path{trimmed.toStdString()},
                                                      pulsar::project_file::FileProjectRepository{});
    if (result.status == pulsar::application::ProjectSaveStatus::Durable) {
      setStatus(QStringLiteral("Proyecto guardado"));
    } else {
      setStatus(QStringLiteral("Proyecto guardado (durabilidad no confirmada)"));
    }
    clearError();
    const auto saved_path = std::filesystem::path{trimmed.toStdString()};
    if (active_project_file_ != saved_path) {
      active_project_file_ = saved_path;
      emit activeProjectPathChanged();
    }
    publishProjectDirty();
    return true;
  } catch (const std::exception& error) {
    // Nothing is marked saved: the project stays dirty and open.
    logCause("save failed", error);
    setError(projectSaveMessage());
    return false;
  }
}
bool DesktopController::openProject(const QString& path) {
  const QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }
  const std::filesystem::path project_path{trimmed.toStdString()};
  setBusy(true);
  setStatus(QStringLiteral("Abriendo proyecto…"));
  clearError();
  try {
    auto restore_state = pulsar::project_file::FileProjectRepository{}.load(project_path);
    // Candidate-first: the old session, mailbox, epochs and generations stay
    // untouched until the new session is fully constructed. In particular the
    // candidate gets its own visual mailbox, because activating a worker on
    // the shared mailbox would disturb the old session before commit.
    auto candidate_mailbox =
        std::make_shared<pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox>(8);
#if defined(PULSAR_HAS_QT_AUDIO)
    auto candidate_output = std::make_unique<pulsar::media::qt_audio::QtAudioOutput>(
        pulsar::media::qt_audio::QtAudioOutput::Options{{48000 * 2}, {}});
    auto candidate = std::make_unique<pulsar::desktop::EditorSession>(
        std::move(restore_state), makeWorkerFactory(candidate_mailbox, project_path),
        makeAudioSourceFactory(project_path), std::move(candidate_output));
#else
    auto candidate = std::make_unique<pulsar::desktop::EditorSession>(
        std::move(restore_state), makeWorkerFactory(candidate_mailbox, project_path),
        makeAudioSourceFactory(project_path));
#endif
    ++import_request_generation_;
    active_session_ = std::move(candidate);
    visual_mailbox_ = std::move(candidate_mailbox);
    const bool active_path_changed = active_project_file_ != project_path;
    active_project_file_ = project_path;
    if (active_path_changed) emit activeProjectPathChanged();
    waveform_clip_identities_.clear();
    waveform_requests_.clear();
    waveform_item_stamps_.clear();
    ++editor_session_epoch_; selected_asset_id_.clear();
  } catch (const std::exception& error) {
    // Candidate-first: the current session, mailbox and workers were never touched.
    logCause("open project failed", error);
    setBusy(false);
    setError(projectOpenMessage(error));
    setStatus(QStringLiteral("Error"));
    return false;
  }
  pause();
  has_media_ = false;
  const auto assets = active_session_->document().assets();
  if (!assets.empty()) {
    const auto& asset = assets.front();
    media_name_ = QString::fromStdString(asset.source_locator().filename().string());
    metadata_ = assetMetadataText(asset);
  } else {
    media_name_.clear();
    metadata_.clear();
  }
  duration_text_ = "--:--:--.---";
  progress_ = 0.0;
  position_text_ = "00:00:00.000";
  publishTimeline();
  clearProgram(has_video_media_ ? "loading" : "empty");
  setBusy(false);
  clearError();
  setStatus(QStringLiteral("Proyecto abierto"));
  emit mediaChanged();
  emit playbackChanged();
  emit positionChanged();
  return true;
}

// ---- Save / export destination helpers -----------------------------------------------------
QString DesktopController::suggestedPath(const QString& purpose) const {
  const bool is_export = purpose == QStringLiteral("export");
  QString directory;
  if (!active_project_file_.empty()) {
    directory = QString::fromStdString(active_project_file_.parent_path().string());
  } else {
    directory = QStandardPaths::writableLocation(is_export ? QStandardPaths::MoviesLocation
                                                           : QStandardPaths::DocumentsLocation);
    if (directory.isEmpty() || !QDir{directory}.exists()) directory = QDir::homePath();
  }
  QString stem = active_project_file_.empty() ? QStringLiteral("pulsar") : QString::fromStdString(active_project_file_.stem().string());
  return QDir{directory}.filePath(stem + (is_export ? QStringLiteral(".mp4") : QStringLiteral(".pulsar")));
}

QString DesktopController::normalizeSavePath(const QString& text, const QString& extension) const {
  QString path = text.trimmed();
  if (path.startsWith(QStringLiteral("~/")) || path == QStringLiteral("~")) path = QDir::homePath() + path.mid(1);
  if (!path.isEmpty() && QDir::isRelativePath(path)) {
    const auto suggested = QFileInfo{suggestedPath(extension == QStringLiteral(".mp4") ? QStringLiteral("export") : QStringLiteral("project"))};
    path = suggested.absolutePath() + QLatin1Char('/') + path;
  }
  path = QDir::cleanPath(path);
  if (!path.isEmpty() && !extension.isEmpty() && !path.endsWith(extension, Qt::CaseInsensitive)) path += extension;
  return path;
}

QString DesktopController::validateSavePath(const QString& path) const {
  if (path.trimmed().isEmpty()) return QStringLiteral("Escribí la ruta del archivo.");
  const QFileInfo info{path};
  if (info.isDir()) return QStringLiteral("Esa ruta es una carpeta; escribí también el nombre del archivo.");
  const QFileInfo parent{info.absolutePath()};
  if (!parent.exists() || !parent.isDir()) return QStringLiteral("La carpeta no existe.");
  if (!parent.isWritable()) return QStringLiteral("No tenés permiso para escribir en esa carpeta.");
  if (info.exists() && !info.isWritable()) return QStringLiteral("No tenés permiso para reemplazar ese archivo.");
  return {};
}

bool DesktopController::pathExists(const QString& path) const { return QFileInfo::exists(path); }

void DesktopController::exportProject(const QString& path) {
  const auto trimmed = path.trimmed();
  if (export_running_) return;
  if (!active_session_ || !has_video_media_) {
    setExportStatus(QStringLiteral("No hay vídeo exportable en el proyecto."));
    return;
  }
  if (trimmed.isEmpty()) {
    setExportStatus(QStringLiteral("Selecciona un destino para exportar."));
    return;
  }
  auto destination = std::filesystem::path{trimmed.toStdString()};
  if (destination.extension() != ".mp4" && destination.extension() != ".MP4") {
    setExportStatus(QStringLiteral("El destino debe ser un archivo .mp4."));
    return;
  }

  if (export_thread_.joinable()) export_thread_.join();
  const auto snapshot = active_session_->document().snapshot();
  const auto project_file = active_project_file_;
  const auto generation = ++export_generation_;
  export_cancel_requested_.store(false, std::memory_order_release);
  export_destination_ = QString::fromStdString(destination.string());
  emit exportDestinationChanged();
  export_progress_ = 0.0;
  emit exportProgressChanged();
  export_running_ = true;
  emit exportRunningChanged();
  setExportStatus(QStringLiteral("Exportando…"));
  clearError();
  pause();
  export_thread_ = std::thread{&DesktopController::exportLoop, this, snapshot,
                                std::move(destination), project_file, generation};
}

void DesktopController::cancelExport() {
  if (!export_running_) return;
  export_cancel_requested_.store(true, std::memory_order_release);
  setExportStatus(QStringLiteral("Cancelando…"));
}

void DesktopController::exportLoop(pulsar::application::ProjectSnapshot snapshot,
                                    std::filesystem::path destination,
                                    std::filesystem::path project_file,
                                    std::uint64_t generation) {
  try {
    ResolvingExportVideoSourceFactory video_sources{project_file};
    pulsar::media::ffmpeg::FFmpegOfflineAudioSourceFactory audio_sources(
        [project_file](const std::filesystem::path& logical) {
          return desktop_runtime_media_path(project_file, logical);
        });
    DesktopExportWriterFactory writers;
    pulsar::project_file::FilesystemExportOutputPublisher publisher;
    pulsar::application::exporting::ExportJob job{video_sources, audio_sources, writers, publisher};
    double last_published = -1.0;
    const auto result = job.run(
        {std::move(snapshot), destination, pulsar::application::exporting::ExportProfile::beta_v01()},
        [this] { return export_cancel_requested_.load(std::memory_order_acquire); },
        [this, generation, &last_published](
            const pulsar::application::exporting::ExportProgress& progress) {
          if (progress.fraction != 1.0 && progress.fraction - last_published < 0.01) return;
          last_published = progress.fraction;
          QMetaObject::invokeMethod(
              this,
              [this, generation, value = progress.fraction] {
                if (generation != export_generation_) return;
                export_progress_ = std::clamp(value, 0.0, 1.0);
                emit exportProgressChanged();
              },
              Qt::QueuedConnection);
        });
    QMetaObject::invokeMethod(
        this,
        [this, generation, result] {
          if (generation != export_generation_) return;
          export_running_ = false;
          emit exportRunningChanged();
          export_progress_ = result.progress.fraction;
          emit exportProgressChanged();
          setExportStatus(QStringLiteral("Exportación completada"));
        },
        Qt::QueuedConnection);
  } catch (const pulsar::application::exporting::ExportError& error) {
    if (error.code() != pulsar::application::exporting::ExportErrorCode::Cancelled) {
      std::cerr << "EXPORT ERROR\n"
                << "export_code=" << exportErrorCodeText(error.code())
                << " cause=" << error.what() << '\n';
    }
    QMetaObject::invokeMethod(
        this,
        [this, generation, message = exportErrorMessage(error), cancelled =
                                  error.code() == pulsar::application::exporting::ExportErrorCode::Cancelled] {
          if (generation != export_generation_) return;
          export_running_ = false;
          emit exportRunningChanged();
          setExportStatus(cancelled ? QStringLiteral("Exportación cancelada")
                                    : QStringLiteral("Error de exportación"));
          if (!cancelled) setError(message);
        },
        Qt::QueuedConnection);
  } catch (const std::exception& error) {
    std::cerr << "EXPORT ERROR\n"
              << "export_code=InternalInvariant cause=" << error.what() << '\n';
    QMetaObject::invokeMethod(
        this,
        [this, generation, message = QStringLiteral("Ocurrió un error inesperado durante la exportación.")] {
          if (generation != export_generation_) return;
          export_running_ = false;
          emit exportRunningChanged();
          setExportStatus(QStringLiteral("Error de exportación"));
          setError(message);
        },
        Qt::QueuedConnection);
  }
}

std::unique_ptr<pulsar::desktop::EditorSession> DesktopController::makeEditorSession() {
#if defined(PULSAR_HAS_QT_AUDIO)
  auto output = std::make_unique<pulsar::media::qt_audio::QtAudioOutput>(
      pulsar::media::qt_audio::QtAudioOutput::Options{{48000 * 2}, {}});
  return std::make_unique<pulsar::desktop::EditorSession>(
      makeWorkerFactory(visual_mailbox_, std::filesystem::path{}),
      makeAudioSourceFactory(std::filesystem::path{}), std::move(output));
#else
  return std::make_unique<pulsar::desktop::EditorSession>(
      makeWorkerFactory(visual_mailbox_, std::filesystem::path{}),
      makeAudioSourceFactory(std::filesystem::path{}));
#endif
}

pulsar::application::PlaybackWorkerFactory DesktopController::makeWorkerFactory(
    std::shared_ptr<pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox> mailbox,
    std::filesystem::path project_file) {
  return [this, mailbox, project_file = std::move(project_file)](
             const std::filesystem::path& path, int stream) {
    const auto runtime_path =
        pulsar::desktop::is_builtin_sound_locator(path)
            ? pulsar::desktop::builtin_sound_runtime_path(path)
            : pulsar::desktop::resolve_runtime_media_path(project_file, path);
    const auto start = std::chrono::steady_clock::now();
    auto worker = std::make_unique<FFmpegPlaybackWorker>(runtime_path, stream,
                                                         FFmpegPlaybackWorker::Limits{8}, mailbox);
    worker_ctor_durations_.push(std::chrono::steady_clock::now() - start);
    worker_descriptor_durations_.push(worker->stats().descriptor_latency);
    ++worker_construction_count_;
    return worker;
  };
}

pulsar::application::AudioPlaybackSourceFactory DesktopController::makeAudioSourceFactory(
    std::filesystem::path project_file) {
  return [this, project_file = std::move(project_file)](const std::filesystem::path& path,
                                                        int stream) {
    const auto runtime_path =
        pulsar::desktop::is_builtin_sound_locator(path)
            ? pulsar::desktop::builtin_sound_runtime_path(path)
            : pulsar::desktop::resolve_runtime_media_path(project_file, path);
    return std::make_unique<pulsar::media::ffmpeg::FFmpegAudioPlaybackSource>(runtime_path,
                                                                                stream);
  };
}

void DesktopController::postOpen(std::filesystem::path path, std::uint64_t generation) { setBusy(true); setStatus("Preparando…"); setTechnicalStatus("Probing"); clearError(); enqueue(WorkerCommand{std::move(path),generation}); }
void DesktopController::enqueue(WorkerCommand command){{std::lock_guard lock{mutex_};pending_command_=std::move(command);}wake_.notify_one();}
void DesktopController::workerLoop(){while(true){WorkerCommand command;{std::unique_lock lock{mutex_};wake_.wait(lock,[this]{return stop_worker_||pending_command_.has_value();});if(stop_worker_)return;command=std::move(*pending_command_);pending_command_.reset();}try{FFmpegMediaProbe probe;auto result=probe.probe(command.path);if(result.video_streams.empty()&&result.audio_streams.empty())throw std::runtime_error{"media has no usable video or audio stream"};QMetaObject::invokeMethod(this,[this,generation=command.generation,result=std::move(result)]()mutable{if(generation!=import_request_generation_){++stale_visual_frames_;return;}installTimelineAsset(result);},Qt::QueuedConnection);}catch(const std::exception& error){logCause("import failed",error);QMetaObject::invokeMethod(this,[this,generation=command.generation,message=importMessage(command.path)]{if(generation!=import_request_generation_)return;setBusy(false);setError(message);setStatus("Error");},Qt::QueuedConnection);}}}

void DesktopController::presentTimeline(const pulsar::application::TimelinePresentation& presentation){std::visit([this](const auto& value){using T=std::decay_t<decltype(value)>;if constexpr(std::is_same_v<T,pulsar::application::TimelinePresentSample>){displayLease(value);}else if constexpr(std::is_same_v<T,pulsar::application::TimelineHoldPresentation>){pulsar::application::TimelinePresentSample present{value.clip_id,value.media_id,value.timeline_position,value.sample};displayLease(present);}else if constexpr(std::is_same_v<T,pulsar::application::TimelineGapPresentation>||std::is_same_v<T,pulsar::application::TimelineSourceGapPresentation>){clearProgram(has_video_media_ ? "gap" : "empty");}else if constexpr(std::is_same_v<T,pulsar::application::TimelineMissingMediaPresentation>){clearProgram(has_video_media_ ? "missing" : "empty");}else if constexpr(std::is_same_v<T,pulsar::application::TimelineStarvedPresentation>){
  const auto epoch = visual_mailbox_->active_epoch();
  const auto generation = active_session_->document().document_generation();
  const bool same = displayed_identity_.has_value() && displayed_identity_->session_epoch == editor_session_epoch_ &&
                    displayed_identity_->document_generation == generation && displayed_identity_->clip_id == value.clip_id &&
                    displayed_identity_->media_id == value.media_id && displayed_identity_->source_epoch == epoch &&
                    viewer_state_ == "frame";
  // A decoder transition can starve for a tick while the previous frame is
  // still valid. Keep that frame until the first sample of the next clip;
  // real editorial gaps use TimelineGapPresentation and still clear to black.
  if (!same && viewer_state_ != "frame") clearProgram("loading");
}else if constexpr(std::is_same_v<T,pulsar::application::TimelineSourceEndedEarlyPresentation>||std::is_same_v<T,pulsar::application::TimelineSourceErrorPresentation>){clearProgram("error");}else if constexpr(std::is_same_v<T,pulsar::application::TimelineEndedPresentation>){clearProgram("ended");}},presentation);}
void DesktopController::displayLease(const pulsar::application::TimelinePresentSample& present){const auto source_epoch=visual_mailbox_->active_epoch();const auto generation=active_session_->document().document_generation();VisualIdentity identity{editor_session_epoch_,generation,present.clip_id,present.media_id,source_epoch,present.sample.ordinal};if(displayed_identity_&&displayed_identity_.value().session_epoch==identity.session_epoch&&displayed_identity_->document_generation==identity.document_generation&&displayed_identity_->clip_id==identity.clip_id&&displayed_identity_->media_id==identity.media_id&&displayed_identity_->source_epoch==identity.source_epoch&&displayed_identity_->ordinal==identity.ordinal&&viewer_state_=="frame")return;auto lease=visual_mailbox_->take(source_epoch,present.sample.ordinal);if(!lease){++visual_misses_;clearProgram("loading");return;}try{FFmpegPreviewConverter converter;auto preview=converter.convert(*lease);image_provider_.setImage(imageFromPreview(preview));last_conversion_time_=preview.conversion_time;conversion_durations_.push(preview.conversion_time);++conversion_count_;displayed_identity_=identity;viewer_state_="frame";++displayed_frames_;++preview_revision_;preview_source_=QString("image://pulsar-preview/current?rev=%1").arg(preview_revision_);emit previewChanged();}catch(const std::exception& error){setError(QString::fromUtf8(error.what()));clearProgram("error");}}
void DesktopController::clearProgram(QString state){const bool changed=viewer_state_!=state||!preview_source_.isEmpty();image_provider_.clear();preview_source_.clear();displayed_identity_.reset();viewer_state_=std::move(state);if(changed)emit previewChanged();}
void DesktopController::updateTick(){if(!active_session_||!has_media_){return;}try{const auto presentation=active_session_->update_playback();presentTimeline(presentation);const bool now_playing=active_session_->playback_intent()==pulsar::application::PlaybackIntent::Playing;if(now_playing!=playing_){playing_=now_playing;emit playbackChanged();}updatePositionProperties();updateDiagnostics();}catch(const std::exception& error){active_session_->pause();playing_=false;emit playbackChanged();clearProgram("error");setError(QString::fromUtf8(error.what()));setStatus("Error");}}
void DesktopController::updatePositionProperties(){if(!active_session_)return;const auto position=active_session_->playback_position();const auto duration=active_session_->timeline_duration();position_text_=formatTime(position);duration_text_=formatTime(duration);progress_=duration>MediaTime::zero()?std::clamp(secondsAsDouble(position)/secondsAsDouble(duration),0.0,1.0):0.0;timeline_playhead_progress_=timeline_view_start_&&timeline_view_duration_&&position>=*timeline_view_start_&&position<=*timeline_view_start_+*timeline_view_duration_?timelinePixelAt(position,1.0):-1.0;emit positionChanged();emit timelinePlayheadChanged();emit editActionStateChanged();}
void DesktopController::publishProjectDirty() {
  const bool now = projectDirty();
  if (now == published_project_dirty_) {
    return;
  }
  published_project_dirty_ = now;
  emit projectDirtyChanged();
}
void DesktopController::updateDiagnostics(){
  const auto audio = active_session_ != nullptr ? active_session_->audio_metrics()
                                                 : pulsar::application::AudioMixPlaybackMetrics{};
  const auto submitted = audio.output.device_frames_submitted;
  const auto processed = audio.output.device_frames_processed;
  const auto in_flight = submitted >= processed ? submitted - processed : 0;
  debug_metrics_=QString("program_frames=%1 visual_misses=%2 conversion_ms=%3 waveform_queries=%4 waveform_segments=%5 audio_underruns=%6 audio_buffered=%7 audio_in_flight=%8 audio_render_p50_us=%9 audio_render_p95_us=%10 audio_render_max_us=%11 audio_source_opens=%12 audio_live_sources=%13 audio_clock_transitions=%14 audio_ts_obs=%15 audio_ts_silence=%16 audio_ts_trim=%17")
      .arg(displayed_frames_).arg(visual_misses_)
      .arg(std::chrono::duration_cast<std::chrono::milliseconds>(last_conversion_time_).count())
      .arg(waveform_query_count_).arg(waveform_segment_count_)
      .arg(audio.output.underrun_count).arg(audio.output.buffered_device_frames)
      .arg(in_flight).arg(audio.render_p50_us).arg(audio.render_p95_us).arg(audio.render_max_us)
      .arg(audio.mixer.source_opens).arg(audio.mixer.live_sources).arg(audio.clock_transitions)
      .arg(audio.mixer.timestamp_observations)
      .arg(audio.mixer.timestamp_inserted_silence_frames)
      .arg(audio.mixer.timestamp_trimmed_frames);
  emit diagnosticsChanged();
}
QString DesktopController::performanceSnapshot() const {
  auto summarize = [](const DurationSampleRing& ring) {
    auto sorted = ring.snapshot();
    std::sort(sorted.begin(), sorted.end());
    return std::make_tuple(percentile_sorted(sorted, 0.50), percentile_sorted(sorted, 0.95),
                           sorted.empty() ? std::chrono::nanoseconds{0} : sorted.back());
  };
  const auto [sync_p50, sync_p95, sync_max] = summarize(
      active_session_ != nullptr ? active_session_->synchronization_diagnostics().recent_durations
                                 : DurationSampleRing{});
  std::uint64_t sync_performed = 0;
  std::uint64_t sync_skipped = 0;
  if (active_session_ != nullptr) {
    const auto sync = active_session_->synchronization_diagnostics();
    sync_performed = sync.performed_count;
    sync_skipped = sync.skipped_count;
  }
  const auto [ctor_p50, ctor_p95, ctor_max] = summarize(worker_ctor_durations_);
  const auto [desc_p50, desc_p95, desc_max] = summarize(worker_descriptor_durations_);
  const auto [conv_p50, conv_p95, conv_max] = summarize(conversion_durations_);
  const auto [wave_query_p50, wave_query_p95, wave_query_max] =
      summarize(waveform_query_durations_);
  const auto [wave_prep_p50, wave_prep_p95, wave_prep_max] =
      summarize(waveform_geometry_durations_);
  const auto mailbox = visual_mailbox_ != nullptr ? visual_mailbox_->stats()
                                                  : pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox::Stats{};
  QStringList lines;
  lines << QStringLiteral("desktop.sync_performed_count=%1").arg(sync_performed);
  lines << QStringLiteral("desktop.sync_skipped_count=%1").arg(sync_skipped);
  lines << QStringLiteral("desktop.sync_p50_us=%1").arg(to_microseconds(sync_p50));
  lines << QStringLiteral("desktop.sync_p95_us=%1").arg(to_microseconds(sync_p95));
  lines << QStringLiteral("desktop.sync_max_us=%1").arg(to_microseconds(sync_max));
  lines << QStringLiteral("desktop.worker_construction_count=%1").arg(worker_construction_count_);
  lines << QStringLiteral("desktop.worker_ctor_p50_us=%1").arg(to_microseconds(ctor_p50));
  lines << QStringLiteral("desktop.worker_ctor_p95_us=%1").arg(to_microseconds(ctor_p95));
  lines << QStringLiteral("desktop.worker_ctor_max_us=%1").arg(to_microseconds(ctor_max));
  lines << QStringLiteral("desktop.descriptor_p50_us=%1").arg(to_microseconds(desc_p50));
  lines << QStringLiteral("desktop.descriptor_p95_us=%1").arg(to_microseconds(desc_p95));
  lines << QStringLiteral("desktop.descriptor_max_us=%1").arg(to_microseconds(desc_max));
  lines << QStringLiteral("desktop.conversion_count=%1").arg(conversion_count_);
  lines << QStringLiteral("desktop.conversion_p50_us=%1").arg(to_microseconds(conv_p50));
  lines << QStringLiteral("desktop.conversion_p95_us=%1").arg(to_microseconds(conv_p95));
  lines << QStringLiteral("desktop.conversion_max_us=%1").arg(to_microseconds(conv_max));
  lines << QStringLiteral("waveform.query_count=%1").arg(waveform_query_count_);
  lines << QStringLiteral("waveform.segment_count_total=%1").arg(waveform_segment_count_);
  lines << QStringLiteral("waveform.visible_items=%1").arg(waveform_items_.size());
  lines << QStringLiteral("waveform.query_p50_us=%1").arg(to_microseconds(wave_query_p50));
  lines << QStringLiteral("waveform.query_p95_us=%1").arg(to_microseconds(wave_query_p95));
  lines << QStringLiteral("waveform.query_max_us=%1").arg(to_microseconds(wave_query_max));
  lines << QStringLiteral("waveform.prepare_p50_us=%1").arg(to_microseconds(wave_prep_p50));
  lines << QStringLiteral("waveform.prepare_p95_us=%1").arg(to_microseconds(wave_prep_p95));
  lines << QStringLiteral("waveform.prepare_max_us=%1").arg(to_microseconds(wave_prep_max));
  lines << QStringLiteral("desktop.displayed_frames=%1").arg(displayed_frames_);
  lines << QStringLiteral("desktop.visual_misses=%1").arg(visual_misses_);
  lines << QStringLiteral("desktop.mailbox_published=%1").arg(mailbox.published);
  lines << QStringLiteral("desktop.mailbox_evicted=%1").arg(mailbox.evicted);
  lines << QStringLiteral("desktop.mailbox_high_water=%1").arg(mailbox.high_water_mark);
  lines << QStringLiteral("desktop.mailbox_current_size=%1").arg(mailbox.current_size);
  if (active_session_ != nullptr) {
    const auto& audio = active_session_->audio_metrics();
    const auto submitted = audio.output.device_frames_submitted;
    const auto processed = audio.output.device_frames_processed;
    const auto in_flight = submitted >= processed ? submitted - processed : 0;
    lines << QStringLiteral("audio.underruns=%1").arg(audio.output.underrun_count);
    lines << QStringLiteral("audio.buffered_device_frames=%1").arg(audio.output.buffered_device_frames);
    lines << QStringLiteral("audio.in_flight_frames=%1").arg(in_flight);
    lines << QStringLiteral("audio.render_p50_us=%1").arg(audio.render_p50_us);
    lines << QStringLiteral("audio.render_p95_us=%1").arg(audio.render_p95_us);
    lines << QStringLiteral("audio.render_max_us=%1").arg(audio.render_max_us);
    lines << QStringLiteral("audio.source_opens=%1").arg(audio.mixer.source_opens);
    lines << QStringLiteral("audio.live_sources=%1").arg(audio.mixer.live_sources);
    lines << QStringLiteral("audio.clock_transitions=%1").arg(audio.clock_transitions);
    lines << QStringLiteral("audio.timestamp_observations=%1").arg(audio.mixer.timestamp_observations);
    lines << QStringLiteral("audio.timestamp_positive_deltas=%1").arg(audio.mixer.timestamp_positive_deltas);
    lines << QStringLiteral("audio.timestamp_negative_deltas=%1").arg(audio.mixer.timestamp_negative_deltas);
    lines << QStringLiteral("audio.timestamp_inserted_silence_frames=%1").arg(audio.mixer.timestamp_inserted_silence_frames);
    lines << QStringLiteral("audio.timestamp_trimmed_frames=%1").arg(audio.mixer.timestamp_trimmed_frames);
  }
  return lines.join(QLatin1Char('\n'));
}
void DesktopController::setBusy(bool value){if(busy_==value)return;busy_=value;emit busyChanged();}void DesktopController::setStatus(QString value){if(status_==value)return;status_=std::move(value);emit statusChanged();}void DesktopController::setTechnicalStatus(QString value){if(technical_status_==value)return;technical_status_=std::move(value);emit technicalStatusChanged();}void DesktopController::setPreviewLate(bool value){if(preview_late_==value)return;preview_late_=value;emit technicalStatusChanged();}void DesktopController::setExportStatus(QString value){if(export_status_==value)return;export_status_=std::move(value);emit exportStatusChanged();}void DesktopController::setError(QString value){error_message_=std::move(value);emit errorChanged();if(!error_message_.isEmpty())error_timer_.start();else error_timer_.stop();}void DesktopController::clearError(){if(error_message_.isEmpty())return;error_message_.clear();emit errorChanged();}
QString DesktopController::formatTime(MediaTime time){auto sec=time.seconds();auto whole=sec.numerator()/sec.denominator();auto rem=sec.numerator()%sec.denominator();auto ms=static_cast<std::int64_t>(std::llround(static_cast<long double>(rem)*1000.0L/static_cast<long double>(sec.denominator())));if(ms>=1000){++whole;ms=0;}return QString("%1:%2:%3.%4").arg(whole/3600,2,10,QChar('0')).arg((whole/60)%60,2,10,QChar('0')).arg(whole%60,2,10,QChar('0')).arg(ms,3,10,QChar('0'));}
MediaTime DesktopController::mediaTimeFromSeconds(double seconds){if(!std::isfinite(seconds)||seconds<=0)return MediaTime::zero();const auto q=std::llround(seconds*1'000'000.0);return MediaTime{q,Rational{1,1'000'000}};}
double DesktopController::secondsAsDouble(MediaTime time){const auto sec=time.seconds();return static_cast<double>(sec.numerator())/static_cast<double>(sec.denominator());}
