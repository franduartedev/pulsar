// Vertical creator project: canvas presets, cover framing, overlay/title/music,
// persistence and portrait export all exercise the desktop controller.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>

#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>

namespace {
namespace fs = std::filesystem;
using pulsar::domain::MediaTime;

bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 90'000) {
  if (predicate()) return true;
  QEventLoop loop;
  QTimer poll, timeout;
  poll.setInterval(10);
  QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (predicate()) loop.quit(); });
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  poll.start();
  timeout.setSingleShot(true);
  timeout.start(timeout_ms);
  loop.exec();
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

pulsar::media::ffmpeg::FFmpegPreviewFrame first_frame(const fs::path& path) {
  pulsar::media::ffmpeg::FFmpegVideoDecodeSession session{path, 0};
  auto frame = session.decode_next_visual();
  return pulsar::media::ffmpeg::FFmpegPreviewConverter{}.convert(*frame);
}
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { \
  std::fprintf(stderr, "SOCIAL step %d failed: %s | error='%s' export='%s'\n", step, #condition, \
               controller.errorMessage().toUtf8().constData(), controller.exportStatus().toUtf8().constData()); \
  return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider provider;
  DesktopController controller{provider, {}};
  const auto source = fs::path{PULSAR_SOURCE_DIR};
  const auto fixtures = source / "tests/fixtures";
  const auto root = fs::temp_directory_path() /
      ("pulsar-social-test-" + std::to_string(QCoreApplication::applicationPid()));
  fs::remove_all(root);
  fs::create_directories(root);

  controller.openPath(QString::fromStdString((fixtures / "e6_video_a.mp4").string()));
  REQUIRE(1, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  controller.openPaths({QString::fromStdString((fixtures / "still_alpha.png").string()),
                        QString::fromStdString((source / "assets/sounds/pulse.wav").string())});
  REQUIRE(2, wait_until([&] { return controller.mediaAssets()->rowCount() == 3 && !controller.busy(); }));

  // Every creator canvas is selectable; portrait is the project used below.
  controller.setCanvasPreset(QStringLiteral("landscape"));
  REQUIRE(3, controller.canvasPreset() == QStringLiteral("landscape") &&
                 std::abs(controller.viewerCanvasAspectRatio() - 16.0 / 9.0) < 1e-8);
  controller.setCanvasPreset(QStringLiteral("square"));
  REQUIRE(4, controller.canvasPreset() == QStringLiteral("square") && controller.viewerCanvasPixelWidth() == 1080);
  controller.setCanvasPreset(QStringLiteral("portrait45"));
  REQUIRE(5, controller.canvasPreset() == QStringLiteral("portrait45") && controller.viewerCanvasPixelHeight() == 1350);
  controller.setCanvasPreset(QStringLiteral("portrait"));
  REQUIRE(6, controller.canvasPreset() == QStringLiteral("portrait") &&
                 controller.viewerCanvasPixelWidth() == 1080 && controller.viewerCanvasPixelHeight() == 1920);

  // Horizontal source: Fill covers portrait, manual framing remains undoable.
  controller.selectTimelineClip(clip_key(controller, QStringLiteral("e6_video_a.mp4")));
  controller.setSelectedClipFitMode(QStringLiteral("fill"));
  REQUIRE(7, controller.selectedClipFitMode() == QStringLiteral("fill"));
  controller.setSelectedClipTransform(0.12, -0.08, 1.05, 0.0, 1.0);
  REQUIRE(8, std::abs(controller.selectedClipPosX() - 0.12) < 1e-8);
  controller.undoEdit();
  REQUIRE(9, std::abs(controller.selectedClipPosX()) < 1e-8 && controller.selectedClipFitMode() == QStringLiteral("fill"));
  controller.redoEdit();
  REQUIRE(10, std::abs(controller.selectedClipPosX() - 0.12) < 1e-8);

  // Animated transforms are edited at the playhead, not into an unused base
  // transform. The inspector therefore reports the evaluated value.
  controller.seekToStart();
  controller.setSelectedClipVisualKeyframe(0);
  controller.seekNormalized(0.5);
  controller.setSelectedClipTransform(-0.20, 0.10, 0.80, 25.0, 0.70);
  REQUIRE(101, controller.selectedClipVisualKeyframes().size() == 2 &&
                   std::abs(controller.selectedClipPosX() + 0.20) < 1e-8 &&
                   std::abs(controller.selectedClipScale() - 0.80) < 1e-8);
  controller.setSelectedClipVisualKeyframeInterpolation(1, 2);
  REQUIRE(102, controller.selectedClipVisualKeyframes().at(1).toMap().value(QStringLiteral("interpolation")).toInt() == 2);
  controller.removeSelectedClipVisualKeyframe(1);
  REQUIRE(103, controller.selectedClipVisualKeyframes().size() == 1);

  controller.insertAssetAsOverlay(asset_id(controller, QStringLiteral("still_alpha.png")));
  REQUIRE(11, controller.selectedClipIsOverlay());
  controller.setSelectedClipTransform(0.0, -0.38, 1.0, 0.0, 0.65);
  controller.seekToStart();
  controller.addTitle();
  controller.setSelectedTitleText(QStringLiteral("ZONALERO · REEL"));
  controller.insertAssetAtPlayhead(asset_id(controller, QStringLiteral("pulse.wav")));
  REQUIRE(12, controller.timelineTracks().size() >= 3);

  const auto project = root / "zonalero-reel.pulsar";
  REQUIRE(13, controller.saveProject(QString::fromStdString(project.string())));
  REQUIRE(14, controller.openProject(QString::fromStdString(project.string())));
  REQUIRE(15, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }) &&
                  controller.canvasPreset() == QStringLiteral("portrait") &&
                  controller.timelineTitles()->rowCount() == 1);
  controller.selectTimelineClip(clip_key(controller, QStringLiteral("e6_video_a.mp4")));
  REQUIRE(16, controller.selectedClipFitMode() == QStringLiteral("fill") &&
                  std::abs(controller.selectedClipPosX() - 0.12) < 1e-8);

  controller.seekToStart();
  REQUIRE(17, wait_until([&] { return !provider.image().isNull() &&
                                       provider.image().width() == 1080 && provider.image().height() == 1920; }));
  const QImage preview = provider.image();
  REQUIRE(18, preview.pixelColor(540, 80).alpha() == 255); // portrait canvas is fully composed

  const auto output = root / "zonalero-reel-720.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(output.string()), QStringLiteral("720p"));
  REQUIRE(19, wait_until([&] { return !controller.exportRunning(); }, 180'000) && fs::exists(output));
  REQUIRE(20, controller.exportStatus() == QStringLiteral("Exportación completada"));
  const auto exported = first_frame(output);
  REQUIRE(21, exported.width == 720 && exported.height == 1280);
  const auto exported_media = pulsar::media::ffmpeg::FFmpegMediaProbe{}.probe(output);
  REQUIRE(22, !exported_media.audio_streams.empty());

  const auto evidence = qEnvironmentVariable("PULSAR_SOCIAL_EVIDENCE_DIR");
  if (!evidence.isEmpty()) {
    fs::create_directories(evidence.toStdString());
    preview.save(evidence + QStringLiteral("/zonalero-reel-preview.png"));
    fs::copy_file(project, fs::path{evidence.toStdString()} / "zonalero-reel.pulsar", fs::copy_options::overwrite_existing);
    fs::copy_file(output, fs::path{evidence.toStdString()} / "zonalero-reel-720.mp4", fs::copy_options::overwrite_existing);
  }
  fs::remove_all(root);
  return 0;
}
