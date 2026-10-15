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

app::ProjectSnapshot project(int layers, const std::filesystem::path& locator) {
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
    snapshot.timeline.insert_clip(track, d::Clip{d::ClipId{media.value()}, media,
        {d::MediaTime::zero(), duration}, d::MediaTime::zero()});
  }
  return snapshot;
}

void run(int layers, const std::filesystem::path& locator, bool real_media) {
  const auto snapshot = project(layers, locator);
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
  std::cout << (real_media ? "FFmpeg " : "CPU synthetic ") << layers << " layers, " << width << 'x' << height << ", " << frames
            << " frames: " << elapsed << " s (" << frames / elapsed << " fps); peak decoders "
            << metrics.peak_active_decoders << ", peak retained source frames "
            << metrics.peak_live_source_frames << '\n';
}
} // namespace

int main(int argc, char** argv) {
  const bool real_media = argc == 2;
  if (argc > 2) throw std::invalid_argument{"pass one 1080p 30 fps 2-second video path"};
  const std::filesystem::path locator = real_media ? argv[1] : "synthetic";
  run(1, locator, real_media);
  run(3, locator, real_media);
}
