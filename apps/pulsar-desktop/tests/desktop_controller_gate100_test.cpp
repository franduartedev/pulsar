// 100-clip creator gate: a realistic big project (100 video clips, 3 audio tracks, titles,
// thumbnails) driven through the controller. Prints timings; asserts generous budgets.
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
double ms_since(Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }

bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 120'000) {
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
long rss_kb() {
  std::ifstream status{"/proc/self/status"};
  for (std::string line; std::getline(status, line);) if (line.rfind("VmRSS:", 0) == 0) return std::stol(line.substr(6));
  return -1;
}
void write_wav(const fs::path& path, int seconds) {
  const std::uint32_t rate = 44100, samples = rate * seconds, bytes = samples * 2;
  std::ofstream out{path, std::ios::binary};
  const auto u32 = [&](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
  const auto u16 = [&](std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
  out.write("RIFF", 4); u32(36 + bytes); out.write("WAVEfmt ", 8); u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
  out.write("data", 4); u32(bytes);
  std::vector<std::int16_t> tone(samples);
  for (std::uint32_t i = 0; i < samples; ++i) tone[i] = static_cast<std::int16_t>(8000 * ((i / 50) % 2 ? 1 : -1));
  out.write(reinterpret_cast<const char*>(tone.data()), static_cast<std::streamsize>(bytes));
}
int clips(DesktopController& c) { return c.timelineClips()->rowCount(); }
QString clip_key(DesktopController& c, int row) { return c.timelineClips()->index(row, 0).data(TimelineClipListModel::ClipKeyRole).toString(); }
QString asset_id(DesktopController& c, int row) { return c.mediaAssets()->index(row, 0).data(MediaAssetListModel::MediaIdRole).toString(); }
int video_clips(DesktopController& c) {
  int n = 0;
  for (int i = 0; i < clips(c); ++i) if (!c.timelineClips()->index(i, 0).data(TimelineClipListModel::AudioTrackRole).toBool()) ++n;
  return n;
}
int thumbs_ready(DesktopController& c) {
  int n = 0;
  for (int i = 0; i < clips(c); ++i) if (!c.timelineClips()->index(i, 0).data(TimelineClipListModel::ThumbSourceRole).toString().isEmpty()) ++n;
  return n;
}
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { std::fprintf(stderr, "GATE100 step %d failed: %s\n", step, #condition); return step; } } while (false)
#define REPORT(name, value) std::fprintf(stderr, "GATE100 %-34s %10.2f\n", name, static_cast<double>(value))

int main(int argc, char** argv) {
  QGuiApplication application{argc, argv};
  PreviewImageProvider provider;
  const auto fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  const auto root = fs::temp_directory_path() / "pulsar-gate100";
  fs::remove_all(root);
  fs::create_directories(root);
  // PULSAR_GATE_CLIPS / PULSAR_GATE_ASSETS scale the project (defaults: the 100-clip creator target).
  const int clip_target = std::getenv("PULSAR_GATE_CLIPS") ? std::max(10, std::atoi(std::getenv("PULSAR_GATE_CLIPS"))) : 100;
  const int asset_target = std::getenv("PULSAR_GATE_ASSETS") ? std::max(3, std::atoi(std::getenv("PULSAR_GATE_ASSETS"))) : 30;
  const fs::path originals[] = {fixtures / "e6_video_a.mp4", fixtures / "e6_video_b.mp4", fixtures / "e6_video_rational.mp4"};
  std::vector<fs::path> videos;
  for (int i = 0; i < asset_target; ++i) {           // distinct files = distinct library assets
    const auto copy = root / ("v" + std::to_string(i) + ".mp4");
    fs::copy_file(originals[i % 3], copy);
    videos.push_back(copy);
  }
  write_wav(root / "music.wav", 60);
  const auto project = root / "big.pulsar";
  long rss_start = rss_kb();

  {
    DesktopController controller{provider, {}};
    auto t = Clock::now();
    QStringList files;
    for (const auto& v : videos) files << q(v);
    files << q(root / "music.wav");   // the 100-ish clips are built below
    controller.openPaths(files);
    REQUIRE(1, wait_until([&] { return controller.mediaAssets()->rowCount() == asset_target + 1 && !controller.busy(); }));
    REPORT("import 4 files (ms)", ms_since(t));

    // 100 video clips: appended one by one (each call republishes the whole read model).
    std::vector<double> per_call;
    for (int i = 0; i < clip_target; ++i) {
      if (i == 0) continue;                       // the first video is already on the timeline
      const auto start = Clock::now();
      controller.appendAssetToVideoTrack(asset_id(controller, i % asset_target));
      per_call.push_back(ms_since(start));
    }
    REQUIRE(2, video_clips(controller) == clip_target);
    double first_ten = 0, last_ten = 0;
    for (int i = 0; i < 10; ++i) { first_ten += per_call[static_cast<std::size_t>(i)]; last_ten += per_call[per_call.size() - 1 - static_cast<std::size_t>(i)]; }
    REPORT("append call, first 10 avg (ms)", first_ten / 10);
    REPORT("append call, last 10 avg (ms)", last_ten / 10);

    // 3 audio tracks x 10 clips (music 60 s dropped at spaced positions, lane by lane).
    const auto music = asset_id(controller, asset_target);
    controller.seekToStart();
    controller.openPath(q(root / "music.wav"));        // creates AUDIO 1 (overlapping the first)
    controller.openPath(q(root / "music.wav"));
    REQUIRE(3, wait_until([&] { return controller.timelineTracks().size() >= 3 && !controller.busy(); }));
    t = Clock::now();
    int dropped = 0;
    const double width = 1000.0;
    for (int lane = 1; lane < controller.timelineTracks().size(); ++lane) {
      for (int k = 0; k < 10; ++k) {
        const double pixel = 20.0 + k * 95.0;
        dropped += controller.dropAsset(music, lane, pixel, width) ? 1 : 0;
      }
    }
    REPORT("audio drops accepted", dropped);
    REPORT("audio drops (ms)", ms_since(t));

    // Titles.
    t = Clock::now();
    int titles = 0;
    for (int i = 0; i < 12; ++i) {
      controller.seekNormalized((i + 0.5) / 12.5);
      controller.addTitle();
      titles = controller.timelineTitles()->rowCount();
    }
    REPORT("titles added", titles);
    REPORT("12 titles (ms)", ms_since(t));
    REQUIRE(4, titles >= 8);

    // Interaction costs on the big project.
    t = Clock::now();
    for (int i = 0; i < clip_target; ++i) controller.selectTimelineClip(clip_key(controller, i));
    const double select_avg = ms_since(t) / clip_target;
    REPORT("select clip avg (ms)", select_avg);
    t = Clock::now();
    for (int i = 0; i < 60; ++i) controller.zoomTimelineAt(500.0, 1000.0, i % 2 ? 0.8 : 1.25);
    const double zoom_avg = ms_since(t) / 60;
    REPORT("wheel zoom step avg (ms)", zoom_avg);
    controller.fitTimeline();
    t = Clock::now();
    controller.selectTimelineClip(clip_key(controller, clip_target / 2));
    controller.seekNormalized(0.505);
    controller.trimSelectedStartToPlayhead();
    const double trim_ms = ms_since(t);
    REPORT("seek + Q (ms)", trim_ms);
    t = Clock::now();
    controller.selectTimelineClip(clip_key(controller, 10));
    controller.rippleDeleteSelectedClip();
    const double ripple_ms = ms_since(t);
    REPORT("ripple delete (ms)", ripple_ms);
    t = Clock::now();
    controller.undoEdit();
    const double undo_ms = ms_since(t);
    t = Clock::now();
    controller.redoEdit();
    const double redo_ms = ms_since(t);
    REPORT("undo (ms)", undo_ms);
    REPORT("redo (ms)", redo_ms);
    controller.undoEdit();
    controller.undoEdit();

    t = Clock::now();
    REQUIRE(5, controller.saveProject(q(project)));
    const double save_ms = ms_since(t);
    REPORT("save (ms)", save_ms);
    REPORT("project file (KB)", fs::file_size(project) / 1024.0);
    t = Clock::now();
    REQUIRE(6, wait_until([&] { return thumbs_ready(controller) == video_clips(controller); }));
    REPORT("all thumbnails ready (ms)", ms_since(t));
    REPORT("RSS with project (MB)", rss_kb() / 1024.0);
    REQUIRE(7, select_avg < 150 && trim_ms < 600 && ripple_ms < 1500 && save_ms < 1500 && zoom_avg < 150);
  }

  {  // Reopen from disk in a fresh controller, then shut down.
    DesktopController controller{provider, {}};
    auto t = Clock::now();
    REQUIRE(8, controller.openProject(q(project)) && video_clips(controller) == clip_target);
    const double open_ms = ms_since(t);
    REPORT("project open (ms)", open_ms);
    t = Clock::now();
    REQUIRE(9, wait_until([&] { return thumbs_ready(controller) == video_clips(controller); }));
    REPORT("reopen: all thumbnails (ms)", ms_since(t));
    REPORT("RSS after reopen (MB)", rss_kb() / 1024.0);
    REQUIRE(10, open_ms < 3000 && rss_kb() / 1024.0 < 700);
    t = Clock::now();
    // destroyed at scope end
    REPORT("RSS growth vs start (MB)", (rss_kb() - rss_start) / 1024.0);
    (void)t;
  }
  const auto shutdown_start = Clock::now();
  REPORT("(destroyed) shutdown total (ms)", ms_since(shutdown_start));
  fs::remove_all(root);
  return 0;
}
