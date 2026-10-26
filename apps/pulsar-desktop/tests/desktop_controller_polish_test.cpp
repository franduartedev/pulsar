// Frame step on each clip's own frame grid, duplicate, audio fades and title look through the controller.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QTest>
#include <QTimer>

#include <cmath>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
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
QRect visible_pixel_bounds(const QImage& image) {
  int left = image.width(), top = image.height(), right = -1, bottom = -1;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const auto pixel = image.pixelColor(x, y);
      if (pixel.red() + pixel.green() + pixel.blue() < 48) continue;
      left = std::min(left, x); top = std::min(top, y);
      right = std::max(right, x); bottom = std::max(bottom, y);
    }
  }
  return right >= left && bottom >= top ? QRect{QPoint{left, top}, QPoint{right, bottom}} : QRect{};
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

  // Exercise the requested 1920x1080 project canvas while retaining the small
  // decode fixture. A non-default transform makes the saved fixture v6 so its
  // explicit CANVAS record can be replaced deterministically.
  controller.selectTimelineClip(clip_key(controller, 0));
  controller.setSelectedClipTransform(0.0, 0.0, 0.99, 0.0, 1.0);
  const auto geometry_root = fs::temp_directory_path() / "pulsar-polish-viewer-geometry";
  fs::remove_all(geometry_root); fs::create_directories(geometry_root);
  const auto geometry_project = geometry_root / "viewer-1920x1080.pulsar";
  REQUIRE(180, controller.saveProject(QString::fromStdString(geometry_project.string())));
  std::ifstream input{geometry_project};
  std::string project_text{std::istreambuf_iterator<char>{input}, {}};
  input.close();
  const auto canvas_record = project_text.find("CANVAS 320 180 ");
  REQUIRE(181, canvas_record != std::string::npos);
  project_text.replace(canvas_record, std::string{"CANVAS 320 180 "}.size(), "CANVAS 1920 1080 ");
  // Project files are canonical LF. Binary mode keeps the fixture identical on
  // Windows, where text-mode streams otherwise translate LF to CRLF.
  std::ofstream{geometry_project, std::ios::binary | std::ios::trunc} << project_text;
  REQUIRE(182, controller.openProject(QString::fromStdString(geometry_project.string())));
  REQUIRE(183, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  controller.selectTimelineClip(clip_key(controller, 0));
  controller.setSelectedClipTransform(0.0, 0.0, 1.0, 0.0, 1.0);

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
  viewer_window.resize(960, 540);
  viewer->setParentItem(viewer_window.contentItem());
  viewer->setSize(QSizeF{960.0, 540.0});
  viewer_window.show();
  REQUIRE(189, QTest::qWaitForWindowExposed(&viewer_window));
  viewer_window.requestActivate();
  controller.selectTimelineClip(clip_key(controller, 0));
  REQUIRE(191, wait_until([&] {
    return !controller.previewSource().isEmpty() && controller.selectedClipOnScreen();
  }));
  auto* interaction = find_quick_item(viewer, QStringLiteral("viewerInteractionArea"));
  auto* overlay = find_quick_item(viewer, QStringLiteral("viewerInteractionOverlay"));
  auto* canvas = find_quick_item(viewer, QStringLiteral("viewerCanvasRect"));
  auto* bounds = find_quick_item(viewer, QStringLiteral("viewerSelectionBounds"));
  auto* frame = find_quick_item(viewer, QStringLiteral("viewerFrame"));
  REQUIRE(192, interaction != nullptr && overlay != nullptr && canvas != nullptr && bounds != nullptr &&
                   frame != nullptr && interaction->isVisible() && bounds->isVisible());
  const auto bounds_center = [&] {
    return bounds->mapToScene(QPointF{bounds->width() / 2.0, bounds->height() / 2.0});
  };
  const auto bounds_origin = [&] { return bounds->mapToScene(QPointF{0.0, 0.0}); };
  const auto drag = [&](QPoint start, QPoint delta, int step) {
    QTest::mousePress(&viewer_window, Qt::LeftButton, Qt::NoModifier, start);
    REQUIRE(step, interaction->property("gesture").toString().length() > 0);
    QTest::mouseMove(&viewer_window, start, 10);
    QTest::mouseMove(&viewer_window, start + delta, 20);
    QTest::mouseRelease(&viewer_window, Qt::LeftButton, Qt::NoModifier, start + delta);
    return 0;
  };

  // A — identity visual: exact screen delta maps to canvas-normalized position.
  const auto move_start = bounds_center().toPoint();
  const auto origin_before_move = bounds_origin();
  const auto source_before_move = controller.previewSource();
  QTest::mousePress(&viewer_window, Qt::LeftButton, Qt::NoModifier, move_start);
  REQUIRE(193, interaction->property("gesture").toString() == QStringLiteral("move") &&
                   controller.selectedClipPosX() == 0.0 && controller.selectedClipPosY() == 0.0);
  QTest::mouseMove(&viewer_window, move_start, 10); // no first-event jump
  REQUIRE(194, controller.selectedClipPosX() == 0.0 && controller.selectedClipPosY() == 0.0);
  QTest::mouseMove(&viewer_window, move_start + QPoint{100, 50}, 20);
  const double expected_x = 100.0 / canvas->width();
  const double expected_y = 50.0 / canvas->height();
  REQUIRE(195, wait_until([&] {
    return std::abs(controller.selectedClipPosX() - expected_x) < 1e-6 &&
           std::abs(controller.selectedClipPosY() - expected_y) < 1e-6;
  }));
  REQUIRE(196, std::abs(bounds_origin().x() - origin_before_move.x() - 100.0) < 0.1 &&
                   std::abs(bounds_origin().y() - origin_before_move.y() - 50.0) < 0.1);
  REQUIRE(199, wait_until([&] {
    return !controller.previewSource().isEmpty() && controller.previewSource() != source_before_move &&
           controller.displayedPreviewProjectRevision() == controller.projectRevision();
  }) && interaction->property("pressed").toBool() && interaction->isVisible());

  // Pixel truth, not only model/bounds truth: the production composition frame
  // is black where the translated source no longer covers the canvas, and its
  // coloured bounds agree with the shared selection geometry.
  const auto moved_image = provider->image();
  REQUIRE(237, !moved_image.isNull() && moved_image.pixelColor(0, 0).red() < 16 &&
                   moved_image.pixelColor(moved_image.width() / 2, moved_image.height() / 2).red() > 128);
  const auto moved_pixels = visible_pixel_bounds(moved_image);
  const auto moved_geometry = controller.selectedClipVisualGeometry();
  const int expected_left = std::clamp(static_cast<int>(std::floor(
      (moved_geometry.value("centerX").toDouble() - moved_geometry.value("width").toDouble() / 2.0) * moved_image.width())), 0, moved_image.width() - 1);
  const int expected_top = std::clamp(static_cast<int>(std::floor(
      (moved_geometry.value("centerY").toDouble() - moved_geometry.value("height").toDouble() / 2.0) * moved_image.height())), 0, moved_image.height() - 1);
  REQUIRE(238, std::abs(moved_pixels.left() - expected_left) <= 2 &&
                   std::abs(moved_pixels.top() - expected_top) <= 2);

  // The actual QML Image item has loaded those authoritative pixels too.
  auto moved_grab = frame->grabToImage();
  REQUIRE(239, moved_grab && wait_until([&] { return !moved_grab->image().isNull(); }) &&
                   moved_grab->image().pixelColor(1, 1).red() < 24);
  QTest::mouseRelease(&viewer_window, Qt::LeftButton, Qt::NoModifier,
                      move_start + QPoint{100, 50});
  controller.undoEdit();
  REQUIRE(197, controller.selectedClipPosX() == 0.0 && controller.selectedClipPosY() == 0.0);
  controller.redoEdit();
  REQUIRE(198, std::abs(controller.selectedClipPosX() - expected_x) < 1e-6);
  controller.undoEdit();

  // B — movement is scale-independent and still follows the pointer exactly.
  controller.setSelectedClipTransform(0.0, 0.0, 0.60, 0.0, 1.0);
  REQUIRE(200, wait_until([&] { return std::abs(bounds->width() / canvas->width() - 0.60) < 0.01; }));
  const auto scaled_origin = bounds_origin();
  const auto scaled_start = bounds_center().toPoint();
  REQUIRE(201, drag(scaled_start, QPoint{100, 50}, 201) == 0);
  REQUIRE(202, std::abs(controller.selectedClipPosX() - expected_x) < 1e-6 &&
                   std::abs(controller.selectedClipPosY() - expected_y) < 1e-6 &&
                   std::abs(bounds_origin().x() - scaled_origin.x() - 100.0) < 0.1 &&
                   std::abs(bounds_origin().y() - scaled_origin.y() - 50.0) < 0.1);
  controller.undoEdit();
  REQUIRE(203, controller.selectedClipPosX() == 0.0 && controller.selectedClipPosY() == 0.0 &&
                   std::abs(controller.selectedClipScale() - 0.60) < 1e-9);

  // C–F — radial projection around the renderer's centre anchor makes every
  // corner symmetric. Outward grows; inward shrinks; press alone never jumps.
  struct ScaleCase { int sx; int sy; QPoint delta; bool grows; };
  const std::array<ScaleCase, 4> scale_cases{{
      {-1, -1, {-40, -30}, true}, {1, -1, {-40, 30}, false},
      {-1, 1, {-40, 30}, true}, {1, 1, {-40, -30}, false}}};
  int scale_step = 210;
  bool scale_pixels_checked = false;
  for (const auto& scale_case : scale_cases) {
    controller.setSelectedClipTransform(0.0, 0.0, 0.60, 0.0, 1.0);
    REQUIRE(scale_step++, wait_until([&] { return std::abs(controller.selectedClipScale() - 0.60) < 1e-9; }));
    auto* handle = find_quick_item(viewer, QStringLiteral("viewerScaleHandle_%1_%2")
        .arg(scale_case.sx).arg(scale_case.sy));
    REQUIRE(scale_step++, handle != nullptr && handle->isVisible());
    const auto start_f = handle->mapToScene(QPointF{handle->width() / 2.0, handle->height() / 2.0});
    const auto center_f = bounds_center();
    const QPoint start = start_f.toPoint();
    const double vx = start.x() - center_f.x();
    const double vy = start.y() - center_f.y();
    const double ratio = ((vx + scale_case.delta.x()) * vx +
                          (vy + scale_case.delta.y()) * vy) / (vx * vx + vy * vy);
    const double old_width = bounds->width();
    QTest::mousePress(&viewer_window, Qt::LeftButton, Qt::NoModifier, start);
    REQUIRE(scale_step++, interaction->property("gesture").toString() == QStringLiteral("scale") &&
                              std::abs(controller.selectedClipScale() - 0.60) < 1e-9);
    QTest::mouseMove(&viewer_window, start + scale_case.delta, 20);
    const double expected_scale = 0.60 * ratio;
    REQUIRE(scale_step++, std::abs(controller.selectedClipScale() - expected_scale) < 0.002 &&
                              (scale_case.grows ? bounds->width() > old_width : bounds->width() < old_width));
    QTest::mouseRelease(&viewer_window, Qt::LeftButton, Qt::NoModifier,
                        start + scale_case.delta);
    if (!scale_pixels_checked) {
      REQUIRE(232, wait_until([&] {
        return !controller.previewSource().isEmpty() &&
               controller.displayedPreviewProjectRevision() == controller.projectRevision();
      }));
      const auto scaled_image = provider->image();
      const auto scaled_pixels = visible_pixel_bounds(scaled_image);
      const auto scaled_geometry = controller.selectedClipVisualGeometry();
      const int expected_width = static_cast<int>(std::llround(
          scaled_geometry.value("width").toDouble() * scaled_image.width()));
      const int expected_height = static_cast<int>(std::llround(
          scaled_geometry.value("height").toDouble() * scaled_image.height()));
      REQUIRE(233, !scaled_pixels.isEmpty() &&
                       std::abs(scaled_pixels.width() - expected_width) <= 3 &&
                       std::abs(scaled_pixels.height() - expected_height) <= 3 &&
                       scaled_image.pixelColor(0, 0).red() < 16);
      auto scaled_grab = frame->grabToImage();
      REQUIRE(234, scaled_grab && wait_until([&] { return !scaled_grab->image().isNull(); }) &&
                       scaled_grab->image().pixelColor(1, 1).red() < 24);
      scale_pixels_checked = true;
    }
    controller.undoEdit();
    REQUIRE(scale_step++, std::abs(controller.selectedClipScale() - 0.60) < 1e-9);
  }

  // Crossing the centre anchor clamps to the domain minimum instead of
  // producing a negative, NaN, or mirrored scale.
  controller.setSelectedClipTransform(0.0, 0.0, 0.60, 0.0, 1.0);
  auto* crossing_handle = find_quick_item(viewer, QStringLiteral("viewerScaleHandle_1_1"));
  REQUIRE(235, crossing_handle != nullptr);
  const auto crossing_start = crossing_handle->mapToScene(
      QPointF{crossing_handle->width() / 2.0, crossing_handle->height() / 2.0}).toPoint();
  const auto crossing_center = bounds_center().toPoint();
  const QPoint across_anchor = crossing_center - (crossing_start - crossing_center) / 2;
  QTest::mousePress(&viewer_window, Qt::LeftButton, Qt::NoModifier, crossing_start);
  QTest::mouseMove(&viewer_window, across_anchor, 20);
  QTest::mouseRelease(&viewer_window, Qt::LeftButton, Qt::NoModifier, across_anchor);
  REQUIRE(236, std::isfinite(controller.selectedClipScale()) &&
                   std::abs(controller.selectedClipScale() - 0.01) < 1e-9);
  controller.undoEdit();

  // G–I — resize, letterbox and pillarbox only change Viewer pixels.
  controller.setSelectedClipTransform(0.10, -0.10, 0.60, 0.0, 1.0);
  const double persisted_x = controller.selectedClipPosX();
  const double persisted_y = controller.selectedClipPosY();
  const double persisted_scale = controller.selectedClipScale();
  viewer_window.resize(960, 700); viewer->setSize(QSizeF{960.0, 700.0});
  REQUIRE(240, wait_until([&] { return canvas->height() < overlay->height() - 1.0; }));
  REQUIRE(241, controller.selectedClipPosX() == persisted_x && controller.selectedClipPosY() == persisted_y &&
                   controller.selectedClipScale() == persisted_scale);
  const auto letterbox_center = bounds_center();
  REQUIRE(242, std::abs(letterbox_center.x() - (canvas->mapToScene(QPointF{}).x() + 0.60 * canvas->width())) < 0.1 &&
                   std::abs(letterbox_center.y() - (canvas->mapToScene(QPointF{}).y() + 0.40 * canvas->height())) < 0.1);
  viewer_window.resize(1200, 540); viewer->setSize(QSizeF{1200.0, 540.0});
  REQUIRE(243, wait_until([&] { return canvas->width() < overlay->width() - 1.0; }) &&
                   controller.selectedClipPosX() == persisted_x && controller.selectedClipPosY() == persisted_y &&
                   controller.selectedClipScale() == persisted_scale);

  controller.setSelectedClipTransform(0.0, 0.0, 1.0, 0.0, 1.0);
  controller.seekToEnd();
  REQUIRE(230, wait_until([&] { return !controller.selectedClipOnScreen(); }) &&
                   !interaction->isVisible());
  controller.seekToStart();
  REQUIRE(231, wait_until([&] {
    return controller.selectedClipOnScreen() && interaction->isVisible();
  }));

  // ---- Viewer direct manipulation: pixels are transient, live updates coalesce ---------
  controller.selectTimelineClip(clip_key(controller, 0));
  const auto geometry = controller.selectedClipVisualGeometry();
  REQUIRE(300, controller.selectedClipOnScreen() && geometry.value("width").toDouble() > 0.0 &&
                   geometry.value("height").toDouble() > 0.0);
  controller.beginSelectedClipTransform();
  controller.updateSelectedClipTranslationFromViewer(0.0, 0.0, 40.0, 20.0,
                                                       1000.0, 500.0, 1.0, 0.0, 1.0);
  controller.updateSelectedClipTranslationFromViewer(0.0, 0.0, 100.0, 50.0,
                                                       1000.0, 500.0, 1.0, 0.0, 1.0);
  controller.endSelectedClipTransform();
  REQUIRE(301, std::abs(controller.selectedClipPosX() - 0.1) < 1e-9 &&
                   std::abs(controller.selectedClipPosY() - 0.1) < 1e-9);
  controller.undoEdit();
  REQUIRE(302, controller.selectedClipPosX() == 0.0 && controller.selectedClipPosY() == 0.0);
  controller.redoEdit();
  REQUIRE(303, std::abs(controller.selectedClipPosX() - 0.1) < 1e-9 &&
                   std::abs(controller.selectedClipPosY() - 0.1) < 1e-9);
  controller.undoEdit();

  // The same canvas-relative movement survives a doubled Viewer size.
  controller.beginSelectedClipTransform();
  controller.updateSelectedClipTranslationFromViewer(0.0, 0.0, 200.0, 100.0,
                                                       2000.0, 1000.0, 1.0, 0.0, 1.0);
  controller.endSelectedClipTransform();
  REQUIRE(304, std::abs(controller.selectedClipPosX() - 0.1) < 1e-9 &&
                   std::abs(controller.selectedClipPosY() - 0.1) < 1e-9);
  controller.undoEdit();

  // Corner-scale live updates are likewise one logical history entry.
  controller.beginSelectedClipTransform();
  controller.updateSelectedClipTransform(0.0, 0.0, 1.2, 0.0, 1.0);
  controller.updateSelectedClipTransform(0.0, 0.0, 1.5, 0.0, 1.0);
  controller.endSelectedClipTransform();
  REQUIRE(305, std::abs(controller.selectedClipScale() - 1.5) < 1e-9);
  controller.undoEdit();
  REQUIRE(306, controller.selectedClipScale() == 1.0);

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
  fs::remove_all(geometry_root);
  return 0;
}
