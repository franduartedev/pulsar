#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/export/offline_video_renderer.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/application/audio/audio_contributor_resolver.hpp>

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace pulsar;
namespace exporting = pulsar::application::exporting;

class Probe final : public application::MediaProbe {
public:
  [[nodiscard]] application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    application::MediaProbeResult result;
    result.path = path;
    result.video_streams.push_back(application::VideoStreamInfo{
        0, "rawvideo", 4, 2, domain::Rational{1, 1}, domain::MediaTime{10, {1, 1}},
        domain::FrameRate{1, 1}});
    return result;
  }
};

struct FakeFrame {
  std::optional<domain::MediaTime> timestamp;
  int width;
  int height;
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;
  std::optional<domain::MediaTime> duration{};
};

std::shared_ptr<const application::exporting::OfflineVideoPixelBuffer> image(const FakeFrame& frame) {
  const int stride = frame.width * 4;
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride * frame.height));
  for (int y = 0; y < frame.height; ++y) {
    for (int x = 0; x < frame.width; ++x) {
      const auto offset = static_cast<std::size_t>(y * stride + x * 4);
      pixels[offset] = frame.red;
      pixels[offset + 1] = frame.green;
      pixels[offset + 2] = frame.blue;
      pixels[offset + 3] = 255;
    }
  }
  return std::make_shared<const application::exporting::OfflineVideoPixelBuffer>(
      application::exporting::OfflineVideoPixelBuffer{frame.width, frame.height, stride,
                                                       std::move(pixels)});
}

application::exporting::OfflineVideoSourceFrame source_frame(const FakeFrame& frame) {
  return application::exporting::OfflineVideoSourceFrame{frame.timestamp, frame.duration, image(frame)};
}

class FakeSource final : public exporting::OfflineVideoSource {
public:
  explicit FakeSource(std::vector<FakeFrame> frames) : frames_{std::move(frames)} {}

  [[nodiscard]] exporting::OfflineVideoSeekResult seek_to_presentation_timestamp(
      domain::MediaTime target) override {
    cursor_ = 0;
    std::optional<std::size_t> candidate;
    std::optional<std::size_t> lookahead;
    for (std::size_t index = 0; index < frames_.size(); ++index) {
      if (!frames_[index].timestamp.has_value()) {
        if (!candidate.has_value() && !lookahead.has_value()) candidate = index;
        continue;
      }
      if (*frames_[index].timestamp <= target) {
        candidate = index;
      } else {
        lookahead = index;
        break;
      }
    }
    if (lookahead.has_value()) cursor_ = *lookahead + 1;
    else if (candidate.has_value()) cursor_ = *candidate + 1;
    auto make_frame = [this](std::size_t index) {
      auto result = source_frame(frames_[index]);
      if (index + 1 < frames_.size() && frames_[index + 1].timestamp.has_value() &&
          result.presentation_timestamp.has_value()) {
        result.presentation_duration = *frames_[index + 1].timestamp -
                                       *result.presentation_timestamp;
      } else if (index > 0 && frames_[index - 1].timestamp.has_value() &&
                 result.presentation_timestamp.has_value()) {
        result.presentation_duration = *result.presentation_timestamp -
                                       *frames_[index - 1].timestamp;
      }
      return result;
    };
    return exporting::OfflineVideoSeekResult{
        candidate.has_value() ? std::optional{make_frame(*candidate)} : std::nullopt,
        lookahead.has_value() ? std::optional{make_frame(*lookahead)} : std::nullopt,
        (candidate.has_value() ? 1U : 0U) + (lookahead.has_value() ? 1U : 0U)};
  }

  [[nodiscard]] std::optional<exporting::OfflineVideoSourceFrame> decode_next() override {
    if (cursor_ >= frames_.size()) return std::nullopt;
    const auto index = cursor_++;
    auto result = source_frame(frames_[index]);
    if (index + 1 < frames_.size() && frames_[index + 1].timestamp.has_value() &&
        result.presentation_timestamp.has_value()) {
      result.presentation_duration = *frames_[index + 1].timestamp -
                                     *result.presentation_timestamp;
    } else if (index > 0 && frames_[index - 1].timestamp.has_value() &&
               result.presentation_timestamp.has_value()) {
      result.presentation_duration = *result.presentation_timestamp -
                                     *frames_[index - 1].timestamp;
    }
    return result;
  }

private:
  std::vector<FakeFrame> frames_;
  std::size_t cursor_{0};
};

class FakeFactory final : public exporting::OfflineVideoSourceFactory {
public:
  std::map<std::string, std::vector<FakeFrame>> sources;
  std::uint64_t opens{0};

  [[nodiscard]] std::unique_ptr<exporting::OfflineVideoSource> open(
      const std::filesystem::path& locator, int) override {
    ++opens;
    return std::make_unique<FakeSource>(sources.at(locator.generic_string()));
  }
};

application::ProjectSnapshot project_with_clips(
    const std::vector<std::pair<std::filesystem::path, domain::MediaTime>>& clips,
    domain::MediaTime source_duration = domain::MediaTime{10, {1, 1}}) {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  Probe probe;
  for (const auto& [path, timeline_start] : clips) {
    const auto media = processor.import_asset(path, probe);
    (void)processor.add_clip(media,
                             domain::TimeRange{domain::MediaTime::zero(), source_duration},
                             timeline_start);
  }
  return document.snapshot();
}

exporting::ExportPlan plan_for(const application::ProjectSnapshot& snapshot) {
  return exporting::ExportPlan::build(snapshot);
}

void assert_rgba(const exporting::OfflineVideoFrame& frame, int x, int y,
                 std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  const auto offset = static_cast<std::size_t>(y * frame.stride_bytes + x * 4);
  assert(frame.pixels[offset] == red);
  assert(frame.pixels[offset + 1] == green);
  assert(frame.pixels[offset + 2] == blue);
  assert(frame.pixels[offset + 3] == 255 || (red == 0 && green == 0 && blue == 0));
}

std::vector<FakeFrame> rgb_frames() {
  return {{domain::MediaTime{0, {1, 1}}, 2, 2, 255, 0, 0},
           {domain::MediaTime{1, {1, 1}}, 2, 2, 0, 255, 0},
           {domain::MediaTime{2, {1, 1}}, 2, 2, 0, 0, 255}};
}

void sequential_render_reuses_decoder_and_letterboxes() {
  const auto snapshot = project_with_clips({{"video.mp4", domain::MediaTime::zero()}},
                                           domain::MediaTime{3, {1, 1}});
  const auto plan = plan_for(snapshot);
  FakeFactory factory;
  factory.sources["video.mp4"] = rgb_frames();
  exporting::OfflineVideoRenderer renderer{plan, factory};

  const auto first = renderer.render_frame(domain::FrameNumber{0});
  const auto second = renderer.render_frame(domain::FrameNumber{1});
  const auto third = renderer.render_frame(domain::FrameNumber{2});
  assert(first.frame_index.value() == 0);
  assert((second.timeline_time == domain::MediaTime{1, {1, 1}}));
  assert(factory.opens == 1);
  assert(renderer.metrics().seeks == 1);
  assert(renderer.metrics().output_frames == 3);
  assert(renderer.metrics().decoded_source_frames == 3);
  assert(renderer.metrics().peak_live_source_frames <= 2);
  assert_rgba(first, 0, 0, 0, 0, 0);
  assert_rgba(first, 1, 0, 255, 0, 0);
  assert_rgba(second, 1, 0, 0, 255, 0);
  assert_rgba(third, 1, 0, 0, 0, 255);
}

void multilayer_export_order_gaps_state_and_audio() {
  auto snapshot = project_with_clips({{"video.mp4", domain::MediaTime::zero()}}, {3, {1, 1}});
  domain::Track upper{domain::TrackId{9}, domain::TrackKind::Video};
  upper.insert_clip(domain::Clip{domain::ClipId{20}, snapshot.assets.front().id(),
      domain::TimeRange{{1, {1, 1}}, {2, {1, 1}}}, domain::MediaTime::zero()});
  snapshot.timeline.add_track(std::move(upper));
  snapshot.visual_track_ids = {snapshot.video_track_id, domain::TrackId{9}};
  const auto original = snapshot.assets.front();
  snapshot.assets.front() = application::MediaAsset{
      original.id(), original.source_locator(), original.video_stream_index(),
      original.playable_duration(), original.metadata(), application::MediaAvailability::Available,
      std::nullopt, application::AudioAssetMetadata{1, 48000, 2, {1, 48000}, domain::MediaTime{10, {1, 1}}}};
  const auto audio = application::resolve_audio_contributors(snapshot, {{0, {1, 1}}, {1, {1, 1}}});
  assert(audio.contributors.size() == 1);
  assert(audio.contributors.front().track_id == snapshot.video_track_id);

  const auto plan = plan_for(snapshot);
  FakeFactory factory;
  factory.sources["video.mp4"] = rgb_frames();
  exporting::OfflineVideoRenderer renderer{plan, factory};
  assert_rgba(renderer.render_frame(domain::FrameNumber{0}), 1, 0, 0, 255, 0);
  assert_rgba(renderer.render_frame(domain::FrameNumber{1}), 1, 0, 0, 255, 0);
  assert_rgba(renderer.render_frame(domain::FrameNumber{2}), 1, 0, 0, 0, 255);
  assert(factory.opens == 2);
  assert(renderer.metrics().peak_active_decoders == 2);
  assert(renderer.metrics().peak_active_layers == 2);
  assert(renderer.metrics().peak_live_source_frames <= 4);
  assert(renderer.metrics().composite_nanoseconds > 0);

  snapshot.visual_track_ids = {domain::TrackId{9}, snapshot.video_track_id};
  const auto reversed_plan = plan_for(snapshot);
  exporting::OfflineVideoRenderer reversed{reversed_plan, factory};
  assert_rgba(reversed.render_frame(domain::FrameNumber{0}), 1, 0, 255, 0, 0);

  // Cancellation between layers cannot return a partially composed output frame.
  FakeFactory cancelled_factory;
  cancelled_factory.sources["video.mp4"] = rgb_frames();
  exporting::OfflineVideoRenderer cancelled{plan, cancelled_factory, {},
      [&] { return cancelled_factory.opens != 0; }};
  bool rejected = false;
  try { (void)cancelled.render_frame(domain::FrameNumber{0}); }
  catch (const exporting::OfflineVideoRenderError& error) {
    rejected = error.code() == exporting::OfflineVideoRenderErrorCode::Cancelled;
  }
  assert(rejected && cancelled.metrics().output_frames == 0 && cancelled_factory.opens == 1);
}

void gaps_are_black_and_clip_boundaries_are_half_open() {
  const auto snapshot = project_with_clips({{"video.mp4", {0, {1, 1}}},
                                            {"video.mp4", {2, {1, 1}}}},
                                           {1, {1, 1}});
  const auto plan = plan_for(snapshot);
  FakeFactory factory;
  factory.sources["video.mp4"] = rgb_frames();
  exporting::OfflineVideoRenderer renderer{plan, factory};
  const auto first = renderer.render_frame(domain::FrameNumber{0});
  const auto gap = renderer.render_frame(domain::FrameNumber{1});
  const auto second_clip = renderer.render_frame(domain::FrameNumber{2});
  assert_rgba(first, 1, 0, 255, 0, 0);
  assert_rgba(gap, 0, 0, 0, 0, 0);
  assert_rgba(second_clip, 1, 0, 255, 0, 0);
  assert(renderer.metrics().seeks == 2);
}

void vfr_selection_uses_timestamps_not_source_fps() {
  const auto snapshot = project_with_clips({{"vfr.mp4", domain::MediaTime::zero()}},
                                           {3, {1, 1}});
  const auto plan = plan_for(snapshot);
  FakeFactory factory;
  factory.sources["vfr.mp4"] = std::vector<FakeFrame>{
      FakeFrame{domain::MediaTime{0, {1, 1}}, 2, 2, 255, 0, 0},
      FakeFrame{domain::MediaTime{1, {1, 2}}, 2, 2, 0, 255, 0},
      FakeFrame{domain::MediaTime{7, {1, 4}}, 2, 2, 0, 0, 255},
  };
  exporting::OfflineVideoRenderer renderer{plan, factory};
  (void)renderer.render_frame(domain::FrameNumber{0});
  const auto at_one = renderer.render_frame(domain::FrameNumber{1});
  const auto at_two = renderer.render_frame(domain::FrameNumber{2});
  assert_rgba(at_one, 1, 0, 0, 255, 0);
  assert_rgba(at_two, 1, 0, 0, 0, 255);
}

void trim_source_offset_and_errors_are_explicit() {
  // Rebuild a source-offset project through the domain command API.
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  Probe probe;
  const auto media = processor.import_asset("offset.mp4", probe);
  (void)processor.add_clip(media, domain::TimeRange{{1, {1, 1}}, {3, {1, 1}}},
                           domain::MediaTime::zero());
  FakeFactory factory;
  factory.sources["offset.mp4"] = rgb_frames();
  const auto plan = plan_for(document.snapshot());
  exporting::OfflineVideoRenderer renderer{plan, factory};
  const auto frame = renderer.render_frame(domain::FrameNumber{0});
  assert_rgba(frame, 1, 0, 0, 255, 0);

  FakeFactory eof_factory;
  eof_factory.sources["offset.mp4"] = {
      FakeFrame{domain::MediaTime{1, {1, 1}}, 2, 2, 0, 255, 0, domain::MediaTime{1, {1, 1}}},
  };
  exporting::OfflineVideoRenderer eof_renderer{plan, eof_factory};
  (void)eof_renderer.render_frame(domain::FrameNumber{0});
  try {
    (void)eof_renderer.render_frame(domain::FrameNumber{1});
    assert(false);
  } catch (const exporting::OfflineVideoRenderError& error) {
    assert(error.code() == exporting::OfflineVideoRenderErrorCode::SourceEndedEarly);
  }
}

void source_without_pts_is_not_silently_accepted() {
  const auto snapshot = project_with_clips({{"missing-pts.mp4", domain::MediaTime::zero()}},
                                           {1, {1, 1}});
  FakeFactory factory;
  factory.sources["missing-pts.mp4"] = {{std::nullopt, 2, 2, 255, 0, 0}};
  const auto plan = plan_for(snapshot);
  exporting::OfflineVideoRenderer renderer{plan, factory};
  try {
    (void)renderer.render_frame(domain::FrameNumber{0});
    assert(false);
  } catch (const exporting::OfflineVideoRenderError& error) {
    assert(error.code() == exporting::OfflineVideoRenderErrorCode::TimestampMissing);
  }
}

} // namespace

int main() {
  multilayer_export_order_gaps_state_and_audio();
  sequential_render_reuses_decoder_and_letterboxes();
  gaps_are_black_and_clip_boundaries_are_half_open();
  vfr_selection_uses_timestamps_not_source_fps();
  trim_source_offset_and_errors_are_explicit();
  source_without_pts_is_not_silently_accepted();
}
