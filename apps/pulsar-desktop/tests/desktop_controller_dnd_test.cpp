// Drag & drop from the library: one resolver for preview and drop, overlap refused, undoable.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>

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
QString q(const fs::path& p) { return QString::fromStdString(p.string()); }
int clips(DesktopController& c) { return c.timelineClips()->rowCount(); }
int assets(DesktopController& c) { return c.mediaAssets()->rowCount(); }
QString asset_id(DesktopController& c, int row) { return c.mediaAssets()->index(row, 0).data(MediaAssetListModel::MediaIdRole).toString(); }
QString clip_start(DesktopController& c, int row) { return c.timelineClips()->index(row, 0).data(TimelineClipListModel::StartRole).toString(); }

void write_wav(const fs::path& path, int seconds) {
  const std::uint32_t rate = 44100, samples = rate * seconds, bytes = samples * 2;
  std::ofstream out{path, std::ios::binary};
  const auto u32 = [&](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
  const auto u16 = [&](std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
  out.write("RIFF", 4); u32(36 + bytes); out.write("WAVEfmt ", 8); u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
  out.write("data", 4); u32(bytes);
  std::vector<char> silence(bytes, 0);
  out.write(silence.data(), static_cast<std::streamsize>(silence.size()));
}
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { std::fprintf(stderr, "DND step %d failed: %s | error='%s' clips=%d\n", step, #condition, \
  controller.errorMessage().toUtf8().constData(), clips(controller)); return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider provider;
  DesktopController controller{provider, {}};
  const auto fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  const auto root = fs::temp_directory_path() / "pulsar-dnd-test";
  fs::remove_all(root);
  fs::create_directories(root);
  fs::copy_file(fixtures / "e6_video_a.mp4", root / "a.mp4");
  fs::copy_file(fixtures / "e6_video_b.mp4", root / "b.mp4");
  write_wav(root / "music.wav", 2);

  controller.openPaths(QStringList{q(root / "a.mp4"), q(root / "b.mp4")});
  REQUIRE(1, wait_until([&] { return assets(controller) == 2 && !controller.busy(); }) && clips(controller) == 1);
  const double width = 1000.0;                            // 4 s of video on a 1000 px wide timeline: 250 px/s
  const auto video_b = asset_id(controller, 1);

  // Preview == drop. Near the end of A the start snaps flush to A's end (exact), never overlapping.
  const auto preview = controller.previewAssetDrop(video_b, 0, 996.0, width);
  REQUIRE(2, preview.value("valid").toBool() && preview.value("widthPx").toDouble() > 100.0 && preview.value("reason").toString().isEmpty());
  REQUIRE(3, std::abs(preview.value("startPx").toDouble() - 1000.0) < 0.5);
  REQUIRE(4, controller.dropAsset(video_b, 0, 996.0, width) && clips(controller) == 2);
  REQUIRE(5, clip_start(controller, 1) == QStringLiteral("00:00:04.000"));
  REQUIRE(6, controller.projectDirty() && controller.canUndo());
  controller.undoEdit();
  REQUIRE(7, clips(controller) == 1);                      // one undo step

  // Overlap: previewed as invalid with a reason, refused with the same message, nothing changes.
  const auto bad = controller.previewAssetDrop(video_b, 0, 400.0, width);
  REQUIRE(8, !bad.value("valid").toBool() && bad.value("reason").toString().contains(QStringLiteral("solapar")));
  REQUIRE(9, !controller.dropAsset(video_b, 0, 400.0, width) && clips(controller) == 1 && controller.errorMessage().contains(QStringLiteral("solapar")));
  REQUIRE(10, !controller.previewAssetDrop(video_b, 9, 10.0, width).value("valid").toBool());     // no such lane
  REQUIRE(11, !controller.dropAsset(video_b, -1, 10.0, width));

  // Audio: its own lane only. A video on an audio lane (and the reverse) is refused clearly.
  controller.openPath(q(root / "music.wav"));
  REQUIRE(12, wait_until([&] { return assets(controller) == 3 && !controller.busy(); }));
  const auto music = asset_id(controller, 2);
  const int clips_before = clips(controller);            // video + auto-placed music
  REQUIRE(13, clips_before == 2 && controller.timelineTracks().size() == 2);
  REQUIRE(14, controller.previewAssetDrop(video_b, 1, 100.0, width).value("reason").toString().contains(QStringLiteral("Separar audio")));
  REQUIRE(15, controller.previewAssetDrop(music, 0, 100.0, width).value("reason").toString().contains(QStringLiteral("pista de audio")));
  // The auto-placed music sits at the playhead (0..2 s): drop the second copy after it (2.5 s = 625 px).
  REQUIRE(16, controller.previewAssetDrop(music, 1, 625.0, width).value("valid").toBool());
  REQUIRE(17, controller.dropAsset(music, 1, 625.0, width) && clips(controller) == clips_before + 1);
  REQUIRE(18, !controller.dropAsset(music, 1, 625.0, width) && clips(controller) == clips_before + 1);   // now occupied
  controller.undoEdit();
  REQUIRE(19, clips(controller) == clips_before);
  controller.redoEdit();
  REQUIRE(20, clips(controller) == clips_before + 1);

  fs::remove_all(root);
  return 0;
}
