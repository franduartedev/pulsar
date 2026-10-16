#include <pulsar/application/export/offline_video_renderer.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_video_source.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
namespace app = pulsar::application;
namespace ex = pulsar::application::exporting;
namespace d = pulsar::domain;

constexpr int width = 1920;
constexpr int height = 1080;
constexpr int frames = 60;
const d::FrameRate rate{30, 1};
const auto duration = d::MediaTime{2, {1, 1}};

class Source final : public ex::OfflineVideoSource {
public:
  explicit Source(std::shared_ptr<const ex::OfflineVideoPixelBuffer> image) : image_{std::move(image)} {}
  ex::OfflineVideoSeekResult seek_to_presentation_timestamp(d::MediaTime) override {
    return {ex::OfflineVideoSourceFrame{d::MediaTime::zero(), duration, image_}, {}, 1};
  }
  std::optional<ex::OfflineVideoSourceFrame> decode_next() override { return std::nullopt; }
private:
  std::shared_ptr<const ex::OfflineVideoPixelBuffer> image_;
};

class Factory final : public ex::OfflineVideoSourceFactory {
public:
  Factory() {
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4, 255);
    image = std::make_shared<const ex::OfflineVideoPixelBuffer>(
        ex::OfflineVideoPixelBuffer{width, height, width * 4, std::move(rgba)});
  }
  std::unique_ptr<ex::OfflineVideoSource> open(const std::filesystem::path&, int) override {
    return std::make_unique<Source>(image);
  }
  std::shared_ptr<const ex::OfflineVideoPixelBuffer> image;
};

app::ProjectSnapshot project(int layers, const std::filesystem::path& locator,
                            bool transformed = false) {
  app::ProjectDocument document;
  auto snapshot = document.snapshot();
  snapshot.canvas = app::ProjectCanvas{width, height, rate};
  for (int index = 0; index < layers; ++index) {
    const auto track = index == 0 ? snapshot.video_track_id : d::TrackId{static_cast<std::uint64_t>(index + 1)};
    const auto media = d::MediaId{static_cast<std::uint64_t>(index + 1)};
    if (index) {
      snapshot.timeline.add_track(d::Track{track, d::TrackKind::Video});
      snapshot.visual_track_ids.push_back(track);
    }
    snapshot.assets.emplace_back(media, locator, 0, duration,
        app::VideoAssetMetadata{"raw", width, height, {1, 30}, rate, std::nullopt});
    d::VisualTransform transform;
    if (transformed && index > 0) {
      transform = d::VisualTransform{0.2 * index, -0.1 * index, 0.5, 15.0 * index, 0.8};
    } else if (transformed && layers == 1) {
      transform = d::VisualTransform{0.1, 0.05, 0.75, 30.0, 0.9};
    }
    snapshot.timeline.insert_clip(track, d::Clip{d::ClipId{media.value()}, media,
        {d::MediaTime::zero(), duration}, d::MediaTime::zero(), 0.0, false,
        d::MediaTime::zero(), d::MediaTime::zero(), transform});
  }
  return snapshot;
}

app::ProjectSnapshot dissolve_project(const std::filesystem::path& locator, bool showcase) {
  app::ProjectDocument document;
  auto snapshot = document.snapshot();
  snapshot.canvas = app::ProjectCanvas{width, height, rate};
  snapshot.assets.emplace_back(d::MediaId{1}, locator, 0, duration,
      app::VideoAssetMetadata{"raw", width, height, {1, 30}, rate, std::nullopt});
  snapshot.timeline.insert_clip(snapshot.video_track_id,
      d::Clip{d::ClipId{1}, d::MediaId{1}, {{0, {1, 1}}, {1, {1, 1}}}, {0, {1, 1}}});
  snapshot.timeline.insert_clip(snapshot.video_track_id,
      d::Clip{d::ClipId{2}, d::MediaId{1}, {{1, {1, 1}}, {2, {1, 1}}}, {1, {1, 1}}});
  snapshot.transitions.push_back({snapshot.video_track_id, d::ClipId{1}, d::ClipId{2},
                                  d::TransitionType::CrossDissolve, {1, {1, 1}}});
  if (showcase) {
    for (int index = 0; index < 2; ++index) {
      const d::TrackId track{static_cast<std::uint64_t>(index + 2)};
      snapshot.timeline.add_track(d::Track{track, d::TrackKind::Video});
      snapshot.visual_track_ids.push_back(track);
      snapshot.timeline.insert_clip(track, d::Clip{d::ClipId{static_cast<std::uint64_t>(index + 3)},
          d::MediaId{1}, {{0, {1, 1}}, {2, {1, 1}}}, {0, {1, 1}}, 0.0, false,
          d::MediaTime::zero(), d::MediaTime::zero(),
          d::VisualTransform{index == 0 ? 0.28 : -0.3, index == 0 ? 0.25 : -0.3,
                             index == 0 ? 0.38 : 0.22, 0.0, index == 0 ? 1.0 : 0.85}});
    }
    snapshot.titles.emplace_back(d::TitleId{1}, "PULSAR", d::TimeRange{{1, {1, 2}}, {3, {1, 2}}});
  }
  snapshot.allocator_state.next_media_id = 2;
  snapshot.allocator_state.next_clip_id = showcase ? 5 : 3;
  snapshot.allocator_state.next_track_id = showcase ? 4 : 2;
  snapshot.allocator_state.next_title_id = showcase ? 2 : 1;
  return snapshot;
}

void run(int layers, const std::filesystem::path& locator, bool real_media, bool transformed = false) {
  const auto snapshot = project(layers, locator, transformed);
  const auto plan = ex::ExportPlan::build(snapshot);
  Factory factory;
  pulsar::media::ffmpeg::FFmpegOfflineVideoSourceFactory ffmpeg;
  ex::OfflineVideoRenderer renderer{plan, real_media ? static_cast<ex::OfflineVideoSourceFactory&>(ffmpeg)
                                                  : static_cast<ex::OfflineVideoSourceFactory&>(factory)};
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < frames; ++i) {
    const auto frame = renderer.render_frame(d::FrameNumber{i});
    if (frame.width != width || frame.height != height || frame.pixels[3] != 255)
      throw std::runtime_error{"invalid benchmark output"};
  }
  const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  const auto& metrics = renderer.metrics();
  if (metrics.peak_active_decoders > static_cast<std::uint64_t>(layers) ||
      metrics.peak_live_source_frames > static_cast<std::uint64_t>(2 * layers) ||
      metrics.output_frames != frames) throw std::runtime_error{"unbounded renderer state"};
  std::cout << (real_media ? "FFmpeg " : "CPU synthetic ") << (transformed ? "transformed " : "default ")
            << layers << " layers, " << width << 'x' << height << ", " << frames
            << " frames: " << elapsed << " s (" << frames / elapsed << " fps); peak decoders "
            << metrics.peak_active_decoders << ", peak retained source frames "
            << metrics.peak_live_source_frames << '\n';
}

void run_scenario(std::string_view label, app::ProjectSnapshot snapshot,
                  bool real_media) {
  const auto plan = ex::ExportPlan::build(snapshot);
  Factory factory;
  pulsar::media::ffmpeg::FFmpegOfflineVideoSourceFactory ffmpeg;
  ex::OfflineVideoRenderer renderer{plan, real_media ? static_cast<ex::OfflineVideoSourceFactory&>(ffmpeg)
                                                      : static_cast<ex::OfflineVideoSourceFactory&>(factory)};
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < frames; ++i) (void)renderer.render_frame(d::FrameNumber{i});
  const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  const auto& metrics = renderer.metrics();
  std::cout << (real_media ? "FFmpeg " : "CPU synthetic ") << label << ", " << width << 'x'
            << height << ", " << frames << " frames: " << elapsed << " s (" << frames / elapsed
            << " fps, " << elapsed * 1000.0 / frames << " ms/frame); peak decoders "
            << metrics.peak_active_decoders << ", peak retained source frames "
            << metrics.peak_live_source_frames << ", composite ms "
            << metrics.composite_nanoseconds / 1'000'000.0 << '\n';
}
} // namespace

int main(int argc, char** argv) {
  const bool real_media = argc == 2;
  if (argc > 2) throw std::invalid_argument{"pass one 1080p 30 fps 2-second video path"};
  const std::filesystem::path locator = real_media ? argv[1] : "synthetic";
  run(1, locator, real_media, false);
  run(1, locator, real_media, true);
  run(3, locator, real_media, true);
  run_scenario("2 clips + cross dissolve", dissolve_project(locator, false), real_media);
  run_scenario("showcase composition", dissolve_project(locator, true), real_media);
}
