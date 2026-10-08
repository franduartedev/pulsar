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

#include "recovery_service.hpp"
#include "thumbnail_service.hpp"
#include "title_list_model.hpp"
#include "title_rasterizer.hpp"

#include <pulsar/application/composition/title_compositor.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>
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
#include <deque>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <vector>

class WaveformQuickItem;
class QSoundEffect;

class DesktopController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
  Q_PROPERTY(bool soundPackAvailable READ soundPackAvailable CONSTANT)
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
  Q_PROPERTY(int importPending READ importPending NOTIFY importProgressChanged)
  Q_PROPERTY(int importTotal READ importTotal NOTIFY importProgressChanged)
  Q_PROPERTY(bool exportRunning READ exportRunning NOTIFY exportRunningChanged)
  Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportProgressChanged)
  Q_PROPERTY(QString exportStatus READ exportStatus NOTIFY exportStatusChanged)
  Q_PROPERTY(QString exportDestination READ exportDestination NOTIFY exportDestinationChanged)
  Q_PROPERTY(bool projectDirty READ projectDirty NOTIFY projectDirtyChanged)
  Q_PROPERTY(QString activeProjectPath READ activeProjectPath NOTIFY activeProjectPathChanged)
  Q_PROPERTY(bool previewLate READ previewLate NOTIFY technicalStatusChanged)
  Q_PROPERTY(QString debugMetrics READ debugMetrics NOTIFY diagnosticsChanged)
  Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY recoveryChanged)
  Q_PROPERTY(QString recoveryDescription READ recoveryDescription NOTIFY recoveryChanged)
  Q_PROPERTY(QAbstractItemModel* timelineClips READ timelineClips NOTIFY timelineChanged)
  Q_PROPERTY(QAbstractItemModel* timelineTitles READ timelineTitles NOTIFY timelineChanged)
  Q_PROPERTY(QString selectedTitleKey READ selectedTitleKey NOTIFY timelineChanged)
  Q_PROPERTY(QVariantMap selectedTitle READ selectedTitle NOTIFY timelineChanged)
  Q_PROPERTY(bool selectedTitleOnScreen READ selectedTitleOnScreen NOTIFY selectedTitleOnScreenChanged)   // selected title is drawn at the playhead
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
  Q_PROPERTY(double selectedClipFadeIn READ selectedClipFadeIn NOTIFY timelineChanged)    // seconds
  Q_PROPERTY(double selectedClipFadeOut READ selectedClipFadeOut NOTIFY timelineChanged)  // seconds
  Q_PROPERTY(bool canDuplicate READ canDuplicate NOTIFY timelineChanged)
  Q_PROPERTY(QString selectedClipKind READ selectedClipKind NOTIFY timelineChanged)   // video | image | color | audio | ""
  Q_PROPERTY(bool selectedClipHasAudio READ selectedClipHasAudio NOTIFY timelineChanged)
  Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY snapEnabledChanged)
  Q_PROPERTY(bool canSplit READ canSplit NOTIFY editActionStateChanged)
  Q_PROPERTY(bool canSeparateAudio READ canSeparateAudio NOTIFY editActionStateChanged)
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
  [[nodiscard]] bool recoveryAvailable() const { return recovery_available_; }
  [[nodiscard]] QString recoveryDescription() const { return recovery_description_; }
  [[nodiscard]] QAbstractItemModel* timelineClips() { return &timeline_model_; }
  [[nodiscard]] QAbstractItemModel* timelineTitles() { return &title_model_; }
  [[nodiscard]] QString selectedTitleKey() const;
  [[nodiscard]] QVariantMap selectedTitle() const;
  [[nodiscard]] bool selectedTitleOnScreen() const;
  // Test hook: the first title of the document (nullptr when there is none).
  [[nodiscard]] const pulsar::domain::Title* titleForTest() const { return active_session_ && !active_session_->titles().empty() ? &active_session_->titles().front() : nullptr; }
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
  [[nodiscard]] QString selectedClipKind() const;
  [[nodiscard]] double selectedClipFadeIn() const;
  [[nodiscard]] double selectedClipFadeOut() const;
  [[nodiscard]] bool canDuplicate() const { return active_session_ && active_session_->can_duplicate_selected(); }
  [[nodiscard]] bool selectedClipHasAudio() const {
    return active_session_ && active_session_->selected_clip().has_value() &&
           active_session_->clip_has_audio(*active_session_->selected_clip());
  }
  [[nodiscard]] bool snapEnabled() const { return snap_enabled_; }
  void setSnapEnabled(bool enabled) { if (snap_enabled_ == enabled) return; snap_enabled_ = enabled; emit snapEnabledChanged(); }
  [[nodiscard]] bool canSplit() const { return active_session_ && active_session_->can_split(); }
  [[nodiscard]] bool canSeparateAudio() const {
    return active_session_ && active_session_->selected_clip().has_value() &&
           active_session_->can_separate_audio(*active_session_->selected_clip());
  }
  [[nodiscard]] bool canUndo() const { return active_session_ && active_session_->can_undo(); }
  [[nodiscard]] bool canRedo() const { return active_session_ && active_session_->can_redo(); }
  [[nodiscard]] QVariantList soundLibrary() const { return sound_library_; }
  [[nodiscard]] int importPending() const { return std::max(0, import_total_ - import_done_); }
  [[nodiscard]] int importTotal() const { return import_total_; }
  [[nodiscard]] QString appVersion() const;
  [[nodiscard]] std::shared_ptr<ThumbnailCache> thumbnailCache() const { return thumb_cache_; }
  [[nodiscard]] bool soundPackAvailable() const { return !sound_library_.isEmpty(); }
  Q_INVOKABLE void openPath(const QString& path);
  Q_INVOKABLE void openUrl(const QUrl& url);
  // Batch import: every file is probed in order by the single import worker; assets appear
  // one by one, a bad file never costs the others. Duplicates (same file) are skipped.
  Q_INVOKABLE void openPaths(const QStringList& paths);
  Q_INVOKABLE void openUrls(const QList<QUrl>& urls);
  Q_INVOKABLE void cancelImport();
  Q_INVOKABLE void importRequested();
  Q_INVOKABLE void togglePlayback();
  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void seekNormalized(double value);
  Q_INVOKABLE void seekToStart();
  Q_INVOKABLE void stepFrame(int direction);   // -1 / +1: one frame of the clip under the playhead
  Q_INVOKABLE void seekToEnd();
  Q_INVOKABLE void closeMedia();
  Q_INVOKABLE void selectTimelineClip(const QString& clip_key);
  Q_INVOKABLE void seekTimelineAt(double pixel, double width);
  Q_INVOKABLE void setTimelineZoom(double zoom);
  // Wheel zoom: scales the view by `factor`, keeping the time under `pixel` under the cursor.
  Q_INVOKABLE void zoomTimelineAt(double pixel, double width, double factor);
  // Show the whole program: logical viewport only, clips are untouched.
  Q_INVOKABLE void fitTimeline();
  // Live feedback for a drag, including magnetic snapping. {offset: px to
  // shift the clip by, guide: px of the snapped edge or -1}. Pure: no edit.
  Q_INVOKABLE QVariantMap previewTimelineMove(const QString& clip_key, double press, double current, double width);
  // {px: where the dragged edge should be drawn, guide: snapped edge px or -1}.
  Q_INVOKABLE QVariantMap previewTimelineTrim(const QString& clip_key, bool start_edge, double pixel, double width);
  Q_INVOKABLE void setTimelineScroll(double scroll);
  Q_INVOKABLE void setWaveformDensity(double density);
  Q_INVOKABLE void moveTimelineClip(const QString&, double, double, double);
  Q_INVOKABLE void trimTimelineClipStart(const QString&, double, double);
  Q_INVOKABLE void trimTimelineClipEnd(const QString&, double, double);
  Q_INVOKABLE void splitSelectedClip();
  Q_INVOKABLE void deleteSelectedClip();
  // Embedded audio of the selected video clip -> its own synced audio clip (one undo step).
  Q_INVOKABLE void separateSelectedAudio();
  Q_INVOKABLE void setSelectedClipGainDb(double gain_db);
  Q_INVOKABLE void toggleSelectedClipMute();
  Q_INVOKABLE void setSelectedClipFades(double fade_in_seconds, double fade_out_seconds);
  Q_INVOKABLE void duplicateSelected();
  // ---- Drag & drop from the library. `lane` is the index of the timeline track under the
  // cursor, `pixel` the x inside the timeline area. The preview and the drop share one resolver,
  // so what the ghost shows is exactly what lands (snapping included).
  Q_INVOKABLE QVariantMap previewAssetDrop(const QString& media_id, int lane, double pixel, double width);
  Q_INVOKABLE bool dropAsset(const QString& media_id, int lane, double pixel, double width);
  // ---- Recovery (crash safety). A snapshot of unsaved work is kept apart from the .pulsar file.
  Q_INVOKABLE bool restoreRecovery();     // opens the offered session (stays "unsaved")
  Q_INVOKABLE void discardRecovery();     // forgets it
  Q_INVOKABLE void autosaveNow();         // writes a recovery snapshot if there is unsaved work
  Q_INVOKABLE void flushRecovery() { recovery_.flush(); }
  [[nodiscard]] qulonglong recoveryWriteCount() const { return recovery_.writes_completed(); }
  [[nodiscard]] QString recoveryDirectory() const { return QString::fromStdString(recovery_.store().directory().string()); }
  // ---- Titles v0: static text over the video (see docs/release for scope) -------------------
  Q_INVOKABLE void addTitle();                         // at the playhead
  Q_INVOKABLE void selectTitle(const QString& key);
  Q_INVOKABLE void deleteSelectedTitle();
  Q_INVOKABLE void setSelectedTitleText(const QString& text);
  // colour "#RRGGBB"; h: 0 left 1 centre 2 right; v: 0 top 1 middle 2 bottom; size in 2..20 % of the height
  Q_INVOKABLE void setSelectedTitleStyle(int size_percent, const QString& color, int horizontal, int vertical, bool shadow);
  Q_INVOKABLE void setSelectedTitlePosition(int x_permille, int y_permille);   // block centre, permille of the frame
  Q_INVOKABLE void resetSelectedTitlePosition();                                // back to the alignment presets
  Q_INVOKABLE void setSelectedTitleLook(int background, int opacity_percent);   // 0 none / 1 dark / 2 light box
  Q_INVOKABLE QVariantMap previewTitleMove(const QString& key, double press, double current, double width);
  Q_INVOKABLE void moveTitle(const QString& key, double press, double current, double width);
  Q_INVOKABLE QVariantMap previewTitleTrim(const QString& key, bool trim_start, double pixel, double width);
  Q_INVOKABLE void trimTitle(const QString& key, bool trim_start, double pixel, double width);
  // Export with a size preset: "original" | "1080p" | "720p" (never enlarges the project).
  Q_INVOKABLE void exportProjectWithPreset(const QString& path, const QString& preset);
  Q_INVOKABLE QString exportSizeNote(const QString& preset) const;
  // "Eliminar y cerrar hueco" (see EditorSession::ripple_delete_selected) and its menu gate.
  Q_INVOKABLE void rippleDeleteSelectedClip();
  Q_INVOKABLE bool canRippleDelete() const { return active_session_ && active_session_->can_ripple_delete(); }
  // Q / W: silent no-ops unless the playhead is strictly inside the selected clip.
  Q_INVOKABLE void trimSelectedStartToPlayhead() { trimSelectedToPlayhead(true); }
  Q_INVOKABLE void trimSelectedEndToPlayhead() { trimSelectedToPlayhead(false); }
  // Alt+Right / Alt+Left: moves the selection; reveals the clip if a zoomed view hides it.
  Q_INVOKABLE void selectNextClip() { selectAdjacentClip(true); }
  Q_INVOKABLE void selectPreviousClip() { selectAdjacentClip(false); }
  Q_INVOKABLE void undoEdit();
  Q_INVOKABLE void redoEdit();
  Q_INVOKABLE void insertBuiltinSound(const QString& sound_id);
  Q_INVOKABLE void auditionSound(const QString& sound_id);   // plays a library sound (not part of the project)
  // Returns true only when the project was written; the dirty state is cleared only then.
  Q_INVOKABLE bool saveProject(const QString& path);
  // Returns false (and keeps the current project untouched) if the file cannot be opened.
  Q_INVOKABLE bool openProject(const QString& path);
  // Library actions on imported media. Video assets go to VIDEO 1 as one
  // undoable edit; overlap is refused without changing the project.
  Q_INVOKABLE void selectAsset(const QString& media_id);
  Q_INVOKABLE void insertAssetAtPlayhead(const QString& media_id);
  Q_INVOKABLE void appendAssetToVideoTrack(const QString& media_id);
  Q_INVOKABLE void createColorCard(const QString& color);   // "#rrggbb": a 5 s colour clip
  // Save / export destination helpers (see PathPromptDialog).
  Q_INVOKABLE QString suggestedPath(const QString& purpose) const;
  Q_INVOKABLE QString normalizeSavePath(const QString& text, const QString& extension) const;
  Q_INVOKABLE QString validateSavePath(const QString& path) const;   // "" when usable
  Q_INVOKABLE bool pathExists(const QString& path) const;
  Q_INVOKABLE void exportProject(const QString& path);
  Q_INVOKABLE void cancelExport();
  Q_INVOKABLE void attachWaveformItem(const QString& clip_key, QObject* item);
  Q_INVOKABLE void detachWaveformItem(QObject* item);
signals:
  void statusChanged(); void technicalStatusChanged(); void mediaChanged(); void errorChanged();
  void previewChanged(); void positionChanged(); void selectedTitleOnScreenChanged(); void playbackChanged(); void busyChanged();
  void exportRunningChanged(); void exportProgressChanged(); void exportStatusChanged();
  void exportDestinationChanged();
  void projectDirtyChanged(); void activeProjectPathChanged();
  void diagnosticsChanged(); void timelineChanged(); void timelinePlayheadChanged(); void timelineViewChanged(); void snapEnabledChanged(); void recoveryChanged(); void importProgressChanged(); void waveformPolicyChanged(); void editActionStateChanged(); void focusImportPathRequested();
  void soundLibraryChanged(); void assetsChanged();
private:
  struct WorkerCommand { std::filesystem::path path; std::uint64_t generation; bool auto_place{true}; };
  static constexpr std::size_t kMaxQueuedImports = 500;
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
  void exportLoop(pulsar::application::ProjectSnapshot snapshot, QString preset,
                  std::filesystem::path destination, std::filesystem::path project_file,
                  std::uint64_t generation);
  void postOpen(std::filesystem::path path, bool auto_place);
  void resetImportQueue();
  void finishImportItem(bool ok, const std::filesystem::path& path);
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
  void rebuildRulerTicks();
  [[nodiscard]] static QString rulerLabel(double seconds, double step);
  [[nodiscard]] std::optional<pulsar::domain::MediaTime> snapToleranceAt(double width) const;
  void applyAssetSelection(const QString& media_id);
  void ingestVideoAsset(const QString& media_id, bool append);
  void refreshWaveformItems();
  void loadSoundLibrary();
  void refreshWaveformItem(WaveformQuickItem* item);
  bool installTimelineAsset(const pulsar::application::MediaProbeResult&, bool auto_place = true);
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
  QTimer error_timer_;  // editorial/UI errors fade after a few seconds
  std::shared_ptr<pulsar::media::ffmpeg::FFmpegPlaybackVisualMailbox> visual_mailbox_;
  pulsar::application::WaveformCacheManager waveform_cache_manager_{1};
  WaveformSourceCreator waveform_source_creator_;
  std::unordered_map<std::string, std::shared_ptr<pulsar::application::WaveformCacheRequest>> waveform_requests_;
  std::vector<QPointer<QObject>> waveform_items_;
  [[nodiscard]] std::string thumbnailKey(const pulsar::domain::Clip& clip) const;
  [[nodiscard]] TimelineClipListModel::ThumbnailLookup thumbnailLookup() const;
  void requestThumbnails();
  void resetThumbnails();
  void onThumbnailFinished(const QString& key, bool ok);
  void trimSelectedToPlayhead(bool trim_start);
  void selectAdjacentClip(bool next);
  bool snap_enabled_{true};
  QVariantList sound_library_;
  struct BuiltinSound { std::string id; std::string name; std::string category; std::int64_t duration_frames; };
  std::unordered_map<std::string, BuiltinSound> builtin_sounds_;
  std::unique_ptr<QSoundEffect> audition_;
  std::unordered_map<std::string, std::string> waveform_clip_identities_;
  std::unordered_map<WaveformQuickItem*, WaveformItemStamp> waveform_item_stamps_;
  std::filesystem::path waveform_cache_directory_;
  std::filesystem::path active_project_file_;
  std::unique_ptr<pulsar::desktop::EditorSession> active_session_;
  std::uint64_t editor_session_epoch_{1};
  std::uint64_t import_request_generation_{0};
  // Main-thread bookkeeping of the current import batch (reset when the queue drains/cancels).
  int import_total_{0}; int import_done_{0}; int import_failed_{0}; int import_skipped_{0};
  QString import_first_failure_;
  std::thread worker_; mutable std::mutex mutex_; std::condition_variable wake_; bool stop_worker_{false}; std::deque<WorkerCommand> import_queue_;
  std::thread export_thread_;
  std::atomic_bool export_cancel_requested_{false};
  std::uint64_t export_generation_{0};
  QString status_{"Vacío"}; QString technical_status_{"Preview CPU"}; QString media_name_; QString metadata_; QString error_message_; QString preview_source_; QString viewer_state_{"empty"}; QString position_text_{"00:00:00.000"}; QString duration_text_{"--:--:--.---"}; QString debug_metrics_; QVariantList timeline_ruler_ticks_; QVariantList timeline_tracks_; QString timeline_duration_text_{"00:00:00.000"}; QString selected_clip_key_;
  TimelineClipListModel timeline_model_;
  // Clip thumbnails (decoration): one bounded cache + one worker; see thumbnail_service.hpp.
  std::shared_ptr<ThumbnailCache> thumb_cache_{std::make_shared<ThumbnailCache>()};
  ThumbnailService thumb_service_{thumb_cache_};
  std::unordered_set<std::string> thumb_pending_;
  std::unordered_set<std::string> thumb_failed_;
  QTimer thumb_refresh_timer_;
  RecoveryService recovery_{RecoveryStore::default_directory()};
  QTimer autosave_timer_;
  std::uint64_t recovery_last_history_{0};
  bool recovery_available_{false};
  QString recovery_description_;
  void checkRecoveryAtStartup();
  bool openProjectImpl(const QString& path, std::optional<pulsar::application::ProjectRestoreState> restored);
  // Titles: the same rasteriser + compositor logic serves the preview (here) and the export.
  TitleListModel title_model_;
  std::shared_ptr<pulsar::application::composition::TitleRasterizer> title_rasterizer_{std::make_shared<QtTitleRasterizer>()};
  pulsar::application::composition::TitleCompositor title_compositor_{title_rasterizer_};
  QString title_gap_key_;   // identity of the "title over black" picture currently shown
  [[nodiscard]] std::optional<std::pair<int, int>> projectCanvasSize() const;
  void composeTitlesOnPreview(pulsar::media::ffmpeg::FFmpegPreviewFrame& frame, pulsar::domain::MediaTime time);
  bool showTitleOverGap();
  [[nodiscard]] std::vector<pulsar::domain::MediaTime> titleSnapEdges(std::optional<pulsar::domain::TitleId> exclude) const;
  void commitTitle(pulsar::domain::Title title);
  struct DropResolution { bool valid{false}; QString reason; pulsar::domain::MediaId media{1}; pulsar::domain::TrackId track{1};
                          bool video{false}; bool new_track{false}; pulsar::domain::MediaTime start{pulsar::domain::MediaTime::zero()}; pulsar::domain::MediaTime duration{pulsar::domain::MediaTime::zero()}; };
  [[nodiscard]] DropResolution resolveAssetDrop(const QString& media_id, int lane, double pixel, double width) const;
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
