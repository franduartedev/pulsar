#include <pulsar/application/export/offline_video_renderer.hpp>
#include <pulsar/application/playback/frame_coverage.hpp>
#include <pulsar/application/playback/presentation_scheduler.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <cassert>
#include <memory>

using namespace pulsar;
namespace ex = application::exporting;
using application::StreamTimestamp;
domain::MediaTime t(int n, int d = 1) { return {n, {1, d}}; }

ex::OfflineVideoSourceFrame frame(domain::MediaTime pts, std::optional<domain::MediaTime> duration,
                                 bool green) {
  auto pixels = std::make_shared<ex::OfflineVideoPixelBuffer>(application::composition::black_surface(2, 2));
  for (std::size_t i = 0; i < pixels->pixels.size(); i += 4) pixels->pixels[i + (green ? 1 : 0)] = 255;
  return {pts, duration, std::move(pixels)};
}

struct Source final : ex::OfflineVideoSource {
  std::optional<domain::MediaTime> duration;
  bool lower;
  Source(std::optional<domain::MediaTime> value, bool is_lower) : duration{value}, lower{is_lower} {}
  ex::OfflineVideoSeekResult seek_to_presentation_timestamp(domain::MediaTime) override {
    if (lower) return {frame(t(0), t(3), true), std::nullopt, 1};
    return {frame(t(0), duration, false), frame(t(2), t(1), true), 2};
  }
  std::optional<ex::OfflineVideoSourceFrame> decode_next() override { return std::nullopt; }
};
struct Factory final : ex::OfflineVideoSourceFactory {
  std::optional<domain::MediaTime> duration;
  std::unique_ptr<ex::OfflineVideoSource> open(const std::filesystem::path& path, int) override {
    return std::make_unique<Source>(duration, path == "lower");
  }
};

int main() {
  for (const auto duration : {std::optional{t(2)}, std::optional{t(1)}, std::optional{t(3)},
                              std::optional<domain::MediaTime>{}, std::optional{t(0)}, std::optional{t(-1)}}) {
    for (const bool with_lower : {false, true}) {
      auto snapshot = application::ProjectDocument{}.snapshot();
      snapshot.assets.emplace_back(domain::MediaId{1}, "upper", 0, t(3),
          application::VideoAssetMetadata{"test", 2, 2, {1, 4}, domain::FrameRate{4, 1}, std::nullopt});
      snapshot.timeline.insert_clip(snapshot.video_track_id,
          domain::Clip{domain::ClipId{1}, domain::MediaId{1}, {t(0), t(3)}, t(0)});
      if (with_lower) {
        snapshot.assets.emplace_back(domain::MediaId{2}, "lower", 0, t(3), snapshot.assets.front().metadata());
        snapshot.timeline.add_track(domain::Track{domain::TrackId{2}, domain::TrackKind::Video});
        snapshot.timeline.insert_clip(domain::TrackId{2},
            domain::Clip{domain::ClipId{2}, domain::MediaId{2}, {t(0), t(3)}, t(0)});
        snapshot.visual_track_ids.insert(snapshot.visual_track_ids.begin(), domain::TrackId{2});
      }
      const auto plan = ex::ExportPlan::build(snapshot);
      Factory factory;
      factory.duration = duration;
      ex::OfflineVideoRenderer renderer{plan, factory};
      application::PresentationScheduler scheduler{application::SingleStreamPlaybackClockMap{StreamTimestamp{t(0)}}};
      scheduler.ingest(application::VideoSample{1, StreamTimestamp{t(0)}, duration});
      scheduler.ingest(application::VideoSample{2, StreamTimestamp{t(2)}, t(1)});
      for (int i = 0; i < 12; ++i) {
        const auto position = t(i, 4);
        const auto preview = scheduler.evaluate(application::PlaybackPosition{position});
        const bool gap = duration == t(1) && position >= t(1) && position < t(2);
        assert(std::holds_alternative<application::PresentationGap>(preview) == gap);
        if (!gap) {
          const auto sample = std::holds_alternative<application::PresentSample>(preview)
              ? std::get<application::PresentSample>(preview).sample
              : std::get<application::HoldPresentation>(preview).sample;
          assert(sample.ordinal == (position < t(2) ? 1 : 2));
        }
        const auto exported = renderer.render_frame(domain::FrameNumber{i});
        assert(exported.pixels[0] == (!gap && position < t(2) ? 255 : 0));
        assert(exported.pixels[1] == (position >= t(2) || (gap && with_lower) ? 255 : 0));
        assert(exported.pixels[2] == 0 && exported.pixels[3] == 255);
      }
    }
  }
  // Unknown terminal duration holds the last image, bounded by the editorial clip.
  const auto unknown = application::frame_coverage(t(100), t(0), std::nullopt, std::nullopt, true);
  assert(unknown.kind == application::FrameCoverageKind::Covered && unknown.terminal_end_unknown);
  const auto waiting = application::frame_coverage(t(1), t(0), std::nullopt, std::nullopt, false);
  assert(waiting.kind == application::FrameCoverageKind::NeedMore);
  const auto exact_end = application::frame_coverage(t(1), t(0), t(1), t(2), false);
  assert(exact_end.kind == application::FrameCoverageKind::Gap);
  const auto before_end = application::frame_coverage(t(29999, 30000), t(0), t(1), t(2), false);
  assert(before_end.kind == application::FrameCoverageKind::Covered);
}
