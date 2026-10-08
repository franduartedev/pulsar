// Batch import: ordered, progressive, per-file errors, duplicates, cancellation, 20 files.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 60'000) {
  if (predicate()) return true;
  QEventLoop loop;
  QTimer poll, timeout;
  poll.setInterval(5);
  QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (predicate()) loop.quit(); });
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  poll.start();
  timeout.setSingleShot(true);
  timeout.start(timeout_ms);
  loop.exec();
  return predicate();
}
int assets(DesktopController& c) { return c.mediaAssets()->rowCount(); }
int clips(DesktopController& c) { return c.timelineClips()->rowCount(); }
bool idle(DesktopController& c) { return !c.busy() && c.importPending() == 0; }
QStringList q(const std::vector<fs::path>& paths) {
  QStringList list;
  for (const auto& p : paths) list << QString::fromStdString(p.string());
  return list;
}
QString asset_name(DesktopController& c, int row) {
  return c.mediaAssets()->index(row, 0).data(MediaAssetListModel::NameRole).toString();
}
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { \
  std::fprintf(stderr, "BATCH step %d failed: %s | assets=%d clips=%d busy=%d pending=%d error='%s'\n", step, #condition, \
               assets(controller), clips(controller), controller.busy() ? 1 : 0, controller.importPending(), \
               controller.errorMessage().toUtf8().constData()); \
  return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider provider;
  const auto fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  const auto root = fs::temp_directory_path() / "pulsar-batch-test";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path sources[] = {fixtures / "e6_video_a.mp4", fixtures / "e6_video_b.mp4", fixtures / "e6_video_rational.mp4"};
  std::vector<fs::path> twenty;
  for (int i = 0; i < 20; ++i) {        // zero padded so the expected order is the name order
    char name[32];
    std::snprintf(name, sizeof name, "clip_%02d.mp4", i);
    fs::copy_file(sources[i % 3], root / name);
    twenty.push_back(root / name);
  }

  {  // 1 file: same behaviour as the single import (first video lands on VIDEO 1)
    DesktopController controller{provider, {}};
    controller.openPaths(q({twenty[0]}));
    REQUIRE(1, wait_until([&] { return assets(controller) == 1 && idle(controller); }));
    REQUIRE(2, clips(controller) == 1 && controller.errorMessage().isEmpty());
  }
  {  // 20 files: in order, progressive, exactly one clip auto-placed, no leftovers
    DesktopController controller{provider, {}};
    int changes = 0;
    QObject::connect(&controller, &DesktopController::assetsChanged, [&] { ++changes; });
    controller.openPaths(q(twenty));
    REQUIRE(3, controller.busy() && controller.importTotal() == 20);
    REQUIRE(4, wait_until([&] { return assets(controller) == 20 && idle(controller); }));
    REQUIRE(5, changes > 1);                                             // assets appeared one by one
    for (int i = 0; i < 20; ++i) {
      char name[32];
      std::snprintf(name, sizeof name, "clip_%02d.mp4", i);
      REQUIRE(6, asset_name(controller, i) == QString::fromLatin1(name));
    }
    REQUIRE(7, clips(controller) == 1 && controller.errorMessage().isEmpty());
    // Same files again: nothing new, nothing broken.
    controller.openPaths(q({twenty[0], twenty[1]}));
    REQUIRE(8, wait_until([&] { return idle(controller); }) && assets(controller) == 20);
  }
  {  // a bad file among valid ones costs only itself
    DesktopController controller{provider, {}};
    const auto bad = root / "not_media.mp4";
    { std::ofstream out{bad}; out << "this is not a video"; }
    controller.openPaths(q({twenty[0], bad, twenty[1], root / "missing.mp4", twenty[2]}));
    REQUIRE(9, wait_until([&] { return idle(controller) && assets(controller) == 3; }));
    REQUIRE(10, controller.errorMessage().contains(QStringLiteral("2 de 5")) && !controller.errorMessage().contains(QStringLiteral("std::")));
  }
  {  // duplicates inside one batch are imported once
    DesktopController controller{provider, {}};
    controller.openPaths(q({twenty[0], twenty[0], twenty[1]}));
    REQUIRE(11, wait_until([&] { return idle(controller) && assets(controller) == 2; }));
  }
  {  // cancel: stops, returns to idle, and a later batch still works
    DesktopController controller{provider, {}};
    controller.openPaths(q(twenty));
    controller.cancelImport();
    REQUIRE(12, !controller.busy() && controller.importPending() == 0);
    QEventLoop loop; QTimer::singleShot(400, &loop, &QEventLoop::quit); loop.exec();   // in-flight result is dropped
    REQUIRE(13, assets(controller) < 20 && idle(controller));
    const int before = assets(controller);
    controller.openPaths(q({twenty[5], twenty[6]}));
    REQUIRE(14, wait_until([&] { return idle(controller) && assets(controller) >= before + 1; }));
  }
  {  // A folder imports every media file inside it (name order), ignoring the rest.
    DesktopController controller{provider, {}};
    const auto folder = root / "shoot";
    fs::create_directories(folder);
    fs::copy_file(sources[1], folder / "b_second.mp4");
    fs::copy_file(sources[0], folder / "a_first.MP4");
    { std::ofstream out{folder / "notes.txt"}; out << "not media"; }
    controller.openPath(q({folder}).front());
    REQUIRE(15, wait_until([&] { return assets(controller) == 2 && idle(controller); }));
    REQUIRE(16, asset_name(controller, 0) == QStringLiteral("a_first.MP4") && asset_name(controller, 1) == QStringLiteral("b_second.mp4"));
    controller.openPaths(q({root / "empty_dir_does_not_exist"}));          // not a folder, not a file: reported, no crash
    REQUIRE(17, wait_until([&] { return idle(controller); }));
  }
  fs::remove_all(root);
  return 0;
}
