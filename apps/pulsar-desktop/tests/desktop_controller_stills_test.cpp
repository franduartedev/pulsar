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
  fs::remove_all(root);
  return 0;
}
