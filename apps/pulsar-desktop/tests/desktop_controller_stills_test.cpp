// Photographs and colour cards as real clips: import, place, play, undo, save/reopen, export.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>

namespace {
namespace fs = std::filesystem;
using pulsar::domain::MediaTime;
using pulsar::domain::Rational;

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
void pump(int ms) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); }

QString id_of(DesktopController& c, const QString& name) {
  auto* model = c.mediaAssets();
  for (int row = 0; row < model->rowCount(); ++row) {
    if (model->index(row, 0).data(MediaAssetListModel::NameRole).toString() == name) return model->index(row, 0).data(MediaAssetListModel::MediaIdRole).toString();
  }
  return {};
}
QString asset_name(DesktopController& c, int row) { return c.mediaAssets()->index(row, 0).data(MediaAssetListModel::NameRole).toString(); }
QString kind_of(DesktopController& c, const QString& name) {
  auto* model = c.mediaAssets();
  for (int row = 0; row < model->rowCount(); ++row) {
    if (model->index(row, 0).data(MediaAssetListModel::NameRole).toString() == name) return model->index(row, 0).data(MediaAssetListModel::KindRole).toString();
  }
  return {};
}
QString clip_key_named(DesktopController& c, const QString& name) {
  auto* model = c.timelineClips();
  for (int row = 0; row < model->rowCount(); ++row) {
    const auto index = model->index(row, 0);
    if (index.data(TimelineClipListModel::NameRole).toString() == name) {
      return index.data(TimelineClipListModel::ClipKeyRole).toString();
    }
  }
  return {};
}

pulsar::media::ffmpeg::FFmpegPreviewFrame frame_at(const fs::path& path, MediaTime time) {
  pulsar::media::ffmpeg::FFmpegVideoDecodeSession session{path, 0};
  auto seek = session.seek_to_presentation_timestamp_visual(time);
  auto frame = seek.frame ? std::move(seek.frame) : std::move(seek.lookahead);
  if (!frame) frame = session.decode_next_visual();
  return pulsar::media::ffmpeg::FFmpegPreviewConverter{}.convert(*frame);
}
// Mean RGB of a rectangle (fractions of the picture).
double mean_channel(const pulsar::media::ffmpeg::FFmpegPreviewFrame& f, int channel, double fx0, double fy0, double fx1, double fy1) {
  const int x0 = int(f.width * fx0), x1 = int(f.width * fx1), y0 = int(f.height * fy0), y1 = int(f.height * fy1);
  double sum = 0; long n = 0;
  for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) { sum += f.pixels[std::size_t(y) * f.stride_bytes + x * 4 + channel]; ++n; }
  return n ? sum / n : 0;
}
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { std::fprintf(stderr, "STILLS step %d failed: %s | error='%s' status='%s' duration=%s\n", step, #condition, \
  controller.errorMessage().toUtf8().constData(), controller.status().toUtf8().constData(), controller.timelineDurationText().toUtf8().constData()); return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider provider;
  DesktopController controller{provider, {}};
  const auto fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  const auto root = fs::temp_directory_path() /
      ("pulsar-stills-test-" + std::to_string(QCoreApplication::applicationPid()));
  fs::remove_all(root);
  fs::create_directories(root);
  const auto video = fixtures / "e6_video_a.mp4";

  // 1. A video first, then a batch of pictures: pictures go to the library, tagged as images.
  controller.openPath(QString::fromStdString(video.string()));
  REQUIRE(1, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  controller.openPaths({QString::fromStdString((fixtures / "still_wide.png").string()), QString::fromStdString((fixtures / "still_portrait.jpg").string()),
                        QString::fromStdString((fixtures / "still_bars.webp").string())});
  REQUIRE(2, wait_until([&] { return controller.mediaAssets()->rowCount() == 4 && !controller.busy() && controller.importPending() == 0; }));
  REQUIRE(3, kind_of(controller, "still_wide.png") == "image" && kind_of(controller, "still_portrait.jpg") == "image" &&
             kind_of(controller, "still_bars.webp") == "image" && kind_of(controller, "e6_video_a.mp4") == "video");
  REQUIRE(4, controller.timelineDurationText().contains("4"));            // pictures were not placed on their own

  // The bin can be filtered by kind and searched by name without touching the project.
  auto* bin = qobject_cast<MediaAssetListModel*>(controller.mediaAssets());
  REQUIRE(40, bin != nullptr && bin->totalCount() == 4 && bin->rowCount() == 4);
  bin->setFilter(QStringLiteral("image"), {});
  REQUIRE(41, bin->rowCount() == 3 && bin->totalCount() == 4);
  bin->setFilter(QStringLiteral("video"), {});
  REQUIRE(42, bin->rowCount() == 1);
  bin->setFilter({}, QStringLiteral("WIDE"));
  REQUIRE(43, bin->rowCount() == 1 && asset_name(controller, 0) == QStringLiteral("still_wide.png"));
  bin->setFilter(QStringLiteral("audio"), {});
  REQUIRE(44, bin->rowCount() == 0);
  bin->setFilter({}, {});
  REQUIRE(45, bin->rowCount() == 4);

  // 2. Append a picture: it is a 5 s clip at the end of VIDEO 1.
  controller.appendAssetToVideoTrack(id_of(controller, "still_wide.png"));
  REQUIRE(5, controller.errorMessage().isEmpty() && controller.timelineDurationText() == QStringLiteral("00:00:09.000"));
  controller.appendAssetToVideoTrack(id_of(controller, "still_portrait.jpg"));
  REQUIRE(6, controller.timelineDurationText() == QStringLiteral("00:00:14.000"));

  // 3. The preview shows the picture (not black, not the video), at its own size, at any time inside it.
  controller.seekNormalized(6.0 / 14.0);
  REQUIRE(7, wait_until([&] { return controller.viewerState() == QStringLiteral("frame") && !controller.previewSource().isEmpty(); }));
  QSize size;
  QImage shown = provider.requestImage(QStringLiteral("current"), &size, {});
  REQUIRE(8, shown.width() == 640 && shown.height() == 360);
  controller.seekNormalized(8.5 / 14.0);
  REQUIRE(9, wait_until([&] { return controller.viewerState() == QStringLiteral("frame"); }));
  shown = provider.requestImage(QStringLiteral("current"), &size, {});
  REQUIRE(10, shown.width() == 640 && shown.height() == 360);            // still the same picture, 3.5 s later

  // A transformed still must route through the authoritative compositor even
  // in a one-track project. The resulting project-canvas pixels expose black
  // background outside the moved/scaled picture; reset returns to the direct
  // identity path without changing the canvas contract.
  controller.selectTimelineClip(controller.timelineClips()->index(1, 0).data(TimelineClipListModel::ClipKeyRole).toString());
  controller.setSelectedClipTransform(0.20, 0.10, 0.45, 0.0, 1.0);
  REQUIRE(90, wait_until([&] {
    return controller.displayedPreviewProjectRevision() == controller.projectRevision() &&
           !provider.image().isNull() && provider.image().width() == 320;
  }));
  const auto transformed_still = provider.image();
  REQUIRE(91, transformed_still.width() == 320 && transformed_still.height() == 180 &&
                  transformed_still.pixelColor(0, 0).red() < 16 &&
                  transformed_still.pixelColor(0, 0).green() < 16 &&
                  transformed_still.pixelColor(0, 0).blue() < 16);
  controller.resetSelectedClipTransform();
  REQUIRE(92, wait_until([&] {
    return controller.displayedPreviewProjectRevision() == controller.projectRevision() &&
           !provider.image().isNull();
  }));
  controller.seekNormalized(11.0 / 14.0);
  REQUIRE(11, wait_until([&] { return controller.viewerState() == QStringLiteral("frame"); }));
  shown = provider.requestImage(QStringLiteral("current"), &size, {});
  REQUIRE(12, shown.width() == 180 && shown.height() == 320);            // the portrait picture

  // 4. Playing through the picture keeps showing it (no error, no gap, time advances).
  controller.seekNormalized(4.2 / 14.0);
  pump(300);
  controller.play();
  REQUIRE(13, wait_until([&] { return controller.positionText() >= QStringLiteral("00:00:05.200"); }, 20'000));
  REQUIRE(14, controller.viewerState() == QStringLiteral("frame") && controller.errorMessage().isEmpty());
  controller.pause();

  // 5. Trim and undo behave like any clip: cut the picture short at the playhead, then undo.
  controller.selectTimelineClip(controller.timelineClips()->index(2, 0).data(TimelineClipListModel::ClipKeyRole).toString());
  controller.seekNormalized(11.0 / 14.0);
  controller.trimSelectedEndToPlayhead();
  REQUIRE(15, controller.errorMessage().isEmpty() && controller.timelineDurationText() == QStringLiteral("00:00:11.000"));
  controller.undoEdit();
  REQUIRE(16, controller.timelineDurationText() == QStringLiteral("00:00:14.000"));

  // 5b. A colour card: no file, 5 s, at the playhead (the end of the timeline here); a second one
  //     with the playhead inside a clip goes to the end instead; the asset is reused per colour.
  controller.seekToEnd();
  controller.createColorCard(QStringLiteral("#ff8800"));
  REQUIRE(116, controller.errorMessage().isEmpty() && controller.timelineDurationText() == QStringLiteral("00:00:19.000"));
  REQUIRE(117, kind_of(controller, "Color #FF8800") == "color");
  controller.seekNormalized(0.1);
  controller.createColorCard(QStringLiteral("#ff8800"));
  REQUIRE(118, controller.timelineDurationText() == QStringLiteral("00:00:24.000") && controller.mediaAssets()->rowCount() == 5);
  controller.undoEdit();
  controller.createColorCard(QStringLiteral("#00aa55"));              // a NEW colour: asset + clip are one undo step
  REQUIRE(1190, controller.mediaAssets()->rowCount() == 6);
  controller.undoEdit();
  REQUIRE(1191, controller.mediaAssets()->rowCount() == 5 && controller.timelineDurationText() == QStringLiteral("00:00:19.000"));
  controller.createColorCard(QStringLiteral("not a colour"));
  REQUIRE(119, !controller.errorMessage().isEmpty());
  REQUIRE(120, controller.timelineDurationText() == QStringLiteral("00:00:19.000"));

  // 6. Save, reopen: the pictures are still pictures and the timeline is the same length.
  const auto project = root / "stills.pulsar";
  REQUIRE(17, controller.saveProject(QString::fromStdString(project.string())));
  REQUIRE(18, controller.openProject(QString::fromStdString(project.string())));
  REQUIRE(19, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  REQUIRE(20, controller.timelineDurationText() == QStringLiteral("00:00:19.000") && kind_of(controller, "still_wide.png") == "image" && kind_of(controller, "Color #FF8800") == "color");

  // 7. Export: the picture is in the file for its whole 5 s, scaled into the 320x180 canvas with bars.
  const auto destination = root / "stills.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(destination.string()), QStringLiteral("original"));
  REQUIRE(21, wait_until([&] { return !controller.exportRunning(); }));
  REQUIRE(22, controller.exportStatus() == QStringLiteral("Exportación completada") && fs::exists(destination));
  const auto early = frame_at(destination, MediaTime{4500, Rational{1, 1000}});    // start of the wide picture
  const auto late = frame_at(destination, MediaTime{8500, Rational{1, 1000}});     // end of the wide picture
  REQUIRE(23, early.width == 320 && early.height == 180 && late.width == 320 && late.height == 180);
  double drift = 0; long n = 0;
  for (int y = 0; y < 180; ++y) for (int x = 0; x < 320; ++x) for (int c = 0; c < 3; ++c) {
    drift += std::abs(int(early.pixels[std::size_t(y) * early.stride_bytes + x * 4 + c]) - int(late.pixels[std::size_t(y) * late.stride_bytes + x * 4 + c])); ++n;
  }
  REQUIRE(24, drift / n < 6.0);                                        // the same picture held (codec noise only)
  // The portrait picture (180x320) is pillar-boxed into 320x180: black bars left and right.
  const auto portrait = frame_at(destination, MediaTime{12, Rational{1, 1}});
  REQUIRE(25, mean_channel(portrait, 0, 0.0, 0.0, 0.2, 1.0) < 12 && mean_channel(portrait, 1, 0.8, 0.0, 1.0, 1.0) < 12);
  REQUIRE(26, mean_channel(portrait, 1, 0.4, 0.1, 0.6, 0.9) > 20);     // picture content in the middle
  const auto card = frame_at(destination, MediaTime{17, Rational{1, 1}});
  REQUIRE(27, std::abs(mean_channel(card, 0, 0.0, 0.0, 1.0, 1.0) - 255) < 12 && std::abs(mean_channel(card, 1, 0.0, 0.0, 1.0, 1.0) - 136) < 12 &&
              mean_channel(card, 2, 0.0, 0.0, 1.0, 1.0) < 16);   // the generated colour, edge to edge (16:9 into 16:9)

  // 8. A transparent PNG becomes a creator-facing overlay: one upper visual
  // layer, a useful default logo transform and one undo step for track+clip.
  controller.openPath(QString::fromStdString((fixtures / "still_alpha.png").string()));
  REQUIRE(200, wait_until([&] { return controller.mediaAssets()->rowCount() == 6 && !controller.busy(); }));
  controller.seekNormalized(1.0 / 19.0);
  REQUIRE(201, wait_until([&] {
    return controller.viewerState() == QStringLiteral("frame") &&
           controller.displayedPreviewProjectRevision() == controller.projectRevision() &&
           !provider.image().isNull();
  }));
  const QImage background = provider.image();
  controller.insertAssetAsOverlay(id_of(controller, QStringLiteral("still_alpha.png")));
  REQUIRE(202, controller.errorMessage().isEmpty() && controller.selectedClipIsOverlay() &&
                   controller.timelineTracks().size() == 2 &&
                   std::abs(controller.selectedClipScale() - 0.25) < 1e-9);
  const QString overlay_key = controller.selectedClipKey();
  controller.undoEdit();
  REQUIRE(203, controller.timelineTracks().size() == 1 && clip_key_named(controller, QStringLiteral("still_alpha.png")).isEmpty());
  controller.redoEdit();
  REQUIRE(204, controller.timelineTracks().size() == 2 && !clip_key_named(controller, QStringLiteral("still_alpha.png")).isEmpty());
  controller.selectTimelineClip(overlay_key);
  controller.setSelectedClipTransform(0.0, 0.0, 1.0, 0.0, 1.0);
  REQUIRE(205, wait_until([&] {
    return controller.displayedPreviewProjectRevision() == controller.projectRevision() &&
           provider.image().width() == 320 && provider.image().height() == 180;
  }));
  const QImage centred = provider.image();
  REQUIRE(206, centred.pixelColor(40, 90).red() > 220 &&
                   std::abs(centred.pixelColor(280, 90).red() - background.pixelColor(280, 90).red()) < 4 &&
                   std::abs(centred.pixelColor(280, 90).green() - background.pixelColor(280, 90).green()) < 4);
  const auto evidence = qEnvironmentVariable("PULSAR_OVERLAY_EVIDENCE_DIR");
  if (!evidence.isEmpty()) {
    fs::create_directories(evidence.toStdString());
    centred.save(evidence + QStringLiteral("/overlay-position-a.png"));
  }
  const auto overlay_a_project = root / "overlay-position-a.pulsar";
  REQUIRE(2060, controller.saveProject(QString::fromStdString(overlay_a_project.string())));
  controller.setSelectedClipTransform(-0.25, 0.20, 0.40, 0.0, 0.55);
  REQUIRE(207, wait_until([&] {
    return controller.displayedPreviewProjectRevision() == controller.projectRevision() &&
           std::abs(controller.selectedClipOpacity() - 0.55) < 1e-9;
  }));
  const QImage moved = provider.image();
  // Position A showed the opaque logo far from the centre; B moved and scaled it
  // away from (100,30), and the 55% opacity blends red with the green background.
  REQUIRE(2065, std::abs(moved.pixelColor(100, 30).red() - background.pixelColor(100, 30).red()) < 6 &&
                    std::abs(moved.pixelColor(100, 30).green() - background.pixelColor(100, 30).green()) < 6);
  REQUIRE(2066, moved.pixelColor(40, 120).red() > 100 && moved.pixelColor(40, 120).red() < 200 &&
                    moved.pixelColor(40, 120).green() > 20 && moved.pixelColor(40, 120).green() < 100);
  if (!evidence.isEmpty()) moved.save(evidence + QStringLiteral("/overlay-position-b.png"));
  controller.undoEdit();
  REQUIRE(208, std::abs(controller.selectedClipScale() - 1.0) < 1e-9 &&
                   std::abs(controller.selectedClipOpacity() - 1.0) < 1e-9);
  controller.redoEdit();
  REQUIRE(209, std::abs(controller.selectedClipPosX() + 0.25) < 1e-9 &&
                   std::abs(controller.selectedClipPosY() - 0.20) < 1e-9 &&
                   std::abs(controller.selectedClipScale() - 0.40) < 1e-9 &&
                   std::abs(controller.selectedClipOpacity() - 0.55) < 1e-9);

  const auto overlay_project = root / "overlay.pulsar";
  REQUIRE(210, controller.saveProject(QString::fromStdString(overlay_project.string())) &&
                   controller.openProject(QString::fromStdString(overlay_project.string())));
  REQUIRE(211, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  controller.selectTimelineClip(clip_key_named(controller, QStringLiteral("still_alpha.png")));
  REQUIRE(212, controller.selectedClipIsOverlay() &&
                   std::abs(controller.selectedClipPosX() + 0.25) < 1e-9 &&
                   std::abs(controller.selectedClipScale() - 0.40) < 1e-9 &&
                   std::abs(controller.selectedClipOpacity() - 0.55) < 1e-9);
  controller.seekNormalized(1.5 / 19.0);
  REQUIRE(213, wait_until([&] {
    return controller.displayedPreviewProjectRevision() == controller.projectRevision() &&
           provider.image().width() == 320 && provider.image().height() == 180;
  }));
  const QImage preview_overlay = provider.image();
  REQUIRE(2130, preview_overlay.pixelColor(40, 120).red() > 100 &&
                    preview_overlay.pixelColor(40, 120).red() < 210 &&
                    preview_overlay.pixelColor(40, 120).green() > 15 &&
                    preview_overlay.pixelColor(40, 120).green() < 110);   // persisted above the video
  const auto overlay_export = root / "overlay-export.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(overlay_export.string()), QStringLiteral("original"));
  REQUIRE(214, wait_until([&] { return !controller.exportRunning(); }, 120'000) && fs::exists(overlay_export));
  const auto exported_overlay = frame_at(overlay_export, MediaTime{1500, Rational{1, 1000}});
  const auto exported_red = exported_overlay.pixels[std::size_t(120) * exported_overlay.stride_bytes + 40 * 4];
  const auto exported_green = exported_overlay.pixels[std::size_t(120) * exported_overlay.stride_bytes + 40 * 4 + 1];
  REQUIRE(2140, exported_red > 90 && exported_red < 215 && exported_green > 10 && exported_green < 115);
  double parity_drift = 0; long parity_samples = 0;
  for (int y = 0; y < 180; ++y) for (int x = 0; x < 320; ++x) for (int c = 0; c < 3; ++c) {
    const auto preview_pixel = preview_overlay.pixelColor(x, y);
    const int preview_channel = c == 0 ? preview_pixel.red() : (c == 1 ? preview_pixel.green() : preview_pixel.blue());
    parity_drift += std::abs(int(exported_overlay.pixels[std::size_t(y) * exported_overlay.stride_bytes + x * 4 + c]) -
                             preview_channel);
    ++parity_samples;
  }
  REQUIRE(215, parity_drift / parity_samples < 12.0);
  const auto after_overlay = frame_at(overlay_export, MediaTime{4500, Rational{1, 1000}});
  double interval_drift = 0; long interval_samples = 0;
  for (int y = 0; y < 180; ++y) for (int x = 0; x < 320; ++x) for (int c = 0; c < 3; ++c) {
    interval_drift += std::abs(int(after_overlay.pixels[std::size_t(y) * after_overlay.stride_bytes + x * 4 + c]) -
                               int(early.pixels[std::size_t(y) * early.stride_bytes + x * 4 + c]));
    ++interval_samples;
  }
  REQUIRE(216, interval_drift / interval_samples < 7.0);  // overlay ended with its background clip
  if (!evidence.isEmpty()) {
    fs::copy_file(overlay_a_project, fs::path{evidence.toStdString()} / "overlay-position-a.pulsar",
                  fs::copy_options::overwrite_existing);
    fs::copy_file(overlay_project, fs::path{evidence.toStdString()} / "overlay.pulsar",
                  fs::copy_options::overwrite_existing);
    fs::copy_file(overlay_export, fs::path{evidence.toStdString()} / "overlay-export.mp4",
                  fs::copy_options::overwrite_existing);
  }
  fs::remove_all(root);
  return 0;
}
