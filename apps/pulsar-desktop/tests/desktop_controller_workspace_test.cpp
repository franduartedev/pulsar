// Human polish fix 04: Timeline drag/autoscroll lifecycle and the Viewer/Timeline
// split. These drive real QML items through a real QQuickWindow so the pointer
// routing (grabs, release, cancel, pointer exit) is exercised, not stubbed.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"
#include "../src/thumbnail_service.hpp"
#include "../src/timeline_clip_list_model.hpp"
#include "../src/waveform_quick_item.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QTimer>
#include <qqml.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>

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

QString clip_key(DesktopController& c, int row) {
  return c.timelineClips()->index(row, 0).data(TimelineClipListModel::ClipKeyRole).toString();
}

QQuickItem* find_quick_item(QQuickItem* root, const QString& name) {
  if (root->objectName() == name) return root;
  for (auto* child : root->childItems()) {
    if (auto* found = find_quick_item(child, name)) return found;
  }
  return nullptr;
}

std::string read_bytes(const fs::path& path) {
  std::ifstream input{path, std::ios::binary};
  return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

double prop_real(QObject* object, const char* name) {
  return object->property(name).toDouble();
}
bool prop_bool(QObject* object, const char* name) {
  return object->property(name).toBool();
}

std::unique_ptr<QObject> create_qml(QQmlEngine& engine, const QString& file, const char* tag) {
  QQmlComponent component{&engine, QUrl::fromLocalFile(QString::fromStdString(
      (fs::path{PULSAR_SOURCE_DIR} / "apps/pulsar-desktop/qml" / file.toStdString()).string()))};
  auto object = std::unique_ptr<QObject>{component.create()};
  if (!object) {
    for (const auto& error : component.errors())
      std::fprintf(stderr, "%s: %s\n", tag, error.toString().toUtf8().constData());
  }
  return object;
}
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { \
  std::fprintf(stderr, "WORKSPACE step %d failed: %s | error='%s'\n", step, #condition, \
               controller.errorMessage().toUtf8().constData()); return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  QQmlEngine qml_engine;
  qml_engine.addImportPath(QString::fromStdString(
      (fs::path{PULSAR_SOURCE_DIR} / "apps/pulsar-desktop/imports").string()));
  qmlRegisterType<WaveformQuickItem>("Pulsar.Desktop", 1, 0, "WaveformItem");

  auto* preview = new PreviewImageProvider;
  DesktopController controller{*preview, {}};
  qml_engine.addImageProvider(QStringLiteral("pulsar-preview"), preview);
  // This test owns three short-lived windows and does not assert thumbnail
  // pixels. Do not start asynchronous thumbnail image requests here: Qt 6.4's
  // global QQuickPixmap cache retains the final 160x90 texture until process
  // teardown, obscuring LeakSanitizer coverage of the application objects this
  // test is intended to exercise. Thumbnail decoding/cache behavior has its
  // own integration and sanitizer coverage.
  qml_engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);

  const auto fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  controller.openPath(QString::fromStdString((fixtures / "e6_video_a.mp4").string()));
  REQUIRE(1, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  controller.selectTimelineClip(clip_key(controller, 0));
  REQUIRE(2, controller.selectedClipOnScreen());

  // =============================== Part A: Viewer canvas fit ===============================
  auto viewer_object = create_qml(qml_engine, QStringLiteral("components/ViewerPane.qml"), "ViewerPane");
  auto* viewer = qobject_cast<QQuickItem*>(viewer_object.get());
  REQUIRE(10, viewer != nullptr);
  QQuickWindow viewer_window;
  viewer_window.resize(800, 500);
  viewer->setParentItem(viewer_window.contentItem());
  viewer->setSize(QSizeF{800.0, 500.0});
  viewer_window.show();
  REQUIRE(11, QTest::qWaitForWindowExposed(&viewer_window));
  QTest::qWait(60);

  auto* overlay = find_quick_item(viewer, QStringLiteral("viewerInteractionOverlay"));
  auto* frame = find_quick_item(viewer, QStringLiteral("viewerFrame"));
  REQUIRE(12, overlay != nullptr && frame != nullptr && viewer->width() > 0);

  const double canvas_aspect = controller.viewerCanvasAspectRatio();
  REQUIRE(13, std::abs(canvas_aspect - 16.0 / 9.0) < 0.01);
  const double project_pixel_width = controller.viewerCanvasPixelWidth();
  const double project_pixel_height = controller.viewerCanvasPixelHeight();
  REQUIRE(14, project_pixel_width > 0 && project_pixel_height > 0);

  const auto canvas_width = [&] { return prop_real(overlay, "canvasWidth"); };
  const auto canvas_height = [&] { return prop_real(overlay, "canvasHeight"); };
  const auto canvas_x = [&] { return prop_real(overlay, "canvasX"); };
  const auto canvas_y = [&] { return prop_real(overlay, "canvasY"); };
  const auto ov_width = [&] { return overlay->width(); };
  const auto ov_height = [&] { return overlay->height(); };
  const auto fits_and_touches = [&] {
    const bool within = canvas_width() <= ov_width() + 1.5 && canvas_height() <= ov_height() + 1.5;
    const bool touches = std::abs(canvas_width() - ov_width()) < 1.5 ||
                         std::abs(canvas_height() - ov_height()) < 1.5;
    const bool centered = std::abs(canvas_x() - (ov_width() - canvas_width()) / 2.0) < 1.5 &&
                          std::abs(canvas_y() - (ov_height() - canvas_height()) / 2.0) < 1.5;
    const bool aspect = std::abs(canvas_width() / std::max(1.0, canvas_height()) - canvas_aspect) < 0.01;
    return within && touches && centered && aspect;
  };
  const auto frame_matches_canvas = [&] {
    return std::abs(frame->x() - canvas_x()) < 1.5 && std::abs(frame->y() - canvas_y()) < 1.5 &&
           std::abs(frame->width() - canvas_width()) < 1.5 &&
           std::abs(frame->height() - canvas_height()) < 1.5;
  };

  // A. A wide Viewer: the canvas touches a bound, keeps 16:9, is centred, and
  // the picture is placed exactly on that rect (never stretched).
  controller.setSelectedClipTransform(0.1, -0.2, 0.8, 0.0, 1.0);
  const double pos_x_before = controller.selectedClipPosX();
  const double pos_y_before = controller.selectedClipPosY();
  const double scale_before = controller.selectedClipScale();

  REQUIRE(15, ov_width() > 100.0 && ov_height() > 100.0);
  REQUIRE(16, fits_and_touches());
  REQUIRE(17, frame_matches_canvas());
  REQUIRE(18, std::abs(frame->width() / std::max(1.0, frame->height()) - canvas_aspect) < 0.02);

  // B. A tall, narrow Viewer: still fits, still 16:9, still centred.
  viewer->setSize(QSizeF{300.0, 900.0});
  QTest::qWait(40);
  REQUIRE(19, fits_and_touches());
  REQUIRE(20, std::abs(canvas_width() - ov_width()) < 1.5);   // width bound here
  REQUIRE(21, frame_matches_canvas());

  // C. More Viewer height grows the canvas (height bound while width allows it).
  viewer->setSize(QSizeF{900.0, 320.0});
  QTest::qWait(40);
  const double small_w = canvas_width();
  const double small_h = canvas_height();
  REQUIRE(22, fits_and_touches());
  viewer->setSize(QSizeF{900.0, 460.0});
  QTest::qWait(40);
  REQUIRE(23, canvas_height() > small_h + 40.0);
  REQUIRE(24, canvas_width() > small_w + 40.0);
  REQUIRE(25, std::abs(canvas_width() / canvas_height() - canvas_aspect) < 0.01);

  // D. Resizing the Viewer must never touch the persistent clip transform.
  viewer->setSize(QSizeF{420.0, 260.0});
  QTest::qWait(40);
  viewer->setSize(QSizeF{900.0, 620.0});
  QTest::qWait(40);
  REQUIRE(26, std::abs(controller.selectedClipPosX() - pos_x_before) < 1e-9);
  REQUIRE(27, std::abs(controller.selectedClipPosY() - pos_y_before) < 1e-9);
  REQUIRE(28, std::abs(controller.selectedClipScale() - scale_before) < 1e-9);

  // E. "100%" shows one project pixel per screen pixel, still 16:9 and exact.
  viewer->setProperty("fitMode", QStringLiteral("actual"));
  QTest::qWait(40);
  REQUIRE(29, std::abs(canvas_width() - project_pixel_width) < 1.5);
  REQUIRE(30, std::abs(canvas_height() - project_pixel_height) < 1.5);
  REQUIRE(31, std::abs(canvas_width() / canvas_height() - canvas_aspect) < 0.01);
  REQUIRE(32, frame_matches_canvas());
  viewer->setProperty("fitMode", QStringLiteral("fit"));
  QTest::qWait(40);
  REQUIRE(33, fits_and_touches());
  REQUIRE(34, frame_matches_canvas());

  // =========================== Part B: Timeline drag + autoscroll ==========================
  auto timeline_object = create_qml(qml_engine, QStringLiteral("components/TimelinePane.qml"), "TimelinePane");
  auto* timeline = qobject_cast<QQuickItem*>(timeline_object.get());
  REQUIRE(40, timeline != nullptr);
  QQuickWindow timeline_window;
  timeline_window.resize(1400, 600);
  timeline->setParentItem(timeline_window.contentItem());
  timeline->setPosition(QPointF{0.0, 0.0});
  timeline->setSize(QSizeF{1200.0, 460.0});
  timeline_window.show();
  REQUIRE(41, QTest::qWaitForWindowExposed(&timeline_window));

  controller.setTimelineZoom(6.0);
  controller.setTimelineScroll(0.0);
  QTest::qWait(80);

  const auto timeline_key = clip_key(controller, 0);
  auto* block = find_quick_item(timeline, QStringLiteral("timelineClip_") + timeline_key);
  REQUIRE(42, block != nullptr && block->isVisible());
  auto* body = find_quick_item(block, QStringLiteral("clipBody"));
  REQUIRE(43, body != nullptr);

  auto* edge_timer = timeline->findChild<QObject*>(QStringLiteral("timelineEdgeScrollTimer"));
  REQUIRE(44, edge_timer != nullptr);

  const double pane_width = timeline->width();
  const double press_local_x = 150.0;
  const double press_local_y = std::max(20.0, std::min(block->height() - 20.0, block->height() / 2.0));
  const auto scene_of = [&](double x, double y) {
    return timeline->mapToScene(QPointF{x, y}).toPoint();
  };
  const auto press_block = [&](double local_x, double local_y) {
    return block->mapToScene(QPointF{local_x, local_y}).toPoint();
  };

  // --- Scenario 1: vertical movement inside is legitimate; release anywhere ends the drag ---
  controller.setTimelineScroll(0.0);
  QTest::qWait(40);
  QTest::mousePress(&timeline_window, Qt::LeftButton, Qt::NoModifier,
                    press_block(press_local_x, press_local_y));
  REQUIRE(45, prop_bool(timeline, "dragActive"));
  REQUIRE(46, prop_bool(timeline, "dragInside"));

  // Dragging across lanes (vertical) while inside the Timeline is preserved.
  QTest::mouseMove(&timeline_window, scene_of(pane_width * 0.3, 120.0), 10);
  QTest::qWait(20);
  REQUIRE(47, prop_bool(timeline, "dragActive") && prop_bool(timeline, "dragInside"));
  QTest::mouseMove(&timeline_window, scene_of(pane_width * 0.3, 300.0), 10);
  QTest::qWait(20);
  REQUIRE(48, prop_bool(timeline, "dragActive") && prop_bool(timeline, "dragInside"));

  // Release outside the Timeline: the gesture ends, nothing keeps running.
  QTest::mouseMove(&timeline_window, scene_of(pane_width + 80.0, 300.0), 10);
  QTest::qWait(20);
  REQUIRE(49, !prop_bool(timeline, "dragInside"));
  QTest::mouseRelease(&timeline_window, Qt::LeftButton, Qt::NoModifier,
                      scene_of(pane_width + 80.0, 300.0));
  QTest::qWait(20);
  REQUIRE(50, !prop_bool(timeline, "dragActive"));
  REQUIRE(51, !prop_bool(edge_timer, "running"));
  REQUIRE(52, timeline->property("activeDragBlock").isNull());

  // --- Scenario 2: autoscroll starts at the edge, stops on exit, ends on cancel ---
  controller.setTimelineScroll(0.0);
  QTest::qWait(60);
  REQUIRE(53, wait_until([&] { return block->isVisible(); }));
  QTest::mousePress(&timeline_window, Qt::LeftButton, Qt::NoModifier,
                    press_block(press_local_x, press_local_y));
  REQUIRE(54, prop_bool(timeline, "dragActive"));
  REQUIRE(55, prop_bool(timeline, "dragInside"));

  QTest::mouseMove(&timeline_window, scene_of(pane_width - 20.0, 200.0), 10);
  QTest::qWait(30);
  REQUIRE(56, prop_bool(timeline, "dragInside"));
  REQUIRE(57, timeline->property("edgeScrollDirection").toInt() == 1);
  REQUIRE(58, prop_bool(edge_timer, "running"));

  REQUIRE(59, wait_until([&] { return controller.timelineScroll() > 0.05; }, 4000));
  const double scroll_at_edge = controller.timelineScroll();
  QTest::qWait(120);
  REQUIRE(60, controller.timelineScroll() > scroll_at_edge);   // still advancing while held

  // Leave the Timeline: autoscroll and movement must stop immediately.
  QTest::mouseMove(&timeline_window, scene_of(pane_width + 60.0, 200.0), 10);
  QTest::qWait(30);
  REQUIRE(61, !prop_bool(timeline, "dragInside"));
  REQUIRE(62, timeline->property("edgeScrollDirection").toInt() == 0);
  REQUIRE(63, !prop_bool(edge_timer, "running"));
  const double scroll_at_exit = controller.timelineScroll();
  QTest::qWait(200);
  REQUIRE(64, std::abs(controller.timelineScroll() - scroll_at_exit) < 1e-9);   // frozen

  // Cancel (grab lost / gesture aborted) terminates the drag and clears the timer.
  QMetaObject::invokeMethod(body, "canceled");
  QTest::qWait(20);
  REQUIRE(65, !prop_bool(timeline, "dragActive"));
  REQUIRE(66, !prop_bool(edge_timer, "running"));
  REQUIRE(67, timeline->property("activeDragBlock").isNull());

  // ========================= Part C: Viewer/Timeline splitter =============================
  auto main_object = create_qml(qml_engine, QStringLiteral("Main.qml"), "Main");
  auto* main_window = qobject_cast<QQuickWindow*>(main_object.get());
  REQUIRE(70, main_window != nullptr);
  main_window->resize(1280, 800);
  main_window->show();
  REQUIRE(71, QTest::qWaitForWindowExposed(main_window));
  QTest::qWait(80);

  auto* splitter = find_quick_item(main_window->contentItem(), QStringLiteral("workspaceSplitter"));
  auto* split_mouse = find_quick_item(main_window->contentItem(), QStringLiteral("workspaceSplitMouse"));
  auto* viewer_pane = find_quick_item(main_window->contentItem(), QStringLiteral("viewerPane"));
  auto* media_panel = find_quick_item(main_window->contentItem(), QStringLiteral("mediaPanel"));
  auto* inspector_panel = find_quick_item(main_window->contentItem(), QStringLiteral("inspectorPanel"));
  auto* monitor = find_quick_item(main_window->contentItem(), QStringLiteral("monitorModule"));
  REQUIRE(72, splitter != nullptr && split_mouse != nullptr && viewer_pane != nullptr &&
                  media_panel != nullptr && inspector_panel != nullptr && monitor != nullptr);

  const double split_before = prop_real(main_window, "timelineHeight");
  const double viewer_before = viewer_pane->height();
  REQUIRE(73, split_before > 150.0);

  // The splitter is UI-only: the project on disk must be byte-identical across the drag.
  const auto split_root = fs::temp_directory_path() / "pulsar-workspace-split";
  fs::remove_all(split_root);
  fs::create_directories(split_root);
  const auto before_path = split_root / "before.pulsar";
  const auto after_path = split_root / "after.pulsar";
  REQUIRE(74, controller.saveProject(QString::fromStdString(before_path.string())));
  const double clean_pos_x = controller.selectedClipPosX();
  const double clean_pos_y = controller.selectedClipPosY();
  const double clean_scale = controller.selectedClipScale();
  REQUIRE(75, !controller.projectDirty());

  const auto split_center = split_mouse->mapToScene(
      QPointF{split_mouse->width() / 2.0, split_mouse->height() / 2.0}).toPoint();
  QTest::mousePress(main_window, Qt::LeftButton, Qt::NoModifier, split_center);
  QTest::mouseMove(main_window, QPoint{split_center.x(), split_center.y() - 70}, 10);
  QTest::mouseRelease(main_window, Qt::LeftButton, Qt::NoModifier,
                      QPoint{split_center.x(), split_center.y() - 70});
  QTest::qWait(40);

  const double split_after = prop_real(main_window, "timelineHeight");
  const double viewer_after = viewer_pane->height();
  REQUIRE(76, split_after > split_before + 40.0);       // dragged up: more Timeline
  REQUIRE(77, viewer_after < viewer_before - 40.0);     // and less Viewer
  REQUIRE(78, std::abs(clean_pos_x - controller.selectedClipPosX()) < 1e-9);
  REQUIRE(79, std::abs(clean_pos_y - controller.selectedClipPosY()) < 1e-9);
  REQUIRE(80, std::abs(clean_scale - controller.selectedClipScale()) < 1e-9);
  REQUIRE(81, !controller.projectDirty());
  REQUIRE(82, controller.saveProject(QString::fromStdString(after_path.string())));
  REQUIRE(83, read_bytes(before_path) == read_bytes(after_path));

  // ===================== Screen-size validation (reporting) ==============================
  QMetaObject::invokeMethod(main_window, "resetSplit");
  QTest::qWait(40);
  auto* main_overlay = find_quick_item(main_window->contentItem(),
                                       QStringLiteral("viewerInteractionOverlay"));
  REQUIRE(84, main_overlay != nullptr);
  std::fprintf(stdout, "\nscreen, content_w, content_h, left_w, right_w, central_w, timeline_h, viewer_w, viewer_h, canvas_w, canvas_h, lateral_bar\n");
  const QSize screens[] = {QSize{1280, 720}, QSize{1280, 800}, QSize{1366, 768},
                           QSize{1680, 1050}, QSize{1920, 1080}};
  for (const auto& size : screens) {
    main_window->resize(size);
    QTest::qWait(80);
    const double cw = prop_real(main_overlay, "canvasWidth");
    const double ch = prop_real(main_overlay, "canvasHeight");
    const double vw = main_overlay->width();
    const double vh = main_overlay->height();
    std::fprintf(stdout, "%dx%d, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f\n",
                 size.width(), size.height(), main_window->contentItem()->width(),
                 main_window->contentItem()->height(), media_panel->width(), inspector_panel->width(),
                 monitor->width(), prop_real(main_window, "timelineHeight"),
                 vw, vh, cw, ch, std::max(0.0, (vw - cw) / 2.0));
    REQUIRE(85, std::abs(cw / std::max(1.0, ch) - 16.0 / 9.0) < 0.02);
    REQUIRE(86, cw <= vw + 2.0 && ch <= vh + 2.0);
    REQUIRE(87, std::abs(cw - vw) < 3.0 || std::abs(ch - vh) < 3.0);
  }

  // Tear down QML and render resources while the event loop and controller are
  // still alive. Qt Quick may otherwise leave the final image texture queued
  // on its render thread until process exit, which LeakSanitizer correctly
  // reports even though the production window lifecycle is event-loop driven.
  main_window->close();
  main_window->releaseResources();
  main_object.reset();
  timeline_window.close();
  timeline_window.releaseResources();
  timeline->setParentItem(nullptr);
  timeline_object.reset();
  viewer_window.close();
  viewer_window.releaseResources();
  viewer->setParentItem(nullptr);
  viewer_object.reset();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QTest::qWait(200); // drain asynchronous image-provider completions
  qml_engine.clearComponentCache();
  qml_engine.collectGarbage();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QCoreApplication::processEvents();
  std::fprintf(stdout, "workspace test: all steps passed\n");
  return 0;
}
