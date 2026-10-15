// Preview/export parity of titles: the exported frame must show the same title, in the same
// place, as the preview — both go through one rasteriser and one compositor.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"
#include "../src/title_rasterizer.hpp"

#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_video_source.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_frame_scaler.hpp>
#include <pulsar/project_file/file_project_repository.hpp>

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <cstring>
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
  const auto root = fs::temp_directory_path() /
      ("pulsar-title-parity-" + std::to_string(QCoreApplication::applicationPid()));
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

  // ---- Look: a dark box and 70 % opacity must come out of the export exactly as in the preview ----
  controller.setSelectedTitleLook(1, 70);
  REQUIRE(40, controller.selectedTitle().value("background").toInt() == 1 && controller.selectedTitle().value("opacity").toInt() == 70);
  const auto boxed_titles = std::vector<pulsar::domain::Title>{*controller.titleForTest()};
  REQUIRE(41, boxed_titles[0].background() == pulsar::domain::TitleBackground::Dark && boxed_titles[0].opacity_percent() == 70);
  auto boxed_expected = frame_at(video, t);
  composition::TitleCompositor boxed_compositor{std::make_shared<QtTitleRasterizer>()};
  REQUIRE(42, boxed_compositor.composite(composition::RgbaCanvas{boxed_expected.width, boxed_expected.height, boxed_expected.stride_bytes, boxed_expected.pixels.data()}, boxed_titles, t));
  int bx0 = boxed_expected.width, by0 = boxed_expected.height, bx1 = 0, by1 = 0;
  for (int y = 0; y < boxed_expected.height; ++y) for (int x = 0; x < boxed_expected.width; ++x) {
    if (mean_abs_diff(boxed_expected, without_title, x, y, x + 1, y + 1) > 8) { bx0 = std::min(bx0, x); bx1 = std::max(bx1, x); by0 = std::min(by0, y); by1 = std::max(by1, y); }
  }
  REQUIRE(43, (bx1 - bx0) > (max_x - min_x) + 8 && (by1 - by0) > (max_y - min_y) + 4);   // the box is bigger than the bare text
  // The box darkens the picture behind the text, but not completely (70 % opacity).
  const double box_luma_change = mean_abs_diff(boxed_expected, without_title, bx0 + 1, by0 + 1, bx0 + 5, by0 + 5);
  REQUIRE(44, box_luma_change > 10 && box_luma_change < 200);
  const auto boxed_destination = root / "boxed.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(boxed_destination.string()), QStringLiteral("original"));
  REQUIRE(45, wait_until([&] { return !controller.exportRunning(); }));
  const auto boxed_exported = frame_at(boxed_destination, t);
  const double boxed_inside = mean_abs_diff(boxed_exported, boxed_expected, bx0, by0, bx1 + 1, by1 + 1);
  const double boxed_vs_untitled = mean_abs_diff(boxed_exported, without_title, bx0, by0, bx1 + 1, by1 + 1);
  std::fprintf(stderr, "TITLES boxed export vs preview: %.2f, vs untitled: %.2f\n", boxed_inside, boxed_vs_untitled);
  REQUIRE(46, boxed_inside < 14.0 && boxed_inside * 2.0 < boxed_vs_untitled);
  controller.undoEdit();
  REQUIRE(47, controller.selectedTitle().value("background").toInt() == 0 && controller.selectedTitle().value("opacity").toInt() == 100);

  // ---- Free position: a dragged title lands where it was dropped, in preview and in the export ----
  controller.setSelectedTitlePosition(220, 260);
  REQUIRE(50, controller.selectedTitle().value("hasPosition").toBool());
  REQUIRE(51, controller.selectedTitle().value("blockW").toDouble() > 0.0);
  const auto placed_titles = std::vector<pulsar::domain::Title>{*controller.titleForTest()};
  REQUIRE(52, placed_titles[0].position() && placed_titles[0].position()->x_permille == 220);
  auto placed_expected = frame_at(video, t);
  composition::TitleCompositor placed_compositor{std::make_shared<QtTitleRasterizer>()};
  REQUIRE(53, placed_compositor.composite(composition::RgbaCanvas{placed_expected.width, placed_expected.height, placed_expected.stride_bytes, placed_expected.pixels.data()}, placed_titles, t));
  int px0 = placed_expected.width, py0 = placed_expected.height, px1 = 0, py1 = 0;
  for (int y = 0; y < placed_expected.height; ++y) for (int x = 0; x < placed_expected.width; ++x) {
    if (mean_abs_diff(placed_expected, without_title, x, y, x + 1, y + 1) > 8) { px0 = std::min(px0, x); px1 = std::max(px1, x); py0 = std::min(py0, y); py1 = std::max(py1, y); }
  }
  const double centre_x = (px0 + px1) / 2.0 / placed_expected.width * 1000.0;
  const double centre_y = (py0 + py1) / 2.0 / placed_expected.height * 1000.0;
  std::fprintf(stderr, "TITLES placed centre: %.0f,%.0f permille (asked 220,260)\n", centre_x, centre_y);
  REQUIRE(54, std::fabs(centre_x - 220.0) < 40.0 && std::fabs(centre_y - 260.0) < 60.0);   // the ink sits where it was dropped
  const auto placed_destination = root / "placed.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(placed_destination.string()), QStringLiteral("original"));
  REQUIRE(55, wait_until([&] { return !controller.exportRunning(); }));
  const auto placed_exported = frame_at(placed_destination, t);
  const double placed_inside = mean_abs_diff(placed_exported, placed_expected, px0, py0, px1 + 1, py1 + 1);
  const double placed_vs_untitled = mean_abs_diff(placed_exported, without_title, px0, py0, px1 + 1, py1 + 1);
  std::fprintf(stderr, "TITLES placed export vs preview: %.2f, vs untitled: %.2f\n", placed_inside, placed_vs_untitled);
  REQUIRE(56, placed_inside < 14.0 && placed_inside * 2.0 < placed_vs_untitled);
  // The alignment presets take the title back to the grid; undo / redo move it exactly.
  controller.setSelectedTitleStyle(controller.selectedTitle().value("size").toInt(), QStringLiteral("#FFFFFF"), 0, 0, true);
  REQUIRE(57, !controller.selectedTitle().value("hasPosition").toBool());
  controller.undoEdit();
  REQUIRE(58, controller.selectedTitle().value("hasPosition").toBool());
  controller.resetSelectedTitlePosition();
  REQUIRE(59, !controller.selectedTitle().value("hasPosition").toBool());
  controller.undoEdit();
  REQUIRE(60, controller.selectedTitle().value("hasPosition").toBool());
  controller.undoEdit();   // back to the plain look so the scaler step below is unchanged
  REQUIRE(61, !controller.selectedTitle().value("hasPosition").toBool());

  // ---- Scaler: a 720p-box preset on this 320x180 project never enlarges ------------------------
  const auto destination720 = root / "small.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(destination720.string()), QStringLiteral("1080p"));
  REQUIRE(15, wait_until([&] { return !controller.exportRunning(); }));
  const auto same_size = frame_at(destination720, t);
  REQUIRE(16, same_size.width == 320 && same_size.height == 180);

  // A persisted multi-layer document previews through the same renderer, then titles.
  const auto multi_path = root / "multi.pulsar";
  REQUIRE(70, controller.saveProject(QString::fromStdString(multi_path.string())));
  pulsar::project_file::FileProjectRepository repository;
  auto document = pulsar::application::ProjectDocument::restore(repository.load(multi_path));
  pulsar::application::CommandProcessor processor{document};
  const auto upper_media = processor.import_asset(fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures/e6_video_b.mp4",
      pulsar::media::ffmpeg::FFmpegMediaProbe{});
  auto multi = document.snapshot();
  const auto track_id = document.next_track_id();
  multi.timeline.add_track(pulsar::domain::Track{track_id, pulsar::domain::TrackKind::Video});
  multi.timeline.insert_clip(track_id, pulsar::domain::Clip{document.next_clip_id(), upper_media,
      {MediaTime::zero(), MediaTime{2, {1, 1}}}, MediaTime::zero()});
  multi.visual_track_ids.push_back(track_id);
  ++multi.allocator_state.next_track_id;
  ++multi.allocator_state.next_clip_id;
  (void)repository.save(multi_path, multi);
  REQUIRE(71, controller.openProject(QString::fromStdString(multi_path.string())));
  const auto multi_plan = pulsar::application::exporting::ExportPlan::build(multi);
  pulsar::media::ffmpeg::FFmpegOfflineVideoSourceFactory sources;
  pulsar::media::ffmpeg::FFmpegOfflineFrameScaler scaler;
  composition::TitleCompositor multi_titles{std::make_shared<QtTitleRasterizer>()};
  pulsar::application::exporting::OfflineVideoRenderer renderer{multi_plan, sources, {&scaler, &multi_titles}};
  for (const auto position : {1, 2, 0, 1}) {
    controller.seekNormalized(0.75);
    controller.seekNormalized(0.0);
    controller.seekNormalized(position / 4.0);
    const auto expected_multi = renderer.render_at(MediaTime{position, {1, 1}});
    REQUIRE(72, wait_until([&] {
      const auto actual = provider.requestImage(QStringLiteral("current"), nullptr, {});
      if (actual.width() != expected_multi.width || actual.height() != expected_multi.height) return false;
      for (int y = 0; y < actual.height(); ++y) {
        if (std::memcmp(actual.constScanLine(y), expected_multi.pixels.data() +
            std::size_t(y) * expected_multi.stride_bytes, std::size_t(expected_multi.width) * 4) != 0) return false;
      }
      return true;
    }));
  }
  // A stable canvas and titles still agree when all VIDEO 1 clips are removed.
  const auto* primary_track = multi.timeline.find_track(multi.video_track_id);
  std::vector<pulsar::domain::ClipId> primary_clips;
  for (const auto& clip : primary_track->clips()) primary_clips.push_back(clip.id());
  for (const auto id : primary_clips) multi.timeline.remove_clip(id);
  REQUIRE(73, multi.canvas.has_value() &&
      multi.timeline.find_track(multi.video_track_id)->clips().empty());
  (void)repository.save(multi_path, multi);
  REQUIRE(74, controller.openProject(QString::fromStdString(multi_path.string())));
  const auto upper_only_plan = pulsar::application::exporting::ExportPlan::build(multi);
  pulsar::application::exporting::OfflineVideoRenderer upper_only{upper_only_plan, sources, {&scaler, &multi_titles}};
  controller.seekNormalized(0.5);
  const auto expected_upper = upper_only.render_at(MediaTime{1, {1, 1}});
  REQUIRE(75, wait_until([&] {
    const auto actual = provider.requestImage(QStringLiteral("current"), nullptr, {});
    if (actual.width() != expected_upper.width || actual.height() != expected_upper.height) return false;
    for (int y = 0; y < actual.height(); ++y) {
      if (std::memcmp(actual.constScanLine(y), expected_upper.pixels.data() +
          std::size_t(y) * expected_upper.stride_bytes, std::size_t(expected_upper.width) * 4) != 0) return false;
    }
    return true;
  }));
  fs::remove_all(root);
  return 0;
}
