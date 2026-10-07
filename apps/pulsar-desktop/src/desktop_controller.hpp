#pragma once

#include "editor_session.hpp"
#include "duration_sample_ring.hpp"
#include "preview_image_provider.hpp"
#include "timeline_clip_list_model.hpp"
#include "media_asset_list_model.hpp"

#include <pulsar/application/media/media_probe_result.hpp>
#include <pulsar/application/playback/timeline_playback_coordinator.hpp>
#include <pulsar/application/waveform/waveform_cache.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_playback_visual_mailbox.hpp>

#include <QElapsedTimer>
#include <QAbstractItemModel>
#include <QPointer>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <condition_variable>
#include <cstdint>
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class WaveformQuickItem;

class DesktopController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(QString technicalStatus READ technicalStatus NOTIFY technicalStatusChanged)
  Q_PROPERTY(QString mediaName READ mediaName NOTIFY mediaChanged)
  Q_PROPERTY(QString metadata READ metadata NOTIFY mediaChanged)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
  Q_PROPERTY(QString previewSource READ previewSource NOTIFY previewChanged)
  Q_PROPERTY(QString viewerState READ viewerState NOTIFY previewChanged)
  Q_PROPERTY(QString positionText READ positionText NOTIFY positionChanged)
  Q_PROPERTY(QString durationText READ durationText NOTIFY mediaChanged)
  Q_PROPERTY(double progress READ progress WRITE seekNormalized NOTIFY positionChanged)
  Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY mediaChanged)
  Q_PROPERTY(bool hasVideoMedia READ hasVideoMedia NOTIFY mediaChanged)
  Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(bool exportRunning READ exportRunning NOTIFY exportRunningChanged)
  Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportProgressChanged)
  Q_PROPERTY(QString exportStatus READ exportStatus NOTIFY exportStatusChanged)
  Q_PROPERTY(QString exportDestination READ exportDestination NOTIFY exportDestinationChanged)
  Q_PROPERTY(bool projectDirty READ projectDirty NOTIFY projectDirtyChanged)
  Q_PROPERTY(QString activeProjectPath READ activeProjectPath NOTIFY activeProjectPathChanged)
  Q_PROPERTY(bool previewLate READ previewLate NOTIFY technicalStatusChanged)
  Q_PROPERTY(QString debugMetrics READ debugMetrics NOTIFY diagnosticsChanged)
  Q_PROPERTY(QAbstractItemModel* timelineClips READ timelineClips NOTIFY timelineChanged)
  Q_PROPERTY(QVariantList timelineTracks READ timelineTracks NOTIFY timelineChanged)
  Q_PROPERTY(QString timelineDurationText READ timelineDurationText NOTIFY timelineChanged)
  Q_PROPERTY(QVariantList timelineRulerTicks READ timelineRulerTicks NOTIFY timelineChanged)
  Q_PROPERTY(double timelinePlayheadProgress READ timelinePlayheadProgress NOTIFY timelinePlayheadChanged)
  Q_PROPERTY(double timelineZoom READ timelineZoom NOTIFY timelineViewChanged)
  Q_PROPERTY(double timelineScroll READ timelineScroll NOTIFY timelineViewChanged)
  Q_PROPERTY(double waveformDensity READ waveformDensity WRITE setWaveformDensity NOTIFY waveformPolicyChanged)
  Q_PROPERTY(QAbstractItemModel* mediaAssets READ mediaAssets NOTIFY assetsChanged)
  Q_PROPERTY(QString selectedAssetId READ selectedAssetId NOTIFY assetsChanged)
  Q_PROPERTY(QString selectedClipKey READ selectedClipKey NOTIFY timelineChanged)
  Q_PROPERTY(double selectedClipGainDb READ selectedClipGainDb NOTIFY timelineChanged)
  Q_PROPERTY(bool selectedClipMuted READ selectedClipMuted NOTIFY timelineChanged)
  Q_PROPERTY(bool canSplit READ canSplit NOTIFY editActionStateChanged)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY timelineChanged)
  Q_PROPERTY(bool canRedo READ canRedo NOTIFY timelineChanged)
  Q_PROPERTY(QVariantList soundLibrary READ soundLibrary NOTIFY soundLibraryChanged)
public:
  using WaveformSourceCreator = std::function<std::unique_ptr<pulsar::application::WaveformSource>(
      const std::filesystem::path&, int)>;
  explicit DesktopController(PreviewImageProvider& image_provider,
                             WaveformSourceCreator waveform_source_creator,
                             QObject* parent = nullptr);
  ~DesktopController() override;
  [[nodiscard]] QString status() const { return status_; }
  [[nodiscard]] QString technicalStatus() const { return technical_status_; }
  [[nodiscard]] QString mediaName() const { return media_name_; }
  [[nodiscard]] QString metadata() const { return metadata_; }
  [[nodiscard]] QString errorMessage() const { return error_message_; }
  [[nodiscard]] QString previewSource() const { return preview_source_; }
  [[nodiscard]] QString viewerState() const { return viewer_state_; }
  [[nodiscard]] QString positionText() const { return position_text_; }
  [[nodiscard]] QString durationText() const { return duration_text_; }
  [[nodiscard]] double progress() const { return progress_; }
  [[nodiscard]] bool hasMedia() const { return has_media_; }
  [[nodiscard]] bool hasVideoMedia() const { return has_video_media_; }
  [[nodiscard]] bool playing() const { return playing_; }
  [[nodiscard]] bool busy() const { return busy_; }
  [[nodiscard]] bool exportRunning() const noexcept { return export_running_; }
  [[nodiscard]] double exportProgress() const noexcept { return export_progress_; }
  [[nodiscard]] QString exportStatus() const { return export_status_; }
  [[nodiscard]] QString exportDestination() const { return export_destination_; }
  [[nodiscard]] bool projectDirty() const noexcept {
    return active_session_ && active_session_->is_dirty();
  }
  [[nodiscard]] QString activeProjectPath() const {
    return QString::fromStdString(active_project_file_.string());
  }
  [[nodiscard]] bool previewLate() const { return preview_late_; }
  [[nodiscard]] QString debugMetrics() const { return debug_metrics_; }
  // On-demand desktop performance snapshot for D3 evidence. Calculates
  // percentiles from copied ring samples; call only for explicit reporting,
  // never on a hot path. Not a Q_PROPERTY, emits no signals.
  [[nodiscard]] QString performanceSnapshot() const;
  [[nodiscard]] QAbstractItemModel* timelineClips() { return &timeline_model_; }
  [[nodiscard]] QVariantList timelineTracks() const { return timeline_tracks_; }
  [[nodiscard]] QString timelineDurationText() const { return timeline_duration_text_; }
  [[nodiscard]] QVariantList timelineRulerTicks() const { return timeline_ruler_ticks_; }
  [[nodiscard]] double timelinePlayheadProgress() const { return timeline_playhead_progress_; }
  [[nodiscard]] double timelineZoom() const { return timeline_zoom_; }
  [[nodiscard]] double timelineScroll() const { return timeline_scroll_; }
  [[nodiscard]] double waveformDensity() const { return waveform_density_; }
  [[nodiscard]] QAbstractItemModel* mediaAssets() { return &asset_model_; }
  [[nodiscard]] QString selectedAssetId() const { return selected_asset_id_; }
  [[nodiscard]] QString selectedClipKey() const { return selected_clip_key_; }
  [[nodiscard]] double selectedClipGainDb() const;
  [[nodiscard]] bool selectedClipMuted() const;
  [[nodiscard]] bool canSplit() const { return active_session_ && active_session_->can_split(); }
  [[nodiscard]] bool canUndo() const { return active_session_ && active_session_->can_undo(); }
  [[nodiscard]] bool canRedo() const { return active_session_ && active_session_->can_redo(); }
  [[nodiscard]] QVariantList soundLibrary() const { return sound_library_; }
  Q_INVOKABLE void openPath(const QString& path);
  Q_INVOKABLE void openUrl(const QUrl& url);
  Q_INVOKABLE void importRequested();
  Q_INVOKABLE void togglePlayback();
  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void seekNormalized(double value);
  Q_INVOKABLE void seekToStart();
  Q_INVOKABLE void seekToEnd();
  Q_INVOKABLE void closeMedia();
  Q_INVOKABLE void selectTimelineClip(const QString& clip_key);
  Q_INVOKABLE void seekTimelineAt(double pixel, double width);
  Q_INVOKABLE void setTimelineZoom(double zoom);
  Q_INVOKABLE void setTimelineScroll(double scroll);
  Q_INVOKABLE void setWaveformDensity(double density);
  Q_INVOKABLE void moveTimelineClip(const QString&, double, double, double);
  Q_INVOKABLE void trimTimelineClipStart(const QString&, double, double);
  Q_INVOKABLE void trimTimelineClipEnd(const QString&, double, double);
  Q_INVOKABLE void splitSelectedClip();
  Q_INVOKABLE void deleteSelectedClip();
  Q_INVOKABLE void setSelectedClipGainDb(double gain_db);
  Q_INVOKABLE void toggleSelectedClipMute();
  Q_INVOKABLE void undoEdit();
  Q_INVOKABLE void redoEdit();
  Q_INVOKABLE void insertBuiltinSound(const QString& sound_id);
  Q_INVOKABLE void saveProject(const QString& path);
  Q_INVOKABLE void openProject(const QString& path);
  // Library actions on imported media. Video assets go to VIDEO 1 as one
  // undoable edit; overlap is refused without changing the project.
  Q_INVOKABLE void selectAsset(const QString& media_id);
  Q_INVOKABLE void insertAssetAtPlayhead(const QString& media_id);
  Q_INVOKABLE void appendAssetToVideoTrack(const QString& media_id);
  Q_INVOKABLE void exportProject(const QString& path);
  Q_INVOKABLE void cancelExport();
  Q_INVOKABLE void attachWaveformItem(const QString& clip_key, QObject* item);
  Q_INVOKABLE void detachWaveformItem(QObject* item);
signals:
  void statusChanged(); void technicalStatusChanged(); void mediaChanged(); void errorChanged();
  void previewChanged(); void positionChanged(); void playbackChanged(); void busyChanged();
  void exportRunningChanged(); void exportProgressChanged(); void exportStatusChanged();
  void exportDestinationChanged();
  void projectDirtyChanged(); void activeProjectPathChanged();
  void diagnosticsChanged(); void timelineChanged(); void timelinePlayheadChanged(); void timelineViewChanged(); void waveformPolicyChanged(); void editActionStateChanged(); void focusImportPathRequested();
  void soundLibraryChanged(); void assetsChanged();
private:
  struct WorkerCommand { std::filesystem::path path; std::uint64_t generation; };
  struct WaveformItemStamp {
    std::optional<pulsar::domain::ClipId> clip_id;
    std::optional<pulsar::domain::MediaId> media_id;
    std::optional<pulsar::domain::MediaTime> source_start;
    std::optional<pulsar::domain::MediaTime> source_end;
    std::optional<pulsar::domain::MediaTime> stream_origin;
    std::uint32_t width{0};
    std::uint32_t height{0};
    double density{1.0};
    pulsar::application::WaveformCacheStatus status{pulsar::application::WaveformCacheStatus::Missing};
    std::uint64_t generated_frames{0};
    bool initialized{false};
  };
  struct VisualIdentity { std::uint64_t session_epoch; std::uint64_t document_generation; pulsar::domain::ClipId clip_id; pulsar::domain::MediaId media_id; std::uint64_t source_epoch; std::uint64_t ordinal; };
  void workerLoop();
  void exportLoop(pulsar::application::ProjectSnapshot snapshot,
                  std::filesystem::path destination, std::filesystem::path project_file,
                  std::uint64_t generation);
  void postOpen(std::filesystem::path path, std::uint64_t generation);
  [[nodiscard]] std::unique_ptr<pulsar::desktop::EditorSession> makeEditorSession();
  // Worker factory bound to one mailbox, measuring construction for D3
  // evidence. An empty project_file leaves authored locators unchanged;
  // otherwise relative locators resolve against the project directory for
  // FFmpeg only (the document keeps the authored value).
  [[nodiscard]] pulsar::application::PlaybackWorkerFactory makeWorkerFactory(
      std::shared_ptr<pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox> mailbox,
      std::filesystem::path project_file);
  [[nodiscard]] pulsar::application::AudioPlaybackSourceFactory makeAudioSourceFactory(
      std::filesystem::path project_file);
  void enqueue(WorkerCommand command);
  void updateTick();
  void updatePositionProperties();
  void updateDiagnostics();
  void publishTimeline();
  void recomputeTimelineViewport();
  [[nodiscard]] std::optional<pulsar::domain::MediaTime> timelineTimeAt(double pixel,
                                                                        double width) const;
  [[nodiscard]] double timelinePixelAt(pulsar::domain::MediaTime time, double width) const;
  void publishProjectDirty();
  void refreshAssetsIfChanged();
  void applyAssetSelection(const QString& media_id);
  void ingestVideoAsset(const QString& media_id, bool append);
  void refreshWaveformItems();
  void loadSoundLibrary();
  void refreshWaveformItem(WaveformQuickItem* item);
  void installTimelineAsset(const pulsar::application::MediaProbeResult&);
  void presentTimeline(const pulsar::application::TimelinePresentation&);
  void clearProgram(QString state);
  void displayLease(const pulsar::application::TimelinePresentSample&);
  void setBusy(bool); void setStatus(QString); void setTechnicalStatus(QString); void setPreviewLate(bool);
  void setExportStatus(QString);
  void setError(QString); void clearError();
  [[nodiscard]] static QString formatTime(pulsar::domain::MediaTime);
  [[nodiscard]] static pulsar::domain::MediaTime mediaTimeFromSeconds(double);
  [[nodiscard]] static double secondsAsDouble(pulsar::domain::MediaTime);
  PreviewImageProvider& image_provider_;
  QTimer tick_timer_;
  QTimer waveform_timer_;
  std::shared_ptr<pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox> visual_mailbox_;
  pulsar::application::WaveformCacheManager waveform_cache_manager_{1};
  WaveformSourceCreator waveform_source_creator_;
  std::unordered_map<std::string, std::shared_ptr<pulsar::application::WaveformCacheRequest>> waveform_requests_;
  std::vector<QPointer<QObject>> waveform_items_;
  QVariantList sound_library_;
  struct BuiltinSound { std::string id; std::string name; std::string category; std::int64_t duration_frames; };
  std::unordered_map<std::string, BuiltinSound> builtin_sounds_;
  std::unordered_map<std::string, std::string> waveform_clip_identities_;
  std::unordered_map<WaveformQuickItem*, WaveformItemStamp> waveform_item_stamps_;
  std::filesystem::path waveform_cache_directory_;
  std::filesystem::path active_project_file_;
  std::unique_ptr<pulsar::desktop::EditorSession> active_session_;
  std::uint64_t editor_session_epoch_{1};
  std::uint64_t import_request_generation_{0};
  std::thread worker_; mutable std::mutex mutex_; std::condition_variable wake_; bool stop_worker_{false}; std::optional<WorkerCommand> pending_command_;
  std::thread export_thread_;
  std::atomic_bool export_cancel_requested_{false};
  std::uint64_t export_generation_{0};
  QString status_{"Vacío"}; QString technical_status_{"Preview CPU"}; QString media_name_; QString metadata_; QString error_message_; QString preview_source_; QString viewer_state_{"empty"}; QString position_text_{"00:00:00.000"}; QString duration_text_{"--:--:--.---"}; QString debug_metrics_; QVariantList timeline_ruler_ticks_; QVariantList timeline_tracks_; QString timeline_duration_text_{"00:00:00.000"}; QString selected_clip_key_;
  TimelineClipListModel timeline_model_;
  MediaAssetListModel asset_model_;
  QString selected_asset_id_;
  std::uint64_t asset_model_epoch_{0};
  std::uint64_t asset_model_revision_{~std::uint64_t{0}};
  double timeline_playhead_progress_{0.0}; double progress_{0.0}; double export_progress_{0.0}; bool has_media_{false}; bool has_video_media_{false}; bool playing_{false}; bool busy_{false}; bool export_running_{false}; bool preview_late_{false}; bool published_project_dirty_{false};
  QString export_status_{""}; QString export_destination_;
  std::optional<pulsar::domain::MediaTime> timeline_view_start_;
  std::optional<pulsar::domain::MediaTime> timeline_view_duration_;
  double timeline_zoom_{1.0};
  double timeline_scroll_{0.0};
  double waveform_density_{1.0};
  std::optional<VisualIdentity> displayed_identity_; std::uint64_t preview_revision_{0}; std::uint64_t stale_visual_frames_{0}; std::uint64_t displayed_frames_{0}; std::uint64_t visual_misses_{0}; std::uint64_t seek_count_{0}; std::uint64_t read_count_{0}; std::chrono::nanoseconds last_conversion_time_{0};
  // Bounded D3 diagnostics. Single-writer (GUI/control thread); no locks.
  std::uint64_t worker_construction_count_{0};
  std::uint64_t conversion_count_{0};
  std::uint64_t waveform_query_count_{0};
  std::uint64_t waveform_segment_count_{0};
  DurationSampleRing worker_ctor_durations_;
  DurationSampleRing worker_descriptor_durations_;
  DurationSampleRing conversion_durations_;
  DurationSampleRing waveform_query_durations_;
  DurationSampleRing waveform_geometry_durations_;
};
