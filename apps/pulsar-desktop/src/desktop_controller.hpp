#pragma once

#include "preview_image_provider.hpp"
#include "editor_session.hpp"

#include <pulsar/application/media/media_probe_result.hpp>

#include <pulsar/application/playback/monotonic_clock.hpp>
#include <pulsar/application/playback/playback_transport.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

class DesktopController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(QString technicalStatus READ technicalStatus NOTIFY technicalStatusChanged)
  Q_PROPERTY(QString mediaName READ mediaName NOTIFY mediaChanged)
  Q_PROPERTY(QString metadata READ metadata NOTIFY mediaChanged)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
  Q_PROPERTY(QString previewSource READ previewSource NOTIFY previewChanged)
  Q_PROPERTY(QString positionText READ positionText NOTIFY positionChanged)
  Q_PROPERTY(QString durationText READ durationText NOTIFY mediaChanged)
  Q_PROPERTY(double progress READ progress WRITE seekNormalized NOTIFY positionChanged)
  Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY mediaChanged)
  Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(bool previewLate READ previewLate NOTIFY technicalStatusChanged)
  Q_PROPERTY(QString debugMetrics READ debugMetrics NOTIFY diagnosticsChanged)
  Q_PROPERTY(QVariantList timelineClips READ timelineClips NOTIFY timelineChanged)
  Q_PROPERTY(QString timelineDurationText READ timelineDurationText NOTIFY timelineChanged)
  Q_PROPERTY(double timelinePlayheadProgress READ timelinePlayheadProgress NOTIFY timelineChanged)
  Q_PROPERTY(QString selectedClipKey READ selectedClipKey NOTIFY timelineChanged)
  Q_PROPERTY(bool canSplit READ canSplit NOTIFY timelineChanged)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY timelineChanged)
  Q_PROPERTY(bool canRedo READ canRedo NOTIFY timelineChanged)

public:
  explicit DesktopController(PreviewImageProvider& image_provider, QObject* parent = nullptr);
  ~DesktopController() override;

  [[nodiscard]] QString status() const { return status_; }
  [[nodiscard]] QString technicalStatus() const { return technical_status_; }
  [[nodiscard]] QString mediaName() const { return media_name_; }
  [[nodiscard]] QString metadata() const { return metadata_; }
  [[nodiscard]] QString errorMessage() const { return error_message_; }
  [[nodiscard]] QString previewSource() const { return preview_source_; }
  [[nodiscard]] QString positionText() const { return position_text_; }
  [[nodiscard]] QString durationText() const { return duration_text_; }
  [[nodiscard]] double progress() const { return progress_; }
  [[nodiscard]] bool hasMedia() const { return has_media_; }
  [[nodiscard]] bool playing() const { return playing_; }
  [[nodiscard]] bool busy() const { return busy_; }
  [[nodiscard]] bool previewLate() const { return preview_late_; }
  [[nodiscard]] QString debugMetrics() const { return debug_metrics_; }
  [[nodiscard]] QVariantList timelineClips() const { return timeline_clips_; }
  [[nodiscard]] QString timelineDurationText() const { return timeline_duration_text_; }
  [[nodiscard]] double timelinePlayheadProgress() const { return timeline_playhead_progress_; }
  [[nodiscard]] QString selectedClipKey() const { return selected_clip_key_; }
  [[nodiscard]] bool canSplit() const { return active_session_ && active_session_->can_split(); }
  [[nodiscard]] bool canUndo() const { return active_session_ && active_session_->can_undo(); }
  [[nodiscard]] bool canRedo() const { return active_session_ && active_session_->can_redo(); }

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
  Q_INVOKABLE void moveTimelineClip(const QString& clip_key, double press_pixel,
                                     double current_pixel, double width);
  Q_INVOKABLE void trimTimelineClipStart(const QString& clip_key, double pixel, double width);
  Q_INVOKABLE void trimTimelineClipEnd(const QString& clip_key, double pixel, double width);
  Q_INVOKABLE void splitSelectedClip();
  Q_INVOKABLE void deleteSelectedClip();
  Q_INVOKABLE void undoEdit();
  Q_INVOKABLE void redoEdit();

signals:
  void statusChanged();
  void technicalStatusChanged();
  void mediaChanged();
  void errorChanged();
  void previewChanged();
  void positionChanged();
  void playbackChanged();
  void busyChanged();
  void diagnosticsChanged();
  void timelineChanged();
  void focusImportPathRequested();

private:
  struct WorkerCommand {
    enum class Kind {
      Open,
      Seek,
      Read,
      Close,
    };

    Kind kind;
    std::filesystem::path path;
    std::optional<pulsar::domain::MediaTime> target;
    std::uint64_t generation;
  };

  struct DecodedFrame {
    QImage image;
    pulsar::domain::MediaTime stream_time;
    std::chrono::nanoseconds conversion_time{0};
  };

  struct WorkerState;
  struct OpenedMedia;

  void workerLoop();
  void postOpen(std::filesystem::path path, std::uint64_t generation);
  void postSeek(pulsar::domain::MediaTime target, std::uint64_t generation);
  void postRead(std::uint64_t generation);
  void postClose(std::uint64_t generation);
  void enqueue(WorkerCommand command);
  void requestReadIfNeeded();
  void setBusy(bool busy);
  void setStatus(QString status);
  void setTechnicalStatus(QString status);
  void setPreviewLate(bool late);
  void setError(QString message);
  void clearError();
  void displayFrame(DecodedFrame frame, bool from_seek);
  void updateTick();
  void updatePositionProperties();
  void updateDiagnostics();
  void publishTimeline();
  void installTimelineAsset(const pulsar::application::MediaProbeResult& probe_result);
  [[nodiscard]] pulsar::domain::MediaTime currentPlaybackPosition() const;
  [[nodiscard]] pulsar::domain::MediaTime currentStreamTarget() const;
  [[nodiscard]] pulsar::domain::MediaTime relativeFromStream(pulsar::domain::MediaTime stream_time) const;
  [[nodiscard]] pulsar::domain::MediaTime streamFromRelative(pulsar::domain::MediaTime relative) const;
  [[nodiscard]] static QString formatTime(pulsar::domain::MediaTime time);
  [[nodiscard]] static pulsar::domain::MediaTime mediaTimeFromSeconds(double seconds);
  [[nodiscard]] static double secondsAsDouble(pulsar::domain::MediaTime time);

  PreviewImageProvider& image_provider_;
  pulsar::application::SteadyMonotonicClock clock_;
  pulsar::application::PlaybackTransport transport_;
  QTimer tick_timer_;

  std::thread worker_;
  mutable std::mutex mutex_;
  std::condition_variable wake_;
  bool stop_worker_{false};
  std::optional<WorkerCommand> pending_command_;

  std::uint64_t generation_{0};
  bool read_in_flight_{false};
  std::optional<DecodedFrame> pending_frame_;
  std::optional<pulsar::domain::MediaTime> origin_stream_time_;
  std::optional<pulsar::domain::MediaTime> duration_;
  std::optional<pulsar::domain::MediaTime> visible_stream_time_;

  QString status_{"Vacío"};
  QString technical_status_{"Preview CPU"};
  QString media_name_;
  QString metadata_;
  QString error_message_;
  QString preview_source_;
  QString position_text_{"00:00:00.000"};
  QString duration_text_{"--:--:--.---"};
  QString debug_metrics_;
  QVariantList timeline_clips_;
  QString timeline_duration_text_{QStringLiteral("00:00:00.000")};
  QString selected_clip_key_;
  double timeline_playhead_progress_{0.0};
  std::unique_ptr<pulsar::desktop::EditorSession> active_session_;
  double progress_{0.0};
  bool has_media_{false};
  bool playing_{false};
  bool busy_{false};
  bool preview_late_{false};

  std::uint64_t preview_revision_{0};
  std::uint64_t stale_visual_frames_{0};
  std::uint64_t displayed_frames_{0};
  std::uint64_t seek_count_{0};
  std::uint64_t read_count_{0};
  std::chrono::nanoseconds last_conversion_time_{0};
  std::chrono::nanoseconds last_seek_visible_latency_{0};
  QElapsedTimer seek_latency_timer_;
};
