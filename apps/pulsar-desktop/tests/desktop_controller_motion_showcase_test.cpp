// MISIÓN 027: reproducible Motion Titles showcase. It deliberately uses the
// labelled e6 fixtures, not unverified ZONALERO footage. Every operation goes
// through DesktopController, so timeline, preview, save/reopen and export use
// the same public desktop path as an editor user.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>

#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QTimer>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>

namespace {
namespace fs = std::filesystem;

bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 180'000) {
  if (predicate()) return true;
  QEventLoop loop;
  QTimer poll, timeout;
  poll.setInterval(10);
  QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (predicate()) loop.quit(); });
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  poll.start(); timeout.setSingleShot(true); timeout.start(timeout_ms); loop.exec();
  return predicate();
}

QString asset_id(DesktopController& controller, const QString& name) {
  for (int row = 0; row < controller.mediaAssets()->rowCount(); ++row) {
    const auto index = controller.mediaAssets()->index(row, 0);
    if (index.data(MediaAssetListModel::NameRole).toString() == name)
      return index.data(MediaAssetListModel::MediaIdRole).toString();
  }
  return {};
}

void write_checksum(std::ofstream& report, const fs::path& path) {
  QFile file{QString::fromStdString(path.string())};
  if (!file.open(QIODevice::ReadOnly)) return;
  report << QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex().toStdString()
         << "  " << path.filename().string() << "\n";
}
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { \
  std::fprintf(stderr, "MOTION SHOWCASE step %d failed: %s | error='%s' export='%s'\n", step, #condition, \
    controller.errorMessage().toUtf8().constData(), controller.exportStatus().toUtf8().constData()); return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider provider;
  DesktopController controller{provider, {}};
  const auto fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  const auto root = fs::temp_directory_path() / ("pulsar-motion-showcase-" + std::to_string(QCoreApplication::applicationPid()));
  fs::remove_all(root); fs::create_directories(root);

  controller.openPath(QString::fromStdString((fixtures / "e6_video_a.mp4").string()));
  REQUIRE(1, wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  controller.openPaths({QString::fromStdString((fixtures / "e6_video_b.mp4").string()),
                        QString::fromStdString((fixtures / "e6_video_rational.mp4").string())});
  REQUIRE(2, wait_until([&] { return controller.mediaAssets()->rowCount() == 3 && !controller.busy(); }));

  // Four sequential clips create a 15.5 s real-video fixture timeline. The first
  // and third joins receive the two transition types used in production.
  const auto track = controller.agentTimelineState().front().toMap();
  const auto video_track = track.value("id").toULongLong();
  const auto first_clip = track.value("clips").toList().front().toMap().value("id").toULongLong();
  QString problem;
  const auto a_id = asset_id(controller, "e6_video_a.mp4").toULongLong();
  // e6_video_a is exactly four seconds. Starting this copy at 0.5 s leaves
  // 3.5 s of source audio/video, so never promise an out-of-range sample.
  const auto b = controller.agentAddClip(a_id, video_track, 4.0, 0.5, 3.5, &problem);
  const auto c = controller.agentAddClip(a_id, video_track, 7.5, 0.0, 4.0, &problem);
  const auto d = controller.agentAddClip(a_id, video_track, 11.5, 0.0, 4.0, &problem);
  REQUIRE(3, b && c && d);
  controller.selectTimelineClip(QString::number(first_clip)); controller.addCrossDissolve();
  REQUIRE(4, controller.selectedTransitionType() == "crossfade");
  controller.selectTimelineClip(QString::number(*c)); controller.addDipToBlack();
  REQUIRE(5, controller.selectedTransitionType() == "dip");

  struct ShowcaseTitle { int preset; double start; const char* main; const char* secondary; int in; int out; };
  const ShowcaseTitle showcase[] = {
    {0, 0.0,  "FD LABS",           "Build. Explore. Innovate.", 1, 1},
    {1, 3.0,  "ZONALERO",          "DEVLOG 003 — EN DESARROLLO", 5, 1},
    {2, 6.0,  "FRANCISCO DUARTE",  "FD LABS — DESARROLLO", 2, 2},
    {3, 9.0,  "01:38.545",         "VUELTA DE REFERENCIA", 5, 1},
    {4, 12.0, "APPELLA",           "ENTRENAMIENTOS", 1, 1},
  };
  for (int index = 0; index < 5; ++index) {
    const auto& item = showcase[index];
    REQUIRE(10 + index, controller.agentSeekSeconds(item.start, &problem));
    controller.addTitlePreset(item.preset);
    REQUIRE(20 + index, !controller.selectedTitleKey().isEmpty());
    controller.setSelectedTitleText(QString::fromUtf8(item.main));
    controller.setSelectedTitleGraphics(item.preset, QString::fromUtf8(item.secondary), QStringLiteral("#59D9FF"));
    controller.setSelectedTitleMotion(item.in, item.out, 0.35, 0.25);
    const auto info = controller.selectedTitle();
    REQUIRE(30 + index, info.value("preset").toInt() == item.preset && info.value("secondary").toString() == QString::fromUtf8(item.secondary));
    // Capture an entering and settled preview state: these are timeline seeks,
    // never QML-clock snapshots.
    REQUIRE(40 + index, controller.agentSeekSeconds(item.start + 0.05, &problem) && wait_until([&] { return !provider.image().isNull(); }));
    provider.image().save(QString::fromStdString((root / ("preset-" + std::to_string(index) + "-enter.png")).string()));
    REQUIRE(50 + index, controller.agentSeekSeconds(item.start + 0.75, &problem) && wait_until([&] { return !provider.image().isNull(); }));
    provider.image().save(QString::fromStdString((root / ("preset-" + std::to_string(index) + "-settled.png")).string()));
  }
  REQUIRE(60, controller.timelineTitles()->rowCount() == 5);

  const auto project = root / "zonalero-motion-titles-showcase.pulsar";
  REQUIRE(61, controller.saveProject(QString::fromStdString(project.string())));
  REQUIRE(62, controller.openProject(QString::fromStdString(project.string())) && wait_until([&] { return controller.hasVideoMedia() && !controller.busy(); }));
  REQUIRE(63, controller.timelineTitles()->rowCount() == 5);
  for (int index = 0; index < 5; ++index) {
    controller.selectTitle(controller.timelineTitles()->index(index, 0).data(TitleListModel::KeyRole).toString());
    REQUIRE(64 + index, controller.selectedTitle().value("preset").toInt() == showcase[index].preset);
  }

  const auto landscape = root / "zonalero-motion-titles-16x9.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(landscape.string()), QStringLiteral("720p"));
  REQUIRE(70, wait_until([&] { return !controller.exportRunning(); }) && fs::exists(landscape));
  controller.setCanvasPreset(QStringLiteral("portrait"));
  const auto portrait = root / "zonalero-motion-titles-9x16.mp4";
  controller.exportProjectWithPreset(QString::fromStdString(portrait.string()), QStringLiteral("720p"));
  REQUIRE(71, wait_until([&] { return !controller.exportRunning(); }) && fs::exists(portrait));
  const auto probe = pulsar::media::ffmpeg::FFmpegMediaProbe{}.probe(portrait);
  REQUIRE(72, !probe.video_streams.empty() && probe.video_streams.front().width == 720 && probe.video_streams.front().height == 1280);

  const auto evidence = qEnvironmentVariable("PULSAR_MOTION_TITLES_EVIDENCE_DIR");
  if (!evidence.isEmpty()) {
    const auto destination = fs::path{evidence.toStdString()}; fs::create_directories(destination);
    for (const auto& entry : fs::directory_iterator(root)) fs::copy_file(entry.path(), destination / entry.path().filename(), fs::copy_options::overwrite_existing);
    std::ofstream report{destination / "QA-REPORT.txt"};
    report << "MISIÓN 027 Motion Graphics Showcase\n"
           << "Media: labelled repository fixtures e6_video_a/b/rational; no unverified ZONALERO footage was substituted.\n"
           << "Presets: Classic, Hero, Lower Third, Stat Card, Chapter/Location.\n"
           << "Transitions: Crossfade at 4s; Dip to Black at 7.5s.\n"
           << "Validation: controller timeline seeks, preview captures, save/reopen, 16:9 and 9:16 exports.\n\nSHA256\n";
    write_checksum(report, destination / project.filename());
    write_checksum(report, destination / landscape.filename());
    write_checksum(report, destination / portrait.filename());
  }
  fs::remove_all(root);
  return 0;
}
