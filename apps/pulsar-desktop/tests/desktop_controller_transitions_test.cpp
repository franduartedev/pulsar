// End-to-end transition QA: real decoded media, PNG overlay, preview, persistence
// and horizontal/portrait exports all use the desktop controller's normal path.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>

namespace {
namespace fs = std::filesystem;

bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 90'000) {
  if (predicate()) return true;
  QEventLoop loop;
  QTimer poll, timeout;
  poll.setInterval(10);
  QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (predicate()) loop.quit(); });
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  poll.start(); timeout.setSingleShot(true); timeout.start(timeout_ms); loop.exec();
  return predicate();
}

QString asset_id(DesktopController& controller, const QString& name) {
  for (int row = 0; row < controller.mediaAssets()->rowCount(); ++row) {
    const auto index = controller.mediaAssets()->index(row, 0);
    if (index.data(MediaAssetListModel::NameRole).toString() == name) {
      return index.data(MediaAssetListModel::MediaIdRole).toString();
    }
  }
  return {};
}

QString clip_key(DesktopController& controller, const QString& name) {
  for (int row = 0; row < controller.timelineClips()->rowCount(); ++row) {
    const auto index = controller.timelineClips()->index(row, 0);
    if (index.data(TimelineClipListModel::NameRole).toString() == name) {
      return index.data(TimelineClipListModel::ClipKeyRole).toString();
    }
  }
  return {};
}

QVariantMap primary_track(DesktopController& controller) {
  for (const auto& value : controller.agentTimelineState()) {
    const auto track = value.toMap();
    if (track.value(QStringLiteral("kind")).toString() == QStringLiteral("video")) return track;
  }
  return {};
}
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { \
  std::fprintf(stderr, "TRANSITIONS step %d failed: %s | error='%s' export='%s'\n", step, #condition, \
               controller.errorMessage().toUtf8().constData(), controller.exportStatus().toUtf8().constData()); \
  return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider provider;
  DesktopController controller{provider, {}};
  const auto fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  const auto root = fs::temp_directory_path() / ("pulsar-transition-test-" + std::to_string(QCoreApplication::applicationPid()));
  fs::remove_all(root); fs::create_directories(root);

  controller.openPath(QString::fromStdString((fixtures / "e6_video_a.mp4").string()));
  REQUIRE(1, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  controller.openPaths({QString::fromStdString((fixtures / "e6_video_b.mp4").string()),
                        QString::fromStdString((fixtures / "e6_video_rational.mp4").string()),
                        QString::fromStdString((fixtures / "still_alpha.png").string())});
  REQUIRE(2, wait_until([&] { return controller.mediaAssets()->rowCount() == 4 && !controller.busy(); }));

  // Build a true crossfade using B's available 0.5 s source handle, followed by
  // a no-handle dip to black. This is the same selection/UI action used by QML.
  const auto video = primary_track(controller);
  const auto a = video.value(QStringLiteral("clips")).toList().front().toMap();
  const auto video_track = video.value(QStringLiteral("id")).toULongLong();
  const auto a_end = a.value(QStringLiteral("end_seconds")).toDouble();
  QString problem;
  const auto b = controller.agentAddClip(asset_id(controller, QStringLiteral("e6_video_b.mp4")).toULongLong(),
                                         video_track, a_end, 0.5, 1.5, &problem);
  REQUIRE(3, b.has_value());
  controller.selectTimelineClip(clip_key(controller, QStringLiteral("e6_video_a.mp4")));
  controller.addCrossDissolve();
  REQUIRE(4, controller.selectedClipHasTransition() && controller.selectedTransitionType() == QStringLiteral("crossfade") &&
                 std::abs(controller.selectedTransitionDuration() - 0.5) < 1e-8);
  controller.undoEdit(); REQUIRE(5, !controller.selectedClipHasTransition());
  controller.redoEdit(); REQUIRE(6, controller.selectedClipHasTransition());

  const auto b_end = primary_track(controller).value(QStringLiteral("clips")).toList().back().toMap()
                         .value(QStringLiteral("end_seconds")).toDouble();
  const auto c = controller.agentAddClip(asset_id(controller, QStringLiteral("e6_video_rational.mp4")).toULongLong(),
                                         video_track, b_end, 0.0, 1.0, &problem);
  REQUIRE(7, c.has_value());
  controller.selectTimelineClip(clip_key(controller, QStringLiteral("e6_video_b.mp4")));
  controller.addDipToBlack();
  REQUIRE(8, controller.selectedClipHasTransition() && controller.selectedTransitionType() == QStringLiteral("dip") &&
                 std::abs(controller.selectedTransitionDuration() - 0.5) < 1e-8);
  controller.insertAssetAsOverlay(asset_id(controller, QStringLiteral("still_alpha.png")));
  REQUIRE(9, controller.selectedClipIsOverlay());

  const auto project = root / "transitions.pulsar";
  REQUIRE(10, controller.saveProject(QString::fromStdString(project.string())));
  REQUIRE(11, controller.openProject(QString::fromStdString(project.string())) &&
                  wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  controller.selectTimelineClip(clip_key(controller, QStringLiteral("e6_video_a.mp4")));
  REQUIRE(12, controller.selectedClipHasTransition() && controller.selectedTransitionType() == QStringLiteral("crossfade"));
  REQUIRE(13, controller.agentSeekSeconds(a_end - 0.25, &problem) &&
                  wait_until([&] { return !provider.image().isNull(); }));

  const auto landscape = root / "transitions-landscape.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(landscape.string()), QStringLiteral("720p"));
  REQUIRE(14, wait_until([&] { return !controller.exportRunning(); }, 180'000) && fs::exists(landscape));
  controller.setCanvasPreset(QStringLiteral("portrait"));
  const auto portrait = root / "transitions-portrait.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(portrait.string()), QStringLiteral("720p"));
  REQUIRE(15, wait_until([&] { return !controller.exportRunning(); }, 180'000) && fs::exists(portrait));
  const auto portrait_probe = pulsar::media::ffmpeg::FFmpegMediaProbe{}.probe(portrait);
  REQUIRE(16, portrait_probe.video_streams.front().width == 720 && portrait_probe.video_streams.front().height == 1280);

  const auto evidence = qEnvironmentVariable("PULSAR_TRANSITION_EVIDENCE_DIR");
  if (!evidence.isEmpty()) {
    fs::create_directories(evidence.toStdString());
    provider.image().save(evidence + QStringLiteral("/crossfade-preview.png"));
    fs::copy_file(project, fs::path{evidence.toStdString()} / "transitions.pulsar", fs::copy_options::overwrite_existing);
    fs::copy_file(landscape, fs::path{evidence.toStdString()} / "transitions-landscape.mp4", fs::copy_options::overwrite_existing);
    fs::copy_file(portrait, fs::path{evidence.toStdString()} / "transitions-portrait.mp4", fs::copy_options::overwrite_existing);
  }
  fs::remove_all(root);
  return 0;
}
