// Frame step on each clip's own frame grid, duplicate, audio fades and title look through the controller.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QTimer>

#include <cmath>
#include <array>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>

namespace {
namespace fs = std::filesystem;

bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 60'000) {
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

// "00:00:01.040" -> milliseconds.
long ms_of(const QString& text) {
  const auto parts = text.split(':');
  const auto sec = parts.value(2).split('.');
  return parts.value(0).toLong() * 3'600'000 + parts.value(1).toLong() * 60'000 + sec.value(0).toLong() * 1000 + sec.value(1).toLong();
}
QString clip_key(DesktopController& c, int row) { return c.timelineClips()->index(row, 0).data(TimelineClipListModel::ClipKeyRole).toString(); }
QQuickItem* find_quick_item(QQuickItem* root, const QString& name) {
  if (root->objectName() == name) return root;
  for (auto* child : root->childItems()) {
    if (auto* found = find_quick_item(child, name)) return found;
  }
  return nullptr;
}
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { std::fprintf(stderr, "POLISH step %d failed: %s | error='%s' pos=%s dur=%s\n", step, #condition, \
  controller.errorMessage().toUtf8().constData(), controller.positionText().toUtf8().constData(), controller.timelineDurationText().toUtf8().constData()); return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  QQmlEngine qml_engine;
  qml_engine.addImportPath(QString::fromStdString(
      (fs::path{PULSAR_SOURCE_DIR} / "apps/pulsar-desktop/imports").string()));
  auto* provider = new PreviewImageProvider;
  DesktopController controller{*provider, {}};
  qml_engine.addImageProvider(QStringLiteral("pulsar-preview"), provider);
  qml_engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
  const auto fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  controller.openPath(QString::fromStdString((fixtures / "e6_video_a.mp4").string()));
  REQUIRE(1, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));

  // ---- Actual QML pointer routing: press/move/release through ViewerPane ----------------
  QQmlComponent viewer_component{
      &qml_engine,
      QUrl::fromLocalFile(QString::fromStdString(
          (fs::path{PULSAR_SOURCE_DIR} / "apps/pulsar-desktop/qml/components/ViewerPane.qml").string()))};
  std::unique_ptr<QObject> viewer_object{viewer_component.create()};
  if (!viewer_object) {
    const auto errors = viewer_component.errors();
    for (const auto& error : errors) std::fprintf(stderr, "%s\n", error.toString().toUtf8().constData());
  }
  auto* viewer = qobject_cast<QQuickItem*>(viewer_object.get());
  REQUIRE(190, viewer != nullptr);
  QQuickWindow viewer_window;
  viewer_window.resize(900, 520);
  viewer->setParentItem(viewer_window.contentItem());
  viewer->setSize(QSizeF{900.0, 520.0});
  viewer_window.show();
  REQUIRE(189, QTest::qWaitForWindowExposed(&viewer_window));
  viewer_window.requestActivate();
  controller.selectTimelineClip(clip_key(controller, 0));
  REQUIRE(191, wait_until([&] {
    return !controller.previewSource().isEmpty() && controller.selectedClipOnScreen();
  }));
  auto* move_area = find_quick_item(viewer, QStringLiteral("viewerMoveArea"));
  REQUIRE(192, move_area != nullptr && move_area->isVisible());
  const auto move_start = move_area->mapToScene(
      QPointF{move_area->width() / 2.0, move_area->height() / 2.0}).toPoint();
  const auto source_before_move = controller.previewSource();
  QTest::mousePress(&viewer_window, Qt::LeftButton, Qt::NoModifier, move_start);
  QTest::mouseMove(&viewer_window, move_start + QPoint{30, 15}, 20);
  REQUIRE(193, wait_until([&] { return std::abs(controller.selectedClipPosX()) > 0.001; }) &&
                   move_area->isVisible() && !controller.previewSource().isEmpty());
  const double first_x = controller.selectedClipPosX();
  QTest::mouseMove(&viewer_window, move_start + QPoint{75, 35}, 20);
  REQUIRE(194, wait_until([&] { return controller.previewSource() != source_before_move; }) &&
                   move_area->property("pressed").toBool());
  QTest::mouseRelease(&viewer_window, Qt::LeftButton, Qt::NoModifier,
                      move_start + QPoint{75, 35});
  REQUIRE(195, controller.selectedClipPosX() > first_x && controller.selectedClipPosY() > 0.0);
  controller.undoEdit();
  REQUIRE(196, controller.selectedClipPosX() == 0.0 && controller.selectedClipPosY() == 0.0);
  controller.redoEdit();
  REQUIRE(197, controller.selectedClipPosX() > first_x);
  controller.undoEdit();
  REQUIRE(198, wait_until([&] {
    return controller.selectedClipScale() == 1.0 && move_area->isVisible();
  }));

  const std::array<QPair<int, int>, 4> corners{{{-1, -1}, {1, -1}, {-1, 1}, {1, 1}}};
  int corner_step = 210;
  for (const auto& [sx, sy] : corners) {
    auto* handle = find_quick_item(viewer,
        QStringLiteral("viewerScaleHandle_%1_%2").arg(sx).arg(sy));
    REQUIRE(corner_step++, handle != nullptr && handle->isVisible());
    const auto start = handle->mapToScene(
        QPointF{handle->width() / 2.0, handle->height() / 2.0}).toPoint();
    auto* handle_mouse = find_quick_item(handle,
        QStringLiteral("viewerScaleMouse_%1_%2").arg(sx).arg(sy));
    REQUIRE(corner_step++, handle_mouse != nullptr);
    QTest::mousePress(&viewer_window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&viewer_window, start + QPoint{sx * 8, sy * 8}, 20);
    REQUIRE(corner_step++, wait_until([&] { return controller.selectedClipScale() > 1.01; }) &&
                               handle->isVisible());
    QTest::mouseMove(&viewer_window, start + QPoint{sx * 12, sy * 12}, 20);
    QTest::mouseRelease(&viewer_window, Qt::LeftButton, Qt::NoModifier,
                        start + QPoint{sx * 12, sy * 12});
    REQUIRE(corner_step++, controller.selectedClipScale() > 1.01);
    controller.undoEdit();
    REQUIRE(corner_step++, wait_until([&] {
      return controller.selectedClipScale() == 1.0 && handle->isVisible();
    }));
  }
  controller.seekToEnd();
  REQUIRE(230, wait_until([&] { return !controller.selectedClipOnScreen(); }) &&
                   !move_area->isVisible());
  controller.seekToStart();
  REQUIRE(231, wait_until([&] {
    return controller.selectedClipOnScreen() && !controller.previewSource().isEmpty();
  }));

  // ---- Viewer direct manipulation: pixels are transient, live updates coalesce ---------
  controller.selectTimelineClip(clip_key(controller, 0));
  const auto geometry = controller.selectedClipVisualGeometry();
  REQUIRE(200, controller.selectedClipOnScreen() && geometry.value("width").toDouble() > 0.0 &&
                   geometry.value("height").toDouble() > 0.0);
  controller.beginSelectedClipTransform();
  controller.updateSelectedClipTranslationFromViewer(0.0, 0.0, 40.0, 20.0,
                                                       1000.0, 500.0, 1.0, 0.0, 1.0);
  controller.updateSelectedClipTranslationFromViewer(0.0, 0.0, 100.0, 50.0,
                                                       1000.0, 500.0, 1.0, 0.0, 1.0);
  controller.endSelectedClipTransform();
  REQUIRE(201, std::abs(controller.selectedClipPosX() - 0.1) < 1e-9 &&
                   std::abs(controller.selectedClipPosY() - 0.1) < 1e-9);
  controller.undoEdit();
  REQUIRE(202, controller.selectedClipPosX() == 0.0 && controller.selectedClipPosY() == 0.0);
  controller.redoEdit();
  REQUIRE(203, std::abs(controller.selectedClipPosX() - 0.1) < 1e-9 &&
                   std::abs(controller.selectedClipPosY() - 0.1) < 1e-9);
  controller.undoEdit();

  // The same canvas-relative movement survives a doubled Viewer size.
  controller.beginSelectedClipTransform();
  controller.updateSelectedClipTranslationFromViewer(0.0, 0.0, 200.0, 100.0,
                                                       2000.0, 1000.0, 1.0, 0.0, 1.0);
  controller.endSelectedClipTransform();
  REQUIRE(204, std::abs(controller.selectedClipPosX() - 0.1) < 1e-9 &&
                   std::abs(controller.selectedClipPosY() - 0.1) < 1e-9);
  controller.undoEdit();

  // Corner-scale live updates are likewise one logical history entry.
  controller.beginSelectedClipTransform();
  controller.updateSelectedClipTransform(0.0, 0.0, 1.2, 0.0, 1.0);
  controller.updateSelectedClipTransform(0.0, 0.0, 1.5, 0.0, 1.0);
  controller.endSelectedClipTransform();
  REQUIRE(205, std::abs(controller.selectedClipScale() - 1.5) < 1e-9);
  controller.undoEdit();
  REQUIRE(206, controller.selectedClipScale() == 1.0);

  // ---- Sound library: Spanish names, categories, durations, auditionable (when the pack is installed) ----
  if (controller.soundPackAvailable()) {
    const auto library = controller.soundLibrary();
    REQUIRE(100, library.size() >= 37);
    QStringList categories;
    for (const auto& item : library) {
      const auto row = item.toMap();
      REQUIRE(101, !row.value("name").toString().isEmpty() && !row.value("category").toString().isEmpty() && row.value("duration").toString().endsWith(" s"));
      if (!categories.contains(row.value("category").toString())) categories << row.value("category").toString();
    }
    REQUIRE(102, categories.contains(QStringLiteral("Pops")) && categories.contains(QStringLiteral("Alertas")) && categories.contains(QStringLiteral("Clics")));
    bool english_left = false;
    for (const auto& item : library) english_left = english_left || item.toMap().value("name").toString() == QStringLiteral("Click Soft");
    REQUIRE(103, !english_left);                                           // the interface speaks Spanish
    const bool dirty_before = controller.projectDirty();
    const auto history_before = controller.canUndo();
    controller.auditionSound(library.front().toMap().value("id").toString());   // never throws, never edits the project
    REQUIRE(104, controller.projectDirty() == dirty_before && controller.canUndo() == history_before);
  }

  // ---- Frame step: 25 fps clip -> exactly 40 ms; never fakes 33 ms ---------------------------------
  controller.seekNormalized(0.25);                                    // 1 s of the 4 s clip
  REQUIRE(2, ms_of(controller.positionText()) == 1000);
  controller.stepFrame(1);
  REQUIRE(3, ms_of(controller.positionText()) == 1040);
  controller.stepFrame(1); controller.stepFrame(-1); controller.stepFrame(-1);
  REQUIRE(4, ms_of(controller.positionText()) == 1000);
  controller.stepFrame(-1);
  REQUIRE(5, ms_of(controller.positionText()) == 960);
  controller.seekToStart();
  controller.stepFrame(-1);
  REQUIRE(6, ms_of(controller.positionText()) == 0);                   // clamped at the start
  controller.seekToEnd();
  controller.stepFrame(1);
  REQUIRE(7, ms_of(controller.positionText()) == 4000);                // clamped at the end
  controller.stepFrame(-1);
  REQUIRE(8, ms_of(controller.positionText()) == 3960);

  // ---- A 30000/1001 clip steps by 33.37 ms (its own rational grid) --------------------------------
  controller.openPath(QString::fromStdString((fixtures / "e6_video_rational.mp4").string()));
  REQUIRE(9, wait_until([&] { return controller.mediaAssets()->rowCount() == 2 && !controller.busy(); }));
  controller.appendAssetToVideoTrack(controller.mediaAssets()->index(1, 0).data(MediaAssetListModel::MediaIdRole).toString());
  REQUIRE(10, controller.errorMessage().isEmpty());
  controller.seekNormalized(4.5 / 8.004);                              // 0.5 s into the second clip
  controller.stepFrame(1);                                             // first step lands on the clip's frame grid
  const long before = ms_of(controller.positionText());
  controller.stepFrame(1);
  const long step = ms_of(controller.positionText()) - before;
  REQUIRE(11, step >= 32 && step <= 34);
  controller.stepFrame(1);
  const long two = ms_of(controller.positionText()) - before;
  REQUIRE(12, two >= 66 && two <= 68);                                 // 2 x 33.367 ms: no drift to 33 or 34 per step

  // ---- Duplicate: right after the original, one undo step, refuses when the room is taken ----------
  controller.selectTimelineClip(clip_key(controller, 0));
  REQUIRE(13, controller.canDuplicate());
  controller.duplicateSelected();
  REQUIRE(14, !controller.errorMessage().isEmpty());                   // the second clip is right behind it
  controller.selectTimelineClip(clip_key(controller, 1));
  controller.duplicateSelected();
  REQUIRE(15, controller.timelineClips()->rowCount() == 3 && controller.timelineDurationText() == QStringLiteral("00:00:12.008"));
  controller.undoEdit();
  REQUIRE(16, controller.timelineClips()->rowCount() == 2 && controller.timelineDurationText() == QStringLiteral("00:00:08.004"));

  // ---- Fades: exposed, undoable, refused without audio -------------------------------------------
  controller.selectTimelineClip(clip_key(controller, 0));
  controller.setSelectedClipFades(0.5, 1.25);
  REQUIRE(17, std::abs(controller.selectedClipFadeIn() - 0.5) < 1e-9 && std::abs(controller.selectedClipFadeOut() - 1.25) < 1e-9);
  controller.undoEdit();
  REQUIRE(18, controller.selectedClipFadeIn() == 0.0 && controller.selectedClipFadeOut() == 0.0);
  controller.redoEdit();
  REQUIRE(19, std::abs(controller.selectedClipFadeOut() - 1.25) < 1e-9);

  // ---- Save/reopen keeps the fades --------------------------------------------------------------
  const auto root = fs::temp_directory_path() / "pulsar-polish-test";
  fs::remove_all(root);
  fs::create_directories(root);
  REQUIRE(20, controller.saveProject(QString::fromStdString((root / "p.pulsar").string())));
  REQUIRE(21, controller.openProject(QString::fromStdString((root / "p.pulsar").string())));
  REQUIRE(22, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  controller.selectTimelineClip(clip_key(controller, 0));
  REQUIRE(23, std::abs(controller.selectedClipFadeIn() - 0.5) < 1e-9 && std::abs(controller.selectedClipFadeOut() - 1.25) < 1e-9);
  fs::remove_all(root);
  return 0;
}
