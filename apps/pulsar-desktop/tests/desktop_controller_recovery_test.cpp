// Crash recovery: dirty -> snapshot, clean -> none, restart offers it, corrupt/stale ignored,
// save/discard clean up, an unusable folder never hurts.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <chrono>
#include <cstdio>
#include <cstdlib>
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
QString q(const fs::path& p) { return QString::fromStdString(p.string()); }
int clips(DesktopController& c) { return c.timelineClips()->rowCount(); }
bool has_files(const fs::path& dir) { return fs::exists(dir / "session.meta") || fs::exists(dir / "session.pulsar-recovery"); }
void write_text(const fs::path& path, const std::string& text) { std::ofstream out{path, std::ios::binary}; out << text; }

void set_environment(const char* name, const std::string& value) {
#if defined(_WIN32)
  _putenv_s(name, value.c_str());
#else
  setenv(name, value.c_str(), 1);
#endif
}

struct Session {
  PreviewImageProvider provider;
  std::unique_ptr<DesktopController> controller;
  explicit Session(const fs::path& recovery_dir) {
    set_environment("PULSAR_RECOVERY_DIR", recovery_dir.string());
    controller = std::make_unique<DesktopController>(provider, DesktopController::WaveformSourceCreator{});
  }
};
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { std::fprintf(stderr, "RECOVERY step %d failed: %s\n", step, #condition); return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  const auto fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  const auto root = fs::temp_directory_path() / "pulsar-recovery-test";
  fs::remove_all(root);
  fs::create_directories(root);
  const auto recovery = root / "recovery";
  const auto media = root / "a.mp4";
  fs::copy_file(fixtures / "e6_video_a.mp4", media);

  {  // A clean project never leaves a snapshot; an empty app has nothing to recover.
    Session s{recovery};
    auto& c = *s.controller;
    REQUIRE(1, !c.recoveryAvailable());
    c.autosaveNow(); c.flushRecovery();
    REQUIRE(2, !has_files(recovery));
    c.openPath(q(media));
    REQUIRE(3, wait_until([&] { return c.hasVideoMedia() && !c.busy(); }));
    REQUIRE(4, c.projectDirty());
    c.autosaveNow(); c.flushRecovery();
    REQUIRE(5, has_files(recovery) && c.recoveryWriteCount() == 1);       // dirty -> snapshot
    c.autosaveNow(); c.flushRecovery();
    REQUIRE(6, c.recoveryWriteCount() == 1);                              // unchanged -> no rewrite
    c.addTitle();
    c.autosaveNow(); c.flushRecovery();
    REQUIRE(7, c.recoveryWriteCount() == 2);                              // new edit -> new snapshot
    c.createColorCard(QStringLiteral("#336699"));                         // a generated card and a fade must survive a crash too
    c.selectTimelineClip(c.timelineClips()->index(0, 0).data(TimelineClipListModel::ClipKeyRole).toString());
    c.setSelectedClipFades(0.5, 1.0);
    c.autosaveNow(); c.flushRecovery();
    // The controller is destroyed WITHOUT a clean shutdown of the project: a "crash".
  }
  {  // Restart: the session is offered and comes back unsaved, titles included.
    Session s{recovery};
    auto& c = *s.controller;
    REQUIRE(8, c.recoveryAvailable() && c.recoveryDescription().contains(QStringLiteral("sin título")));
    REQUIRE(9, c.restoreRecovery());
    REQUIRE(10, clips(c) == 2 && c.timelineTitles()->rowCount() == 1 && c.projectDirty() && c.activeProjectPath().isEmpty());
    c.selectTimelineClip(c.timelineClips()->index(0, 0).data(TimelineClipListModel::ClipKeyRole).toString());
    REQUIRE(110, c.selectedClipFadeIn() == 0.5 && c.selectedClipFadeOut() == 1.0);
    c.selectTimelineClip(c.timelineClips()->index(1, 0).data(TimelineClipListModel::ClipKeyRole).toString());
    REQUIRE(111, c.selectedClipKind() == QStringLiteral("color"));
    REQUIRE(11, has_files(recovery) && !c.recoveryAvailable());           // kept until saved or discarded
    // A normal save makes the snapshot unnecessary.
    REQUIRE(12, c.saveProject(q(root / "work.pulsar")));
    c.flushRecovery();
    REQUIRE(13, !has_files(recovery) && !c.projectDirty());
  }
  {  // Explicit discard removes it for good.
    Session s{recovery};
    auto& c = *s.controller;
    c.openPath(q(media));
    REQUIRE(14, wait_until([&] { return c.hasVideoMedia() && !c.busy(); }));
    c.autosaveNow(); c.flushRecovery();
    REQUIRE(15, has_files(recovery));
  }
  {
    Session s{recovery};
    auto& c = *s.controller;
    REQUIRE(16, c.recoveryAvailable());
    c.discardRecovery(); c.flushRecovery();
    REQUIRE(17, !c.recoveryAvailable() && !has_files(recovery));
  }
  {
    Session s{recovery};
    REQUIRE(18, !s.controller->recoveryAvailable());
  }
  {  // Recovery of a saved project: offered while the .pulsar is older, ignored once it is newer.
    const auto project = root / "saved.pulsar";
    {
      Session s{recovery};
      auto& c = *s.controller;
      c.openPath(q(media));
      REQUIRE(19, wait_until([&] { return c.hasVideoMedia() && !c.busy(); }));
      REQUIRE(20, c.saveProject(q(project)));
      c.addTitle();                                                       // unsaved change after the save
      c.autosaveNow(); c.flushRecovery();
      REQUIRE(21, has_files(recovery));
    }
    {
      Session s{recovery};
      auto& c = *s.controller;
      REQUIRE(22, c.recoveryAvailable() && c.recoveryDescription().contains(QStringLiteral("saved.pulsar")));
      REQUIRE(23, c.restoreRecovery() && c.timelineTitles()->rowCount() == 1 && c.projectDirty() && c.activeProjectPath() == q(project));
      c.autosaveNow(); c.flushRecovery();
    }
    fs::last_write_time(project, fs::file_time_type::clock::now() + std::chrono::hours{1});   // saved later than the snapshot
    {
      Session s{recovery};
      REQUIRE(24, !s.controller->recoveryAvailable() && !has_files(recovery));
    }
  }
  {  // Corrupt snapshot / corrupt metadata: startup is never blocked.
    {
      Session s{recovery};
      auto& c = *s.controller;
      c.openPath(q(media));
      REQUIRE(25, wait_until([&] { return c.hasVideoMedia() && !c.busy(); }));
      c.autosaveNow(); c.flushRecovery();
    }
    write_text(recovery / "session.pulsar-recovery", "PULSAR_PROJECT 2\nthis is damaged\n");
    {
      Session s{recovery};
      REQUIRE(26, !s.controller->recoveryAvailable() && !has_files(recovery));
      REQUIRE(27, s.controller->errorMessage().isEmpty());
    }
    fs::create_directories(recovery);
    write_text(recovery / "session.meta", "garbage\n");
    write_text(recovery / "session.pulsar-recovery", "garbage\n");
    {
      Session s{recovery};
      REQUIRE(28, !s.controller->recoveryAvailable());
    }
    fs::remove_all(recovery);
  }
  {  // An unusable folder (a file where the directory should be): no crash, editing carries on.
    const auto blocked = root / "blocked";
    write_text(blocked, "i am a file");
    Session s{blocked};
    auto& c = *s.controller;
    c.openPath(q(media));
    REQUIRE(29, wait_until([&] { return c.hasVideoMedia() && !c.busy(); }));
    c.autosaveNow(); c.flushRecovery();
    REQUIRE(30, c.recoveryWriteCount() == 0 && clips(c) == 1 && c.errorMessage().isEmpty());
    REQUIRE(31, fs::is_regular_file(blocked));
  }
  {  // Shutdown right after a write request never hangs or corrupts: the file is whole or absent.
    Session s{recovery};
    auto& c = *s.controller;
    c.openPath(q(media));
    REQUIRE(32, wait_until([&] { return c.hasVideoMedia() && !c.busy(); }));
    c.autosaveNow();
  }
  {
    Session s{recovery};
    auto& c = *s.controller;
    if (c.recoveryAvailable()) REQUIRE(33, c.restoreRecovery() && clips(c) == 1);   // complete or nothing
  }
  fs::remove_all(root);
  return 0;
}
