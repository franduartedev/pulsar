// Release-safety integration test: save/open failures never lose or corrupt the
// current project, missing media opens as "offline" without crashing, and export
// failures never replace a good file. Each step has its own exit code.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
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
std::string read_file(const fs::path& path) {
  std::ifstream in{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}
void write_file(const fs::path& path, const std::string& bytes) {
  std::ofstream out{path, std::ios::binary};
  out << bytes;
}
int assets(DesktopController& c) { return c.mediaAssets()->rowCount(); }
int clips(DesktopController& c) { return c.timelineClips()->rowCount(); }
bool asset_available(DesktopController& c, int row) {
  return c.mediaAssets()->index(row, 0).data(MediaAssetListModel::AvailableRole).toBool();
}
bool clip_missing(DesktopController& c, int row) {
  return c.timelineClips()->index(row, 0).data(TimelineClipListModel::MissingRole).toBool();
}
bool import_and_wait(DesktopController& c, const fs::path& path, int expected) {
  c.openPath(QString::fromStdString(path.string()));
  return wait_until([&] { return assets(c) == expected && !c.busy(); });
}
QString q(const fs::path& p) { return QString::fromStdString(p.string()); }
// A user-facing message must not leak internals.
bool is_human(const QString& text) {
  if (text.isEmpty()) return false;
  for (const char* leak : {"cannot ", "std::", "exception", "tmp", ".pulsar.", "invalid_argument", "line "}) {
    if (text.contains(QLatin1String(leak))) return false;
  }
  return true;
}

} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { \
  std::fprintf(stderr, "SAFETY step %d failed: %s | error='%s' assets=%d clips=%d dirty=%d\n", step, #condition, \
               controller.errorMessage().toUtf8().constData(), assets(controller), clips(controller), \
               controller.projectDirty() ? 1 : 0); \
  return step; } } while (false)

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider image_provider;
  DesktopController controller{image_provider, {}};
  const auto fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  const auto root = fs::temp_directory_path() / "pulsar-safety-test";
  fs::remove_all(root);
  fs::create_directories(root / "media");
  fs::copy_file(fixtures / "e6_video_a.mp4", root / "media/a.mp4");
  fs::copy_file(fixtures / "e6_video_b.mp4", root / "media/b.mp4");

  // ---- Save safety --------------------------------------------------------
  REQUIRE(1, import_and_wait(controller, root / "media/a.mp4", 1));
  REQUIRE(2, wait_until([&] { return controller.hasVideoMedia(); }) && controller.projectDirty());
  const auto bad_parent = root / "not-a-folder.txt";
  write_file(bad_parent, "x");
  REQUIRE(3, !controller.saveProject(q(bad_parent / "p.pulsar")));      // parent is a file
  REQUIRE(4, controller.projectDirty() && controller.activeProjectPath().isEmpty() && is_human(controller.errorMessage()));
  REQUIRE(5, !controller.saveProject(q(root / "missing-dir/sub/p.pulsar")));
  REQUIRE(6, controller.projectDirty() && clips(controller) == 1);
  const auto project = root / "one.pulsar";
  REQUIRE(7, controller.saveProject(q(project)) && !controller.projectDirty() && fs::exists(project));
  REQUIRE(8, controller.activeProjectPath() == q(project) && controller.errorMessage().isEmpty());
  // No temporary files are left next to the project.
  int stray = 0;
  for (const auto& entry : fs::directory_iterator(root)) {
    const auto name = entry.path().filename().string();
    if (name != "media" && name != "not-a-folder.txt" && name != "one.pulsar") ++stray;
  }
  REQUIRE(9, stray == 0);

  // A later failed save leaves the saved file byte-identical and the project dirty.
  REQUIRE(10, import_and_wait(controller, root / "media/b.mp4", 2));
  controller.seekToEnd();
  controller.insertAssetAtPlayhead(controller.mediaAssets()->index(1, 0).data(MediaAssetListModel::MediaIdRole).toString());
  REQUIRE(11, clips(controller) == 2 && controller.projectDirty());
  const auto good_bytes = read_file(project);
  REQUIRE(12, !controller.saveProject(q(bad_parent / "again.pulsar")) && controller.projectDirty());
  REQUIRE(13, read_file(project) == good_bytes);
  // Save As: new file, the active path follows, dirty cleared only on success.
  const auto second = root / "two.pulsar";
  REQUIRE(14, controller.saveProject(q(second)) && !controller.projectDirty() && controller.activeProjectPath() == q(second));
  REQUIRE(15, read_file(project) == good_bytes);                          // original untouched

  // ---- Open safety: every failure keeps the current project ----------------
  const int assets_before = assets(controller);
  const int clips_before = clips(controller);
  const auto expect_open_fails = [&](const fs::path& file) {
    const bool dirty_before = controller.projectDirty();
    const bool ok = controller.openProject(q(file));
    return !ok && assets(controller) == assets_before && clips(controller) == clips_before &&
           controller.projectDirty() == dirty_before && controller.activeProjectPath() == q(second) &&
           is_human(controller.errorMessage());
  };
  const auto saved_two = read_file(second);
  write_file(root / "garbage.pulsar", "this is not a project\n\x01\x02\x03");
  REQUIRE(16, expect_open_fails(root / "garbage.pulsar"));
  write_file(root / "truncated.pulsar", saved_two.substr(0, saved_two.size() / 2));
  REQUIRE(17, expect_open_fails(root / "truncated.pulsar"));
  write_file(root / "empty.pulsar", "");
  REQUIRE(18, expect_open_fails(root / "empty.pulsar"));
  write_file(root / "future.pulsar", "PULSAR_PROJECT 999\n" + saved_two.substr(saved_two.find('\n') + 1));
  REQUIRE(19, expect_open_fails(root / "future.pulsar"));
  REQUIRE(20, expect_open_fails(root / "does-not-exist.pulsar"));
  REQUIRE(21, expect_open_fails(root));                                   // a directory

  // ---- Missing media: opens, shows offline, never crashes ------------------
  fs::remove(root / "media/b.mp4");
  REQUIRE(22, controller.openProject(q(second)));
  REQUIRE(23, controller.errorMessage().isEmpty() && assets(controller) == 2 && clips(controller) == 2);
  REQUIRE(24, asset_available(controller, 0) && !asset_available(controller, 1));
  REQUIRE(25, !clip_missing(controller, 0) && clip_missing(controller, 1));
  REQUIRE(26, !controller.projectDirty());                                // opening is not an edit
  // Preview over the offline clip does not crash and does not pretend to render it.
  controller.seekToEnd();
  controller.togglePlayback();
  spin(400);
  controller.togglePlayback();
  controller.seekToStart();
  spin(100);
  // Offline assets cannot be (re)inserted or separated; clear messages.
  const auto offline_id = controller.mediaAssets()->index(1, 0).data(MediaAssetListModel::MediaIdRole).toString();
  const int before_ops = clips(controller);
  controller.appendAssetToVideoTrack(offline_id);
  REQUIRE(27, clips(controller) == before_ops && controller.errorMessage().contains(QStringLiteral("no está disponible")));
  // Export fails clearly and a pre-existing good file is NOT replaced.
  const auto destination = root / "out.mp4";
  write_file(destination, "previous good export");
  controller.exportProject(q(destination));
  REQUIRE(28, wait_until([&] { return !controller.exportRunning(); }));
  REQUIRE(29, controller.exportStatus() == QStringLiteral("Error de exportación"));
  REQUIRE(30, controller.errorMessage().contains(QStringLiteral("multimedia")) && is_human(controller.errorMessage()));
  REQUIRE(31, read_file(destination) == "previous good export");
  int partial = 0;
  for (const auto& entry : fs::directory_iterator(root)) {
    const auto name = entry.path().filename().string();
    if (name.find("out.mp4") != std::string::npos && name != "out.mp4") ++partial;
  }
  REQUIRE(32, partial == 0);                                              // no stray temporary export

  // Restoring the file brings everything back (no hidden state).
  fs::copy_file(fixtures / "e6_video_b.mp4", root / "media/b.mp4");
  REQUIRE(33, controller.openProject(q(second)));
  REQUIRE(34, asset_available(controller, 1) && !clip_missing(controller, 1));

  // ---- Built-in sounds survive save -> reopen -> export (regression: the catalogue
  // locator was looked up relative to the working directory and came back "offline").
  REQUIRE(35, controller.soundPackAvailable());
  const auto sound_id = controller.soundLibrary().front().toMap().value(QStringLiteral("id")).toString();
  controller.seekToStart();
  controller.insertBuiltinSound(sound_id);
  REQUIRE(36, wait_until([&] { return controller.projectDirty() && clips(controller) >= 3; }));
  const auto with_sound = root / "with-sound.pulsar";
  REQUIRE(37, controller.saveProject(q(with_sound)));
  REQUIRE(38, controller.openProject(q(with_sound)));
  bool all_available = true;
  for (int row = 0; row < assets(controller); ++row) all_available = all_available && asset_available(controller, row);
  REQUIRE(39, all_available);
  const auto sound_export = root / "sound-export.mp4";
  controller.exportProject(q(sound_export));
  REQUIRE(40, wait_until([&] { return !controller.exportRunning(); }));
  REQUIRE(41, controller.exportStatus() == QStringLiteral("Exportación completada") && fs::exists(sound_export));

  // ---- Beta+ usability: one mute state, reachable from clip/inspector/menu/M ----------
  REQUIRE(42, controller.openProject(q(second)));
  const auto clip_key = [&](int row) { return controller.timelineClips()->index(row, 0).data(TimelineClipListModel::ClipKeyRole).toString(); };
  const auto clip_muted = [&](int row) { return controller.timelineClips()->index(row, 0).data(TimelineClipListModel::MutedRole).toBool(); };
  const auto clip_has_audio = [&](int row) { return controller.timelineClips()->index(row, 0).data(TimelineClipListModel::HasAudioRole).toBool(); };
  controller.selectTimelineClip(QString{});
  controller.toggleSelectedClipMute();                                   // nothing selected: no-op
  REQUIRE(43, !clip_muted(0) && !clip_muted(1) && !controller.selectedClipHasAudio());
  controller.selectTimelineClip(clip_key(1));
  REQUIRE(44, clip_has_audio(1) && controller.selectedClipHasAudio() && !controller.selectedClipMuted());
  controller.toggleSelectedClipMute();
  REQUIRE(45, controller.selectedClipMuted() && clip_muted(1) && !clip_muted(0));   // only that clip
  REQUIRE(46, controller.canUndo());
  controller.toggleSelectedClipMute();
  REQUIRE(47, !controller.selectedClipMuted() && !clip_muted(1));
  controller.toggleSelectedClipMute();                                   // mute again, then undo/redo it
  controller.undoEdit();
  REQUIRE(48, !clip_muted(1));
  controller.redoEdit();
  REQUIRE(49, clip_muted(1));
  controller.toggleSelectedClipMute();
  REQUIRE(50, !clip_muted(1));

  // ---- Distant Music automation reaches the timeline model ------------------------
  QVariantList automation{
      QVariantMap{{QStringLiteral("time"), 0.0}, {QStringLiteral("value"), -30.0},
                  {QStringLiteral("interpolation"), 1}},
      QVariantMap{{QStringLiteral("time"), 2.0}, {QStringLiteral("value"), -6.0},
                  {QStringLiteral("interpolation"), 2}}};
  controller.setSelectedDistantMusicGainKeyframes(automation);
  REQUIRE(51, controller.errorMessage().isEmpty());
  const auto timeline_automation = controller.timelineClips()->index(1, 0).data(
      TimelineClipListModel::GainKeyframesRole).toList();
  REQUIRE(52, timeline_automation.size() == 2);
  REQUIRE(53, std::abs(timeline_automation.at(0).toMap().value(QStringLiteral("time")).toDouble()) < 0.001);
  REQUIRE(54, std::abs(timeline_automation.at(1).toMap().value(QStringLiteral("value")).toDouble() + 6.0) < 0.001);
  REQUIRE(55, controller.timelineClips()->index(1, 0).data(
      TimelineClipListModel::DurationSecondsRole).toDouble() > 0.0);

  // ---- Snap toggle: same engine, only enabled/disabled -------------------------------
  const double width = 1000.0;
  const double start_px = controller.timelineClips()->index(1, 0).data(TimelineClipListModel::StartProgressRole).toDouble() * width;
  const double end_px = controller.timelineClips()->index(1, 0).data(TimelineClipListModel::EndProgressRole).toDouble() * width;
  const double press = (start_px + end_px) / 2.0;
  REQUIRE(56, controller.snapEnabled());
  const auto snapped = controller.previewTimelineMove(clip_key(1), press, press + 3.0, width);
  REQUIRE(57, snapped.value("guide").toDouble() >= 0.0 && std::abs(snapped.value("offset").toDouble()) < 0.5);
  controller.setSnapEnabled(false);
  const auto free_move = controller.previewTimelineMove(clip_key(1), press, press + 3.0, width);
  REQUIRE(58, free_move.value("guide").toDouble() < 0.0 && std::abs(free_move.value("offset").toDouble() - 3.0) < 0.5);
  // Overlap protection is not part of snapping: dragging left over the neighbour still stops flush.
  const auto into_neighbour = controller.previewTimelineMove(clip_key(1), press, press - (end_px - start_px), width);
  REQUIRE(59, into_neighbour.value("offset").toDouble() >= -0.5);
  controller.setSnapEnabled(true);
  const auto again = controller.previewTimelineMove(clip_key(1), press, press + 3.0, width);
  REQUIRE(60, again.value("guide").toDouble() >= 0.0);

  // ---- Wheel zoom keeps the time under the cursor ------------------------------------
  controller.zoomTimelineAt(width * 0.75, width, 4.0);
  REQUIRE(61, controller.timelineZoom() > 3.9 && controller.timelineScroll() > 0.0);
  controller.fitTimeline();
  REQUIRE(62, controller.timelineZoom() == 1.0 && controller.timelineScroll() == 0.0);

  // ---- Editing power: selection order, Q/W, ripple delete (controller level) ---------
  REQUIRE(63, controller.openProject(q(second)) && clips(controller) == 2);
  const auto clip_start = [&](int row) { return controller.timelineClips()->index(row, 0).data(TimelineClipListModel::StartRole).toString(); };
  const auto clip_end = [&](int row) { return controller.timelineClips()->index(row, 0).data(TimelineClipListModel::EndRole).toString(); };
  controller.selectTimelineClip(QString{});
  controller.selectNextClip();
  REQUIRE(64, controller.selectedClipKey() == clip_key(0));              // no selection: first
  controller.selectNextClip();
  REQUIRE(65, controller.selectedClipKey() == clip_key(1));
  controller.selectNextClip();
  REQUIRE(66, controller.selectedClipKey() == clip_key(1));              // stays at the end
  controller.selectPreviousClip();
  REQUIRE(67, controller.selectedClipKey() == clip_key(0));
  REQUIRE(68, !controller.projectDirty());                               // selection is not an edit

  controller.selectTimelineClip(clip_key(1));
  const auto b_start_text = clip_start(1);
  const auto b_end_text = clip_end(1);
  controller.seekToStart();
  controller.trimSelectedStartToPlayhead();                              // playhead outside: silent no-op
  controller.trimSelectedEndToPlayhead();
  REQUIRE(69, clip_start(1) == b_start_text && clip_end(1) == b_end_text && controller.errorMessage().isEmpty() && !controller.projectDirty());
  controller.seekNormalized(0.7);
  controller.trimSelectedStartToPlayhead();                              // Q
  REQUIRE(70, clip_start(1) != b_start_text && clip_end(1) == b_end_text && controller.projectDirty());
  controller.seekNormalized(0.9);
  controller.trimSelectedEndToPlayhead();                                // W
  REQUIRE(71, clip_end(1) != b_end_text);
  controller.undoEdit();
  controller.undoEdit();
  REQUIRE(72, clip_start(1) == b_start_text && clip_end(1) == b_end_text);
  controller.redoEdit();
  controller.redoEdit();
  REQUIRE(73, clip_start(1) != b_start_text && clip_end(1) != b_end_text);
  controller.undoEdit();
  controller.undoEdit();

  controller.selectTimelineClip(clip_key(0));
  REQUIRE(74, controller.canRippleDelete());
  const auto a_start_text = clip_start(0);
  controller.rippleDeleteSelectedClip();
  REQUIRE(75, clips(controller) == 1 && clip_start(0) == a_start_text && controller.selectedClipKey().isEmpty());
  controller.undoEdit();
  REQUIRE(76, clips(controller) == 2 && clip_start(1) == b_start_text);  // one undo restores the gap-closing move too
  controller.redoEdit();
  REQUIRE(77, clips(controller) == 1 && clip_start(0) == a_start_text);
  controller.undoEdit();
  controller.rippleDeleteSelectedClip();                                 // nothing selected after undo? either way: no crash

  // ---- Thumbnails: decoration only, bounded, never an error ----------------------------
  {
    ThumbnailCache cache{3};
    const auto image = [] { QImage i{4, 4, QImage::Format_RGBA8888}; i.fill(Qt::red); return i; };
    for (const char* key : {"k1", "k2", "k3"}) cache.put(key, image());
    (void)cache.get("k1");                                               // k2 is now the oldest
    cache.put("k4", image());
    cache.put("k5", image());
    REQUIRE(78, cache.size() == 3 && cache.contains("k1") && cache.contains("k5") && !cache.contains("k2") && !cache.contains("k3"));
    REQUIRE(79, cache.bytes() == 3u * 4u * 4u * 4u && cache.get("missing").isNull());
  }
  const auto thumb = [&](int row) { return controller.timelineClips()->index(row, 0).data(TimelineClipListModel::ThumbSourceRole).toString(); };
  REQUIRE(80, controller.openProject(q(second)) && clips(controller) == 2);
  REQUIRE(81, wait_until([&] { return !thumb(0).isEmpty() && !thumb(1).isEmpty(); }));
  REQUIRE(82, controller.thumbnailCache()->size() >= 2 && controller.thumbnailCache()->size() <= 192);
  {
    ThumbnailImageProvider provider{controller.thumbnailCache()};
    QSize size;
    const auto image = provider.requestImage(thumb(0), &size, {});
    REQUIRE(83, !image.isNull() && image.width() <= ThumbnailCache::kWidth && image.height() <= ThumbnailCache::kHeight && size == image.size());
  }
  // Offline media: no picture, no error, clip still listed as offline.
  fs::remove(root / "media/b.mp4");
  REQUIRE(84, controller.openProject(q(second)));
  REQUIRE(85, wait_until([&] { return !thumb(0).isEmpty(); }));
  spin(300);
  REQUIRE(86, thumb(1).isEmpty() && clip_missing(controller, 1) && !controller.errorMessage().contains(QStringLiteral("miniatura")));
  fs::copy_file(fixtures / "e6_video_b.mp4", root / "media/b.mp4");
  REQUIRE(87, controller.openProject(q(second)) && wait_until([&] { return !thumb(1).isEmpty(); }));
  controller.selectTimelineClip(clip_key(1));
  controller.setSelectedDistantMusicGainKeyframes(QVariantList{
      QVariantMap{{QStringLiteral("time"), 9999.0}, {QStringLiteral("value"), -6.0},
                  {QStringLiteral("interpolation"), 1}}});
  REQUIRE(88, controller.errorMessage().contains(QStringLiteral("Keyframe inválido")));
  REQUIRE(89, controller.selectedDistantMusicKeyframeCount() == 0);

  fs::remove_all(root);
  return 0;
}
