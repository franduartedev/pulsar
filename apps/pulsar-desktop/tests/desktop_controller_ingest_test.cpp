// QA-001 desktop integration: several real videos ingested into VIDEO 1 through
// the controller API the Library uses, with undo/redo, overlap refusal, audio
// separation and a save/reopen round trip. Fails with a distinct exit code per
// step so a red run says what broke.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>

namespace {

bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 30'000) {
  if (predicate()) return true;
  QEventLoop loop;
  QTimer poll;
  QTimer timeout;
  poll.setInterval(10);
  QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (predicate()) loop.quit(); });
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  poll.start();
  timeout.setSingleShot(true);
  timeout.start(timeout_ms);
  loop.exec();
  return predicate();
}

void write_u32(std::ofstream& out, std::uint32_t v) { for (int i = 0; i < 4; ++i) out.put(static_cast<char>((v >> (8 * i)) & 0xff)); }
void write_u16(std::ofstream& out, std::uint16_t v) { for (int i = 0; i < 2; ++i) out.put(static_cast<char>((v >> (8 * i)) & 0xff)); }

// One second of mono 48 kHz PCM16 sine: a real audio-only asset without a fixture file.
void write_wav(const std::filesystem::path& path) {
  constexpr std::uint32_t rate = 48'000;
  std::ofstream out{path, std::ios::binary};
  out.write("RIFF", 4); write_u32(out, 36 + rate * 2); out.write("WAVEfmt ", 8);
  write_u32(out, 16); write_u16(out, 1); write_u16(out, 1); write_u32(out, rate);
  write_u32(out, rate * 2); write_u16(out, 2); write_u16(out, 16);
  out.write("data", 4); write_u32(out, rate * 2);
  for (std::uint32_t i = 0; i < rate; ++i) {
    write_u16(out, static_cast<std::uint16_t>(static_cast<std::int16_t>(8000.0 * std::sin(2.0 * 3.14159265358979 * 440.0 * i / rate))));
  }
}

int assets(DesktopController& c) { return c.mediaAssets()->rowCount(); }
int clips(DesktopController& c) { return c.timelineClips()->rowCount(); }
QString asset_id(DesktopController& c, int row) {
  return c.mediaAssets()->index(row, 0).data(MediaAssetListModel::MediaIdRole).toString();
}
QString asset_kind(DesktopController& c, int row) {
  return c.mediaAssets()->index(row, 0).data(MediaAssetListModel::KindRole).toString();
}
bool import_and_wait(DesktopController& c, const std::filesystem::path& path, int expected_assets) {
  c.openPath(QString::fromStdString(path.string()));
  return wait_until([&] { return assets(c) == expected_assets && !c.busy(); });
}

} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { \
  std::fprintf(stderr, "QA-001 step %d failed: %s | error='%s' assets=%d clips=%d\n", step, #condition, \
               controller.errorMessage().toUtf8().constData(), assets(controller), clips(controller)); \
  return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider image_provider;
  DesktopController controller{image_provider, {}};
  const auto fixtures = std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  const auto a = fixtures / "e6_video_a.mp4";
  const auto b = fixtures / "e6_video_b.mp4";
  const auto c = fixtures / "e6_video_rational.mp4";
  const auto root = std::filesystem::temp_directory_path() / "pulsar-qa001-controller-test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto wav = root / "tone.wav";
  write_wav(wav);
  const QString overlap_message = QStringLiteral("No hay espacio suficiente en la pista de video.");

  // First video: auto-placed on VIDEO 1 and listed in the Library.
  REQUIRE(1, import_and_wait(controller, a, 1));
  REQUIRE(2, wait_until([&] { return controller.hasVideoMedia(); }));
  REQUIRE(3, clips(controller) == 1);
  REQUIRE(4, asset_kind(controller, 0) == QStringLiteral("video"));
  REQUIRE(5, controller.selectedAssetId() == asset_id(controller, 0));
  const QString id_a = asset_id(controller, 0);

  // Second video: listed, selected, but NOT placed automatically.
  REQUIRE(6, import_and_wait(controller, b, 2));
  REQUIRE(7, clips(controller) == 1);
  const QString id_b = asset_id(controller, 1);
  REQUIRE(8, controller.selectedAssetId() == id_b);

  // First video is not duplicated by Insertar; overlap is refused cleanly.
  controller.insertAssetAtPlayhead(id_a);
  REQUIRE(9, clips(controller) == 1 && controller.errorMessage() == overlap_message);
  controller.seekToStart();
  const bool dirty_before = controller.projectDirty();
  controller.insertAssetAtPlayhead(id_b);
  REQUIRE(10, clips(controller) == 1 && controller.errorMessage() == overlap_message);
  REQUIRE(11, controller.projectDirty() == dirty_before);

  // Insertar at the end of A (playhead at the end of the program).
  controller.seekToEnd();
  controller.insertAssetAtPlayhead(id_b);
  REQUIRE(12, clips(controller) == 2 && controller.errorMessage().isEmpty());
  REQUIRE(13, controller.canUndo());
  controller.undoEdit();
  REQUIRE(14, clips(controller) == 1 && assets(controller) == 2);
  controller.redoEdit();
  REQUIRE(15, clips(controller) == 2);

  // Third video appended after the last VIDEO 1 clip.
  REQUIRE(16, import_and_wait(controller, c, 3));
  const QString id_c = asset_id(controller, 2);
  REQUIRE(17, clips(controller) == 2);
  controller.appendAssetToVideoTrack(id_c);
  REQUIRE(18, clips(controller) == 3 && controller.errorMessage().isEmpty());
  controller.undoEdit();
  REQUIRE(19, clips(controller) == 2);
  controller.redoEdit();
  REQUIRE(20, clips(controller) == 3);

  // Audio keeps its own flow: listed as audio, placed on an audio track, never on VIDEO 1.
  REQUIRE(21, import_and_wait(controller, wav, 4));
  REQUIRE(22, asset_kind(controller, 3) == QStringLiteral("audio"));
  const int clips_with_audio = clips(controller);
  REQUIRE(23, clips_with_audio == 4);
  controller.appendAssetToVideoTrack(asset_id(controller, 3));
  // Audio is never placed on VIDEO 1: it goes to an audio track (one undo step), no error.
  REQUIRE(24, clips(controller) == clips_with_audio + 1 && controller.errorMessage().isEmpty());
  controller.undoEdit();
  REQUIRE(24, clips(controller) == clips_with_audio);

  // Save, reopen: assets, clips and selection survive.
  const auto project = root / "multi.pulsar";
  controller.saveProject(QString::fromStdString(project.string()));
  REQUIRE(25, !controller.projectDirty() && std::filesystem::exists(project));
  controller.openProject(QString::fromStdString(project.string()));
  REQUIRE(26, wait_until([&] { return assets(controller) == 4 && clips(controller) == clips_with_audio; }));
  REQUIRE(27, asset_id(controller, 0) == id_a && asset_id(controller, 2) == id_c);
  REQUIRE(28, !controller.projectDirty());
  // And a fourth ingest still works after reload.
  controller.appendAssetToVideoTrack(id_b);
  REQUIRE(29, clips(controller) == clips_with_audio + 1 && controller.projectDirty());

  // Zero-gap append: the appended clip starts exactly where the previous video clip ends.
  {
    const auto end_text = [&](int row) { return controller.timelineClips()->index(row, 0).data(TimelineClipListModel::EndRole).toString(); };
    const auto start_text = [&](int row) { return controller.timelineClips()->index(row, 0).data(TimelineClipListModel::StartRole).toString(); };
    REQUIRE(30, end_text(0) == start_text(1) && end_text(1) == start_text(2));
  }

  // Ruler: round major labels, bounded tick count, finer step when zoomed; Ajustar returns to full view.
  {
    const auto majors = [&] {
      QStringList labels;
      for (const auto& tick : controller.timelineRulerTicks()) {
        const auto map = tick.toMap();
        if (map.value("major").toBool()) labels << map.value("label").toString();
      }
      return labels;
    };
    REQUIRE(31, controller.timelineRulerTicks().size() <= 60 && majors().size() >= 3);
    REQUIRE(32, majors().first() == QStringLiteral("0:00"));
    controller.setTimelineZoom(60.0);
    REQUIRE(33, !majors().isEmpty() && controller.timelineRulerTicks().size() <= 60 && majors().first().contains(QLatin1Char('.')));
    controller.fitTimeline();
    REQUIRE(34, controller.timelineZoom() == 1.0 && majors().first() == QStringLiteral("0:00"));
  }

  // Live move preview: pure (no edit), stays at the origin when the pointer has not moved.
  {
    const auto key = controller.timelineClips()->index(1, 0).data(TimelineClipListModel::ClipKeyRole).toString();
    const int clips_before = clips(controller);
    const auto preview = controller.previewTimelineMove(key, 500.0, 500.0, 1000.0);
    REQUIRE(35, std::abs(preview.value("offset").toDouble()) < 0.5 && clips(controller) == clips_before);
  }

  // Separar audio: one grouped, undoable, persistent operation on the selected video clip.
  {
    const auto key = controller.timelineClips()->index(0, 0).data(TimelineClipListModel::ClipKeyRole).toString();
    controller.selectTimelineClip(key);
    REQUIRE(36, controller.canSeparateAudio());
    const int before = clips(controller);
    controller.separateSelectedAudio();
    REQUIRE(37, clips(controller) == before + 1 && controller.errorMessage().isEmpty());
    REQUIRE(38, !controller.canSeparateAudio());          // selection moved to the new audio clip
    controller.selectTimelineClip(key);
    REQUIRE(39, !controller.canSeparateAudio());          // the video clip is now silenced
    controller.separateSelectedAudio();
    REQUIRE(40, clips(controller) == before + 1 && !controller.errorMessage().isEmpty());
    controller.undoEdit();                                 // ONE undo reverts the whole separation
    REQUIRE(41, clips(controller) == before);
    controller.selectTimelineClip(key);
    REQUIRE(42, controller.canSeparateAudio());
    controller.redoEdit();
    REQUIRE(43, clips(controller) == before + 1);
    const auto project2 = root / "separated.pulsar";
    controller.saveProject(QString::fromStdString(project2.string()));
    REQUIRE(44, !controller.projectDirty());
    controller.openProject(QString::fromStdString(project2.string()));
    REQUIRE(45, wait_until([&] { return clips(controller) == before + 1; }));
    controller.selectTimelineClip(key);
    REQUIRE(46, !controller.canSeparateAudio());          // persisted: video clip still silenced
  }

  std::filesystem::remove_all(root);
  return 0;
}
