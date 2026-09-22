#include "desktop_controller.hpp"

#include "project_path_resolution.hpp"
#include "waveform_quick_item.hpp"

#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_audio_playback_source.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_playback_worker.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>
#if defined(PULSAR_HAS_QT_AUDIO)
#include <pulsar/media/qt_audio/qt_audio_output.hpp>
#endif
#include <pulsar/project_file/file_project_repository.hpp>

#include <QMetaObject>
#include <QStandardPaths>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <iomanip>
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
}

DesktopController::DesktopController(PreviewImageProvider& image_provider,
                                     WaveformSourceCreator waveform_source_creator,
                                     QObject* parent)
    : QObject{parent}, image_provider_{image_provider},
      visual_mailbox_{std::make_shared<pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox>(8)},
      waveform_source_creator_{std::move(waveform_source_creator)} {
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
  worker_ = std::thread{&DesktopController::workerLoop, this};
}
DesktopController::~DesktopController() {
  tick_timer_.stop();
  waveform_timer_.stop();
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
  ++seek_count_; active_session_->seek(*target); clearProgram("loading"); updatePositionProperties();
}
void DesktopController::seekToStart() { seekNormalized(0.0); }
void DesktopController::seekToEnd() { seekNormalized(1.0); }
void DesktopController::closeMedia() {
  ++import_request_generation_;
  pause();
  active_session_ = makeEditorSession();
  active_project_file_.clear();
  waveform_clip_identities_.clear();
  waveform_requests_.clear();
  waveform_item_stamps_.clear();
  ++editor_session_epoch_;
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
  recomputeTimelineViewport();
  selected_clip_key_ = active_session_->selected_clip() ? QString::number(active_session_->selected_clip()->value()) : QString{};
  timeline_model_.refresh(*active_session_, *timeline_view_start_, *timeline_view_duration_);
  const auto duration = active_session_->timeline_duration();
  has_media_ = duration > MediaTime::zero();
  timeline_duration_text_ = formatTime(duration);
  duration_text_ = formatTime(duration);
  timeline_ruler_ticks_.clear();
  constexpr int ruler_tick_count = 7;
  for (int index = 0; index < ruler_tick_count; ++index) {
    const double progress = static_cast<double>(index) / static_cast<double>(ruler_tick_count - 1);
    const auto tick_time = timelineTimeAt(progress, 1.0);
    if (!tick_time) continue;
    QVariantMap tick;
    tick.insert("progress", progress);
    tick.insert("label", formatTime(*tick_time));
    timeline_ruler_ticks_.push_back(tick);
  }
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
    runtime_path = pulsar::desktop::resolve_runtime_media_path(active_project_file_,
                                                                binding.source_locator);
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
  if (!active_session_ || result.video_streams.empty()) return;
  try {
    StaticProbe probe{result}; const auto media = active_session_->processor().import_asset(result.path, probe); const auto& stream = result.video_streams.front();
    const auto duration = stream.duration ? stream.duration : result.duration;
    if (duration && *duration > MediaTime::zero() && active_session_->document().timeline().duration() == MediaTime::zero()) (void)active_session_->processor().add_clip(media, pulsar::domain::TimeRange{MediaTime::zero(), *duration}, MediaTime::zero());
    active_session_->synchronize_document_state(); publishTimeline(); has_media_ = active_session_->timeline_duration() > MediaTime::zero(); media_name_ = QString::fromStdString(result.path.filename().string()); metadata_ = metadataText(result, stream); duration_text_ = formatTime(active_session_->timeline_duration()); setBusy(false); setStatus(has_media_ ? "Pausado" : "Listo"); setTechnicalStatus("Ready · Program"); emit mediaChanged();
  } catch (const std::exception& error) { setBusy(false); setError(QString::fromUtf8(error.what())); setStatus("Error"); }
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
void DesktopController::seekTimelineAt(double pixel, double width) { if (!active_session_) return; const auto target=timelineTimeAt(pixel,width); if (!target) return; active_session_->seek(*target); clearProgram("loading"); updatePositionProperties(); }
void DesktopController::moveTimelineClip(const QString& key,double press,double current,double width) { if (!active_session_) return; bool ok=false; auto value=key.toULongLong(&ok);if(!ok||!value)return;const auto press_time=timelineTimeAt(press,width);const auto current_time=timelineTimeAt(current,width);if(!press_time||!current_time)return;try{active_session_->move_clip(pulsar::domain::ClipId{value},*press_time,*current_time);clearError();clearProgram("loading");publishTimeline();}catch(...){setError("El movimiento no es válido.");publishTimeline();} }
void DesktopController::trimTimelineClipStart(const QString& key,double pixel,double width) { if(!active_session_)return;bool ok=false;auto value=key.toULongLong(&ok);if(!ok||!value)return;const auto target=timelineTimeAt(pixel,width);if(!target)return;try{active_session_->trim_clip_start(pulsar::domain::ClipId{value},*target);clearError();clearProgram("loading");publishTimeline();}catch(...){setError("El recorte no es válido.");publishTimeline();} }
void DesktopController::trimTimelineClipEnd(const QString& key,double pixel,double width) { if(!active_session_)return;bool ok=false;auto value=key.toULongLong(&ok);if(!ok||!value)return;const auto target=timelineTimeAt(pixel,width);if(!target)return;try{active_session_->trim_clip_end(pulsar::domain::ClipId{value},*target);clearError();clearProgram("loading");publishTimeline();}catch(...){setError("El recorte no es válido.");publishTimeline();} }
void DesktopController::splitSelectedClip(){if(!active_session_)return;try{(void)active_session_->split_selected_at_playhead();clearError();clearProgram("loading");publishTimeline();}catch(...){setError("El punto de corte debe estar dentro del clip.");}}
void DesktopController::deleteSelectedClip(){if(!active_session_)return;try{active_session_->delete_selected();clearError();clearProgram("loading");publishTimeline();}catch(...){} }
void DesktopController::setSelectedClipGainDb(double gain_db) {
  if (!active_session_ || !active_session_->selected_clip() || !std::isfinite(gain_db)) return;
  try {
    active_session_->set_clip_audio_gain(*active_session_->selected_clip(), gain_db);
    clearError();
    publishTimeline();
  } catch (const std::exception& error) {
    setError(QString::fromUtf8(error.what()));
  }
}
void DesktopController::toggleSelectedClipMute() {
  if (!active_session_ || !active_session_->selected_clip()) return;
  try {
    active_session_->set_clip_muted(*active_session_->selected_clip(), !selectedClipMuted());
    clearError();
    publishTimeline();
  } catch (const std::exception& error) {
    setError(QString::fromUtf8(error.what()));
  }
}
void DesktopController::undoEdit(){if(active_session_&&active_session_->undo()){clearProgram("loading");publishTimeline();}}
void DesktopController::redoEdit(){if(active_session_&&active_session_->redo()){clearProgram("loading");publishTimeline();}}
void DesktopController::saveProject(const QString& path) {
  const QString trimmed = path.trimmed();
  if (trimmed.isEmpty() || !active_session_) {
    return;
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
    publishProjectDirty();
  } catch (const pulsar::application::ProjectRepositoryError& error) {
    setError(QString::fromUtf8(error.what()));
  }
}
void DesktopController::openProject(const QString& path) {
  const QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) {
    return;
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
    active_project_file_ = project_path;
    waveform_clip_identities_.clear();
    waveform_requests_.clear();
    waveform_item_stamps_.clear();
    ++editor_session_epoch_;
  } catch (const std::exception& error) {
    setBusy(false);
    setError(QString::fromUtf8(error.what()));
    setStatus(QStringLiteral("Error"));
    return;
  }
  pause();
  has_media_ = false;
  media_name_.clear();
  metadata_.clear();
  duration_text_ = "--:--:--.---";
  progress_ = 0.0;
  position_text_ = "00:00:00.000";
  publishTimeline();
  clearProgram(active_session_->timeline_duration() > MediaTime::zero() ? "loading" : "empty");
  setBusy(false);
  clearError();
  setStatus(QStringLiteral("Proyecto abierto"));
  emit mediaChanged();
  emit playbackChanged();
  emit positionChanged();
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
        pulsar::desktop::resolve_runtime_media_path(project_file, path);
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
        pulsar::desktop::resolve_runtime_media_path(project_file, path);
    return std::make_unique<pulsar::media::ffmpeg::FFmpegAudioPlaybackSource>(runtime_path,
                                                                                stream);
  };
}

void DesktopController::postOpen(std::filesystem::path path, std::uint64_t generation) { setBusy(true); setStatus("Preparando…"); setTechnicalStatus("Probing"); clearError(); enqueue(WorkerCommand{std::move(path),generation}); }
void DesktopController::enqueue(WorkerCommand command){{std::lock_guard lock{mutex_};pending_command_=std::move(command);}wake_.notify_one();}
void DesktopController::workerLoop(){while(true){WorkerCommand command;{std::unique_lock lock{mutex_};wake_.wait(lock,[this]{return stop_worker_||pending_command_.has_value();});if(stop_worker_)return;command=std::move(*pending_command_);pending_command_.reset();}try{FFmpegMediaProbe probe;auto result=probe.probe(command.path);if(result.video_streams.empty())throw std::runtime_error{"media has no video stream"};QMetaObject::invokeMethod(this,[this,generation=command.generation,result=std::move(result)]()mutable{if(generation!=import_request_generation_){++stale_visual_frames_;return;}installTimelineAsset(result);},Qt::QueuedConnection);}catch(const std::exception& error){QMetaObject::invokeMethod(this,[this,generation=command.generation,message=QString::fromUtf8(error.what())]{if(generation!=import_request_generation_)return;setBusy(false);setError(message);setStatus("Error");},Qt::QueuedConnection);}}}

void DesktopController::presentTimeline(const pulsar::application::TimelinePresentation& presentation){std::visit([this](const auto& value){using T=std::decay_t<decltype(value)>;if constexpr(std::is_same_v<T,pulsar::application::TimelinePresentSample>){displayLease(value);}else if constexpr(std::is_same_v<T,pulsar::application::TimelineHoldPresentation>){pulsar::application::TimelinePresentSample present{value.clip_id,value.media_id,value.timeline_position,value.sample};displayLease(present);}else if constexpr(std::is_same_v<T,pulsar::application::TimelineGapPresentation>||std::is_same_v<T,pulsar::application::TimelineSourceGapPresentation>){clearProgram("gap");}else if constexpr(std::is_same_v<T,pulsar::application::TimelineMissingMediaPresentation>){clearProgram("missing");}else if constexpr(std::is_same_v<T,pulsar::application::TimelineStarvedPresentation>){
  const auto epoch = visual_mailbox_->active_epoch();
  const auto generation = active_session_->document().document_generation();
  const bool same = displayed_identity_.has_value() && displayed_identity_->session_epoch == editor_session_epoch_ &&
                    displayed_identity_->document_generation == generation && displayed_identity_->clip_id == value.clip_id &&
                    displayed_identity_->media_id == value.media_id && displayed_identity_->source_epoch == epoch &&
                    viewer_state_ == "frame";
  if (!same) clearProgram("loading");
}else if constexpr(std::is_same_v<T,pulsar::application::TimelineSourceEndedEarlyPresentation>||std::is_same_v<T,pulsar::application::TimelineSourceErrorPresentation>){clearProgram("error");}else if constexpr(std::is_same_v<T,pulsar::application::TimelineEndedPresentation>){clearProgram("ended");}},presentation);}
void DesktopController::displayLease(const pulsar::application::TimelinePresentSample& present){const auto source_epoch=visual_mailbox_->active_epoch();const auto generation=active_session_->document().document_generation();VisualIdentity identity{editor_session_epoch_,generation,present.clip_id,present.media_id,source_epoch,present.sample.ordinal};if(displayed_identity_&&displayed_identity_.value().session_epoch==identity.session_epoch&&displayed_identity_->document_generation==identity.document_generation&&displayed_identity_->clip_id==identity.clip_id&&displayed_identity_->media_id==identity.media_id&&displayed_identity_->source_epoch==identity.source_epoch&&displayed_identity_->ordinal==identity.ordinal&&viewer_state_=="frame")return;auto lease=visual_mailbox_->take(source_epoch,present.sample.ordinal);if(!lease){++visual_misses_;clearProgram("loading");return;}try{FFmpegPreviewConverter converter;auto preview=converter.convert(*lease);image_provider_.setImage(imageFromPreview(preview));last_conversion_time_=preview.conversion_time;conversion_durations_.push(preview.conversion_time);++conversion_count_;displayed_identity_=identity;viewer_state_="frame";++displayed_frames_;++preview_revision_;preview_source_=QString("image://pulsar-preview/current?rev=%1").arg(preview_revision_);emit previewChanged();}catch(const std::exception& error){setError(QString::fromUtf8(error.what()));clearProgram("error");}}
void DesktopController::clearProgram(QString state){const bool changed=viewer_state_!=state||!preview_source_.isEmpty();image_provider_.clear();preview_source_.clear();displayed_identity_.reset();viewer_state_=std::move(state);if(changed)emit previewChanged();}
void DesktopController::updateTick(){if(!active_session_||!has_media_){return;}const auto presentation=active_session_->update_playback();presentTimeline(presentation);const bool now_playing=active_session_->playback_intent()==pulsar::application::PlaybackIntent::Playing;if(now_playing!=playing_){playing_=now_playing;emit playbackChanged();}updatePositionProperties();updateDiagnostics();}
void DesktopController::updatePositionProperties(){if(!active_session_)return;const auto position=active_session_->playback_position();const auto duration=active_session_->timeline_duration();position_text_=formatTime(position);duration_text_=formatTime(duration);progress_=duration>MediaTime::zero()?std::clamp(secondsAsDouble(position)/secondsAsDouble(duration),0.0,1.0):0.0;timeline_playhead_progress_=timeline_view_start_&&timeline_view_duration_&&position>=*timeline_view_start_&&position<=*timeline_view_start_+*timeline_view_duration_?timelinePixelAt(position,1.0):-1.0;emit positionChanged();emit timelinePlayheadChanged();emit editActionStateChanged();}
void DesktopController::publishProjectDirty() {
  const bool now = projectDirty();
  if (now == published_project_dirty_) {
    return;
  }
  published_project_dirty_ = now;
  emit projectDirtyChanged();
}
void DesktopController::updateDiagnostics(){debug_metrics_=QString("program_frames=%1 visual_misses=%2 conversion_ms=%3 waveform_queries=%4 waveform_segments=%5").arg(displayed_frames_).arg(visual_misses_).arg(std::chrono::duration_cast<std::chrono::milliseconds>(last_conversion_time_).count()).arg(waveform_query_count_).arg(waveform_segment_count_);emit diagnosticsChanged();}
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
  return lines.join(QLatin1Char('\n'));
}
void DesktopController::setBusy(bool value){if(busy_==value)return;busy_=value;emit busyChanged();}void DesktopController::setStatus(QString value){if(status_==value)return;status_=std::move(value);emit statusChanged();}void DesktopController::setTechnicalStatus(QString value){if(technical_status_==value)return;technical_status_=std::move(value);emit technicalStatusChanged();}void DesktopController::setPreviewLate(bool value){if(preview_late_==value)return;preview_late_=value;emit technicalStatusChanged();}void DesktopController::setError(QString value){error_message_=std::move(value);emit errorChanged();}void DesktopController::clearError(){if(error_message_.isEmpty())return;error_message_.clear();emit errorChanged();}
QString DesktopController::formatTime(MediaTime time){auto sec=time.seconds();auto whole=sec.numerator()/sec.denominator();auto rem=sec.numerator()%sec.denominator();auto ms=static_cast<std::int64_t>(std::llround(static_cast<long double>(rem)*1000.0L/static_cast<long double>(sec.denominator())));if(ms>=1000){++whole;ms=0;}return QString("%1:%2:%3.%4").arg(whole/3600,2,10,QChar('0')).arg((whole/60)%60,2,10,QChar('0')).arg(whole%60,2,10,QChar('0')).arg(ms,3,10,QChar('0'));}
MediaTime DesktopController::mediaTimeFromSeconds(double seconds){if(!std::isfinite(seconds)||seconds<=0)return MediaTime::zero();const auto q=std::llround(seconds*1'000'000.0);return MediaTime{q,Rational{1,1'000'000}};}
double DesktopController::secondsAsDouble(MediaTime time){const auto sec=time.seconds();return static_cast<double>(sec.numerator())/static_cast<double>(sec.denominator());}
