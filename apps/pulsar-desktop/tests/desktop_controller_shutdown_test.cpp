// Shutdown safety: a DesktopController can be destroyed in any state without
// deadlock, crash (run it under ASan/UBSan) or leaked threads. Each scenario
// builds a fresh controller; the thread count must not grow between rounds.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>

namespace {
namespace fs = std::filesystem;

bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 30'000) {
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
void spin(int milliseconds) {
  QEventLoop loop;
  QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
  loop.exec();
}
int thread_count() {
  std::ifstream status{"/proc/self/status"};
  for (std::string line; std::getline(status, line);) {
    if (line.rfind("Threads:", 0) == 0) return std::stoi(line.substr(8));
  }
  return -1;
}
QString q(const fs::path& p) { return QString::fromStdString(p.string()); }

struct Context {
  PreviewImageProvider provider;
  fs::path video_a, video_b, project;
};

// Each scenario returns false on a functional failure (hangs/crashes are caught by the harness).
bool scenario_idle(Context& c) { DesktopController controller{c.provider, {}}; (void)controller; return true; }

bool scenario_import_then_immediate_destroy(Context& c) {
  DesktopController controller{c.provider, {}};
  controller.openPath(q(c.video_a));   // worker may still be probing when we die
  return true;
}
bool scenario_playing(Context& c) {
  DesktopController controller{c.provider, {}};
  controller.openPath(q(c.video_a));
  if (!wait_until([&] { return controller.hasVideoMedia(); })) return false;
  controller.togglePlayback();
  spin(300);
  return controller.playing();
}
bool scenario_after_seek(Context& c) {
  DesktopController controller{c.provider, {}};
  controller.openPath(q(c.video_a));
  if (!wait_until([&] { return controller.hasVideoMedia(); })) return false;
  controller.seekNormalized(0.5);
  controller.seekToEnd();
  controller.seekToStart();
  spin(50);
  return true;
}
bool scenario_playing_then_seek_mid_flight(Context& c) {
  DesktopController controller{c.provider, {}};
  controller.openPath(q(c.video_a));
  if (!wait_until([&] { return controller.hasVideoMedia(); })) return false;
  controller.togglePlayback();
  controller.seekNormalized(0.8);       // seek while the audio/video workers are streaming
  return true;
}
bool scenario_export_running(Context& c) {
  DesktopController controller{c.provider, {}};
  controller.openPath(q(c.video_a));
  if (!wait_until([&] { return controller.hasVideoMedia(); })) return false;
  controller.exportProject(q(c.project.parent_path() / "shutdown-export.mp4"));
  spin(150);                            // let it get going, then destroy mid-export
  return true;
}
bool scenario_export_cancelled(Context& c) {
  DesktopController controller{c.provider, {}};
  controller.openPath(q(c.video_a));
  if (!wait_until([&] { return controller.hasVideoMedia(); })) return false;
  controller.exportProject(q(c.project.parent_path() / "cancel-export.mp4"));
  controller.cancelExport();
  return true;                          // destroyed without waiting for the cancel to land
}
bool scenario_open_project(Context& c) {
  DesktopController controller{c.provider, {}};
  if (!controller.openProject(q(c.project))) return false;
  controller.togglePlayback();
  spin(100);
  return true;
}

} // namespace

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  Context context;
  const auto fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  const auto root = fs::temp_directory_path() / "pulsar-shutdown-test";
  fs::remove_all(root);
  fs::create_directories(root);
  context.video_a = fixtures / "e6_video_a.mp4";
  context.video_b = fixtures / "e6_video_b.mp4";
  context.project = root / "p.pulsar";
  {  // a saved project to reopen
    DesktopController controller{context.provider, {}};
    controller.openPath(q(context.video_a));
    if (!wait_until([&] { return controller.hasVideoMedia(); })) return 1;
    if (!controller.saveProject(q(context.project))) return 2;
  }

  struct Named { const char* name; bool (*run)(Context&); };
  const Named scenarios[] = {
      {"idle", scenario_idle},
      {"import-then-destroy", scenario_import_then_immediate_destroy},
      {"playing", scenario_playing},
      {"after-seek", scenario_after_seek},
      {"playing-seek", scenario_playing_then_seek_mid_flight},
      {"export-running", scenario_export_running},
      {"export-cancelled", scenario_export_cancelled},
      {"open-project", scenario_open_project},
  };
  int baseline = -1;
  for (int round = 0; round < 3; ++round) {
    for (const auto& scenario : scenarios) {
      if (!scenario.run(context)) {
        std::fprintf(stderr, "SHUTDOWN scenario '%s' failed (round %d)\n", scenario.name, round);
        return 10 + round;
      }
      spin(20);  // flush queued callbacks addressed to the dead controller (must be harmless)
    }
    if (round == 0) baseline = thread_count();
    else if (thread_count() > baseline) {
      std::fprintf(stderr, "SHUTDOWN thread count grew: %d -> %d (round %d)\n", baseline, thread_count(), round);
      return 20 + round;
    }
  }
  fs::remove_all(root);
  return 0;
}
