#include "desktop_controller.hpp"
#include "preview_image_provider.hpp"
#include "waveform_quick_item.hpp"

#include <pulsar/media/ffmpeg/ffmpeg_waveform_source.hpp>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QUrl>
#include <qqml.h>

#include <iostream>
#include <filesystem>
#include <memory>
#include <optional>

int main(int argc, char** argv) {
  QGuiApplication app{argc, argv};
  app.setApplicationName(QStringLiteral("PULSAR"));
  app.setOrganizationName(QStringLiteral("PULSAR"));

  // Declared before the controller so reverse destruction tears down the
  // controller first: QQmlApplicationEngine (sole owner of image_provider)
  // is destroyed only after every PreviewImageProvider& user is gone.
  QQmlApplicationEngine engine;
  qmlRegisterType<WaveformQuickItem>("Pulsar.Desktop", 1, 0, "WaveformItem");

  auto* image_provider = new PreviewImageProvider{};
  DesktopController controller{
      *image_provider,
      [](const std::filesystem::path& path, int stream_index) {
        return std::make_unique<pulsar::media::ffmpeg::FFmpegWaveformSource>(path,
                                                                            stream_index);
      }};

  engine.addImportPath(QStringLiteral("qrc:/pulsar/imports"));
  engine.addImageProvider(QStringLiteral("pulsar-preview"), image_provider);
  engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);

  QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
    QCoreApplication::exit(1);
  }, Qt::QueuedConnection);
  engine.load(QUrl{QStringLiteral("qrc:/qml/Main.qml")});

  std::optional<QString> initial_path;
  std::optional<int> smoke_exit_ms;
  bool dump_performance_on_exit = false;
  for (int index = 1; index < argc; ++index) {
    const QString argument = QString::fromLocal8Bit(argv[index]);
    if (argument == QStringLiteral("--smoke-exit-ms") && index + 1 < argc) {
      bool ok = false;
      const int value = QString::fromLocal8Bit(argv[++index]).toInt(&ok);
      if (ok && value >= 0) {
        smoke_exit_ms = value;
      }
    } else if (argument == QStringLiteral("--dump-performance-on-exit")) {
      dump_performance_on_exit = true;
    } else if (!argument.startsWith(QLatin1String("--")) && !initial_path.has_value()) {
      initial_path = argument;
    }
  }

  if (initial_path.has_value()) {
    QTimer::singleShot(0, &controller, [&controller, initial_path] {
      controller.openPath(*initial_path);
    });
  }
  if (smoke_exit_ms.has_value()) {
    QTimer::singleShot(*smoke_exit_ms, &app, &QCoreApplication::quit);
  }

  const int exit_code = app.exec();
  if (dump_performance_on_exit) {
    std::cout << controller.performanceSnapshot().toStdString() << '\n';
  }
  // WaveformItem destruction calls back into the controller to detach. Destroy
  // QML roots while the controller is still alive; the engine itself remains
  // alive until after the controller, preserving image-provider ownership.
  const auto roots = engine.rootObjects();
  for (auto* root : roots) delete root;
  return exit_code;
}
