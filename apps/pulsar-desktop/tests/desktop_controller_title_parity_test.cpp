// Preview/export parity of titles: the exported frame must show the same title, in the same
// place, as the preview — both go through one rasteriser and one compositor.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"
#include "../src/title_rasterizer.hpp"

#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <vector>

namespace {
namespace fs = std::filesystem;
using pulsar::domain::MediaTime;
using pulsar::domain::Rational;
namespace composition = pulsar::application::composition;

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

// Decodes the frame shown at `time` as RGBA (the same decode + conversion the preview and
// the exporter use for pictures).
pulsar::media::ffmpeg::FFmpegPreviewFrame frame_at(const fs::path& path, MediaTime time) {
  pulsar::media::ffmpeg::FFmpegVideoDecodeSession session{path, 0};
  auto seek = session.seek_to_presentation_timestamp_visual(time);
  auto frame = seek.frame ? std::move(seek.frame) : std::move(seek.lookahead);
  if (!frame) frame = session.decode_next_visual();
  return pulsar::media::ffmpeg::FFmpegPreviewConverter{}.convert(*frame);
}

double mean_abs_diff(const pulsar::media::ffmpeg::FFmpegPreviewFrame& a, const pulsar::media::ffmpeg::FFmpegPreviewFrame& b,
                     int x0, int y0, int x1, int y1) {
  double sum = 0; long count = 0;
  for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) for (int c = 0; c < 3; ++c) {
    sum += std::abs(int(a.pixels[std::size_t(y) * a.stride_bytes + x * 4 + c]) - int(b.pixels[std::size_t(y) * b.stride_bytes + x * 4 + c]));
    ++count;
  }
  return count ? sum / count : 0;
}
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { std::fprintf(stderr, "TITLES step %d failed: %s\n", step, #condition); return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider provider;
  DesktopController controller{provider, {}};
  const auto video = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures/e6_video_a.mp4";
  const auto root = fs::temp_directory_path() / "pulsar-title-parity";
  fs::remove_all(root);
  fs::create_directories(root);

  controller.openPath(QString::fromStdString(video.string()));
  REQUIRE(1, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  controller.seekNormalized(0.25);                                       // 1 s into the 4 s clip
  controller.addTitle();
  REQUIRE(2, !controller.selectedTitleKey().isEmpty() && controller.errorMessage().isEmpty());
  controller.setSelectedTitleText(QStringLiteral("PARIDAD ñ"));
  controller.setSelectedTitleStyle(14, QStringLiteral("#FFD84D"), 1, 2, true);
  REQUIRE(3, controller.selectedTitle().value("text").toString() == QStringLiteral("PARIDAD ñ") &&
             controller.selectedTitle().value("size").toInt() == 14);
  const auto title_time = controller.timelineTitles()->index(0, 0).data(TitleListModel::StartTextRole).toString();  // sanity: has a start
  REQUIRE(4, !title_time.isEmpty());

  // ---- Expected picture: the source frame + the title through the SAME compositor -------------
  const MediaTime t{1, Rational{1, 1}};
  auto expected = frame_at(video, t);
  // The document is the authority on the title (id, time range, style).
  const auto titles = std::vector<pulsar::domain::Title>{*controller.titleForTest()};
  auto rasterizer = std::make_shared<QtTitleRasterizer>();
  composition::TitleCompositor compositor{rasterizer};
  auto without_title = expected;                                         // copy before drawing
  REQUIRE(5, compositor.composite(composition::RgbaCanvas{expected.width, expected.height, expected.stride_bytes, expected.pixels.data()}, titles, t));
  // Where is the title? (bottom centre) — box of differences between with/without.
  int min_x = expected.width, min_y = expected.height, max_x = 0, max_y = 0;
  for (int y = 0; y < expected.height; ++y) for (int x = 0; x < expected.width; ++x) {
    if (mean_abs_diff(expected, without_title, x, y, x + 1, y + 1) > 8) { min_x = std::min(min_x, x); max_x = std::max(max_x, x); min_y = std::min(min_y, y); max_y = std::max(max_y, y); }
  }
  REQUIRE(6, max_x > min_x && max_y > min_y && min_y > expected.height / 2);   // drawn, in the lower half

  // ---- Preview: the controller's picture at that time ------------------------------------------
  controller.seekNormalized(0.25);
  REQUIRE(7, wait_until([&] { return !controller.previewSource().isEmpty(); }));
  QSize size;
  const QImage shown = provider.requestImage(QStringLiteral("current"), &size, {});
  REQUIRE(8, shown.width() == expected.width && shown.height() == expected.height);
  double preview_diff = 0; long n = 0;
  for (int y = min_y; y <= max_y; ++y) for (int x = min_x; x <= max_x; ++x) for (int c = 0; c < 3; ++c) {
    preview_diff += std::abs(int(shown.constScanLine(y)[x * 4 + c]) - int(expected.pixels[std::size_t(y) * expected.stride_bytes + x * 4 + c])); ++n;
  }
  REQUIRE(9, preview_diff / n < 2.0);   // same pixels (allowing for a neighbouring source frame)

  // ---- Export: the file shows the title where the preview does ----------------------------------
  const auto destination = root / "titled.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(destination.string()), QStringLiteral("original"));
  REQUIRE(10, wait_until([&] { return !controller.exportRunning(); }));
  REQUIRE(11, controller.exportStatus() == QStringLiteral("Exportación completada") && fs::exists(destination));
  const auto exported = frame_at(destination, t);
  REQUIRE(12, exported.width == expected.width && exported.height == expected.height);
  const double inside_with = mean_abs_diff(exported, expected, min_x, min_y, max_x + 1, max_y + 1);
  const double inside_without = mean_abs_diff(exported, without_title, min_x, min_y, max_x + 1, max_y + 1);
  std::fprintf(stderr, "TITLES export vs preview inside box: %.2f, vs untitled: %.2f\n", inside_with, inside_without);
  REQUIRE(13, inside_with < 14.0 && inside_with * 2.0 < inside_without);   // codec noise only; clearly the titled picture
  // Outside the title the export is the plain video (no stray overlay).
  REQUIRE(14, mean_abs_diff(exported, without_title, 0, 0, expected.width, min_y > 8 ? min_y - 4 : 1) < 14.0);

  // ---- Scaler: a 720p-box preset on this 320x180 project never enlarges ------------------------
  const auto destination720 = root / "small.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(destination720.string()), QStringLiteral("1080p"));
  REQUIRE(15, wait_until([&] { return !controller.exportRunning(); }));
  const auto same_size = frame_at(destination720, t);
  REQUIRE(16, same_size.width == 320 && same_size.height == 180);
  fs::remove_all(root);
  return 0;
}
