#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <filesystem>
#include <functional>
#include <vector>

namespace {

bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 30'000) {
  if (predicate()) return true;
  QEventLoop loop;
  QTimer poll;
  QTimer timeout;
  poll.setInterval(10);
  QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
    if (predicate()) loop.quit();
  });
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  poll.start();
  timeout.setSingleShot(true);
  timeout.start(timeout_ms);
  loop.exec();
  return predicate();
}

} // namespace

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider image_provider;
  DesktopController controller{image_provider, {}};
  const auto fixture = std::filesystem::path{PULSAR_SOURCE_DIR} /
                       "tests/fixtures/e6_video_a.mp4";
  const auto root = std::filesystem::temp_directory_path() / "pulsar-e7-controller-test";
  std::filesystem::create_directories(root);
  const auto destination = root / "desktop-export.mp4";
  const auto cancelled_destination = root / "desktop-export-cancelled.mp4";
  const auto duplicate_destination = root / "second.mp4";
  std::filesystem::remove(destination);
  std::filesystem::remove(cancelled_destination);
  std::filesystem::remove(duplicate_destination);

  controller.exportProject(QString::fromStdString(destination.string()));
  if (controller.exportRunning() || !controller.exportStatus().contains(QStringLiteral("No hay vídeo")))
    return 1;

  controller.openPath(QString::fromStdString(fixture.string()));
  if (!wait_until([&] { return controller.hasVideoMedia(); })) return 1;
  const bool dirty_before_export = controller.projectDirty();
  controller.exportProject(QString::fromStdString(cancelled_destination.string()));
  controller.cancelExport();
  if (!wait_until([&] { return !controller.exportRunning(); })) return 1;
  if (controller.exportStatus() != QStringLiteral("Exportación cancelada")) return 1;
  if (std::filesystem::exists(cancelled_destination)) return 1;

  std::vector<double> progress;
  QObject::connect(&controller, &DesktopController::exportProgressChanged, &controller,
                   [&] { progress.push_back(controller.exportProgress()); });
  controller.exportProject(QString::fromStdString(destination.string()));
  controller.exportProject(QString::fromStdString(duplicate_destination.string()));
  if (!wait_until([&] { return !controller.exportRunning(); })) return 1;
  if (controller.exportStatus() != QStringLiteral("Exportación completada")) return 1;
  if (!std::filesystem::is_regular_file(destination)) return 1;
  if (std::filesystem::exists(duplicate_destination)) return 1;
  if (controller.projectDirty() != dirty_before_export) return 1;
  if (progress.empty() || progress.back() != 1.0) return 1;
  for (std::size_t index = 1; index < progress.size(); ++index) {
    if (progress[index] < progress[index - 1]) return 1;
  }

  // Destruction owns the worker: this scope requests no detach and must return
  // only after the export thread has released all FFmpeg resources.
  {
    PreviewImageProvider shutdown_provider;
    DesktopController shutdown_controller{shutdown_provider, {}};
    shutdown_controller.openPath(QString::fromStdString(fixture.string()));
    if (!wait_until([&] { return shutdown_controller.hasVideoMedia(); })) return 1;
    shutdown_controller.exportProject(QString::fromStdString((root / "shutdown.mp4").string()));
  }

  std::filesystem::remove_all(root);
}
