#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
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

std::string read_bytes(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

} // namespace

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider image_provider;
  DesktopController controller{image_provider, {}};
  const auto fixture = std::filesystem::path{PULSAR_SOURCE_DIR} /
                       "tests/fixtures/e6_video_a.mp4";
  const auto root = std::filesystem::temp_directory_path() / "pulsar-e7-controller-test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto source = root / "source.mp4";
  std::filesystem::copy_file(fixture, source);
  const auto destination = root / "desktop-export.mp4";
  const auto cancelled_destination = root / "desktop-export-cancelled.mp4";
  const auto duplicate_destination = root / "second.mp4";
  std::filesystem::remove(destination);
  std::filesystem::remove(cancelled_destination);
  std::filesystem::remove(duplicate_destination);

  controller.exportProject(QString::fromStdString(destination.string()));
  if (controller.exportRunning() || !controller.exportStatus().contains(QStringLiteral("No hay vídeo")))
    return 1;

  controller.openPath(QString::fromStdString(source.string()));
  if (!wait_until([&] { return controller.hasVideoMedia(); })) return 1;
  const auto source_before = read_bytes(source);

  // A normalized spelling of an active source is rejected synchronously,
  // before any export worker or temporary output is created.
  controller.exportProject(QString::fromStdString((root / "." / "source.mp4").string()));
  if (controller.exportRunning()) return 1;
  if (!controller.exportStatus().contains(QStringLiteral("archivo multimedia usado"))) return 1;
  if (!controller.errorMessage().contains(QStringLiteral("No se puede exportar"))) return 1;
  if (read_bytes(source) != source_before) return 1;

  // Existing filesystem aliases resolve to the same source and are protected too.
  const auto source_alias = root / "source-alias.mp4";
  std::filesystem::create_symlink(source.filename(), source_alias);
  controller.exportProject(QString::fromStdString(source_alias.string()));
  if (controller.exportRunning() || read_bytes(source) != source_before) return 1;
  const auto video_key = controller.timelineClips()->index(0, 0)
      .data(TimelineClipListModel::ClipKeyRole).toString();
  controller.selectTimelineClip(video_key);
  if (!controller.canSeparateAudio()) return 1;
  controller.separateSelectedAudio();
  if (!controller.errorMessage().isEmpty() || controller.timelineClips()->rowCount() != 2) return 1;
  const bool dirty_before_export = controller.projectDirty();
  controller.exportProject(QString::fromStdString(cancelled_destination.string()));
  controller.cancelExport();
  if (!wait_until([&] { return !controller.exportRunning(); })) return 1;
  if (controller.exportStatus() != QStringLiteral("Exportación cancelada")) return 1;
  if (std::filesystem::exists(cancelled_destination)) return 1;

  std::vector<double> progress;
  QObject::connect(&controller, &DesktopController::exportProgressChanged, &controller,
                   [&] { progress.push_back(controller.exportProgress()); });
  { std::ofstream previous{destination, std::ios::binary}; previous << "ordinary previous export"; }
  controller.exportProject(QString::fromStdString(destination.string()));
  controller.exportProject(QString::fromStdString(duplicate_destination.string()));
  if (!wait_until([&] { return !controller.exportRunning(); })) return 1;
  if (controller.exportStatus() != QStringLiteral("Exportación completada")) return 1;
  if (!std::filesystem::is_regular_file(destination)) return 1;
  if (read_bytes(destination) == "ordinary previous export") return 1;
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
