#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/export/offline_video_renderer.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_video_source.hpp>

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

namespace {

std::string shell_quote(const std::filesystem::path& path) {
  std::string quoted{"'"};
  for (const char character : path.string()) {
    if (character == '\'') quoted += "'\\''";
    else quoted += character;
  }
  quoted += "'";
  return quoted;
}

class OriginProbe final : public pulsar::application::MediaProbe {
public:
  explicit OriginProbe(pulsar::domain::MediaTime duration) : duration_{duration} {}

  [[nodiscard]] pulsar::application::MediaProbeResult probe(
      const std::filesystem::path& path) const override {
    pulsar::application::MediaProbeResult result;
    result.path = path;
    result.video_streams.push_back(pulsar::application::VideoStreamInfo{
        0, "h264", 320, 180, pulsar::domain::Rational{1, 1000},
        duration_, pulsar::domain::FrameRate{25, 1}});
    return result;
  }

private:
  pulsar::domain::MediaTime duration_;
};

void renders_real_fixture_sequentially() {
  pulsar::application::ProjectDocument document;
  pulsar::application::CommandProcessor processor{document};
  pulsar::media::ffmpeg::FFmpegMediaProbe probe;
  const std::filesystem::path fixture =
      std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/audio_decode_fixture.mp4";
  const auto media = processor.import_asset(fixture, probe);
  const auto asset = document.find_asset(media);
  assert(asset.has_value());
  (void)processor.add_clip(media,
                           pulsar::domain::TimeRange{pulsar::domain::MediaTime::zero(),
                                                     asset->playable_duration()},
                           pulsar::domain::MediaTime::zero());

  const auto plan = pulsar::application::exporting::ExportPlan::build(document.snapshot());
  pulsar::media::ffmpeg::FFmpegOfflineVideoSourceFactory factory;
  pulsar::application::exporting::OfflineVideoRenderer renderer{plan, factory};
  std::vector<std::uint8_t> first_pixels;
  for (std::int64_t index = 0; index < plan.frame_count(); ++index) {
    const auto frame = renderer.render_frame(pulsar::domain::FrameNumber{index});
    assert(frame.width == plan.output_width());
    assert(frame.height == plan.output_height());
    assert(frame.stride_bytes == frame.width * 4);
    assert(frame.pixels.size() == static_cast<std::size_t>(frame.stride_bytes * frame.height));
    if (index == 0) first_pixels = frame.pixels;
  }

  assert(renderer.metrics().output_frames == static_cast<std::uint64_t>(plan.frame_count()));
  assert(renderer.metrics().decoder_opens == 1);
  assert(renderer.metrics().seeks == 1);
  assert(renderer.metrics().decoded_source_frames >= renderer.metrics().output_frames);
  assert(renderer.metrics().peak_live_source_frames <= 2);
  assert(!first_pixels.empty());
}

void normalizes_non_zero_stream_origin() {
  const auto path = std::filesystem::temp_directory_path() /
                    ("pulsar-e2-offline-origin-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                     ".mkv");
  const auto command = shell_quote(PULSAR_TEST_FFMPEG_CLI) +
                       " -y -hide_banner -loglevel error -f lavfi "
                       "-i testsrc2=size=320x180:rate=25:duration=1 "
                       "-vf setpts=PTS+5/TB -c:v libx264 -pix_fmt yuv420p "
                       "-bf 0 -g 25 -f matroska " + shell_quote(path);
  if (std::system(command.c_str()) != 0) {
    throw std::runtime_error{"could not create non-zero stream-origin fixture"};
  }

  try {
    pulsar::application::ProjectDocument document;
    pulsar::application::CommandProcessor processor{document};
    OriginProbe probe{pulsar::domain::MediaTime{1, {1, 1}}};
    const auto media = processor.import_asset(path, probe);
    const auto asset = document.find_asset(media);
    assert(asset.has_value());
    (void)processor.add_clip(media,
                             pulsar::domain::TimeRange{pulsar::domain::MediaTime::zero(),
                                                       asset->playable_duration()},
                             pulsar::domain::MediaTime::zero());
    const auto plan = pulsar::application::exporting::ExportPlan::build(document.snapshot());
    pulsar::media::ffmpeg::FFmpegOfflineVideoSourceFactory factory;
    pulsar::application::exporting::OfflineVideoRenderer renderer{plan, factory};
    const auto frame = renderer.render_frame(pulsar::domain::FrameNumber{0});
    assert(!frame.pixels.empty());
    assert(renderer.metrics().seeks == 1);
  } catch (...) {
    std::filesystem::remove(path);
    throw;
  }
  std::filesystem::remove(path);
}

void multilayer_real_decode_matches_single_track() {
  using namespace pulsar;
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  media::ffmpeg::FFmpegMediaProbe probe;
  const auto fixture = std::filesystem::path{PULSAR_SOURCE_DIR} / "tests/fixtures/audio_decode_fixture.mp4";
  const auto id = processor.import_asset(fixture, probe);
  const auto duration = document.find_asset(id)->playable_duration();
  (void)processor.add_clip(id, {domain::MediaTime::zero(), duration}, domain::MediaTime::zero());
  auto snapshot = document.snapshot();
  const auto single_plan = application::exporting::ExportPlan::build(snapshot);
  for (const auto track_id : {9U, 3U}) {
    domain::Track track{domain::TrackId{track_id}, domain::TrackKind::Video};
    track.insert_clip(domain::Clip{domain::ClipId{track_id}, id,
        {domain::MediaTime::zero(), duration}, domain::MediaTime::zero()});
    snapshot.timeline.add_track(std::move(track));
    snapshot.visual_track_ids.push_back(domain::TrackId{track_id});
  }
  const auto multi_plan = application::exporting::ExportPlan::build(snapshot);
  media::ffmpeg::FFmpegOfflineVideoSourceFactory factory;
  application::exporting::OfflineVideoRenderer single{single_plan, factory};
  application::exporting::OfflineVideoRenderer multi{multi_plan, factory};
  for (std::int64_t i = 0; i < single_plan.frame_count(); ++i) {
    const auto expected = single.render_frame(domain::FrameNumber{i});
    const auto actual = multi.render_frame(domain::FrameNumber{i});
    assert(expected.timeline_time == actual.timeline_time && expected.pixels == actual.pixels);
  }
  const auto& metrics = multi.metrics();
  assert(metrics.peak_active_decoders == 3 && metrics.decoder_opens == 3);
  assert(metrics.peak_live_source_frames <= 6);
  std::cout << "multi-layer export: layers=" << metrics.peak_active_layers
            << " decoders=" << metrics.peak_active_decoders
            << " live_source_frames=" << metrics.peak_live_source_frames
            << " frames=" << metrics.output_frames
            << " composite_ns=" << metrics.composite_nanoseconds << '\n';
}

void renders_long_fixtures_with_bounded_state() {
  for (const auto seconds : {10, 30}) {
    const auto path = std::filesystem::temp_directory_path() /
                      ("pulsar-e2-offline-long-" + std::to_string(seconds) + "-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                       ".mkv");
    const auto command = shell_quote(PULSAR_TEST_FFMPEG_CLI) +
                         " -y -hide_banner -loglevel error -f lavfi "
                         "-i testsrc2=size=64x36:rate=25:duration=" + std::to_string(seconds) +
                         " -c:v mpeg4 -pix_fmt yuv420p -an -f matroska " + shell_quote(path);
    if (std::system(command.c_str()) != 0) {
      throw std::runtime_error{"could not create long offline-render fixture"};
    }

    try {
      pulsar::application::ProjectDocument document;
      pulsar::application::CommandProcessor processor{document};
      OriginProbe probe{pulsar::domain::MediaTime{seconds, {1, 1}}};
      const auto media = processor.import_asset(path, probe);
      (void)processor.add_clip(media,
                               pulsar::domain::TimeRange{
                                   pulsar::domain::MediaTime::zero(),
                                   pulsar::domain::MediaTime{seconds, {1, 1}}},
                               pulsar::domain::MediaTime::zero());
      const auto plan = pulsar::application::exporting::ExportPlan::build(document.snapshot());
      pulsar::media::ffmpeg::FFmpegOfflineVideoSourceFactory factory;
      pulsar::application::exporting::OfflineVideoRenderer renderer{plan, factory};
      for (std::int64_t index = 0; index < plan.frame_count(); ++index) {
        (void)renderer.render_frame(pulsar::domain::FrameNumber{index});
      }
      assert(plan.frame_count() == seconds * 25);
      assert(renderer.metrics().output_frames == static_cast<std::uint64_t>(seconds * 25));
      assert(renderer.metrics().decoder_opens == 1);
      assert(renderer.metrics().seeks == 1);
      assert(renderer.metrics().peak_live_source_frames <= 2);
    } catch (...) {
      std::filesystem::remove(path);
      throw;
    }
    std::filesystem::remove(path);
  }
}

void renders_h264_24fps_non_frame_aligned_source_range() {
  const auto path = std::filesystem::temp_directory_path() /
                    ("pulsar-e7-fix4-h264-24-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                     ".mp4");
  const auto command = shell_quote(PULSAR_TEST_FFMPEG_CLI) +
                       " -y -hide_banner -loglevel error -f lavfi "
                       "-i testsrc2=size=1280x720:rate=24:duration=11.458 "
                       "-c:v libx264 -pix_fmt yuv420p -bf 2 -g 48 "
                       "-video_track_timescale 12288 -an " + shell_quote(path);
  if (std::system(command.c_str()) != 0) {
    throw std::runtime_error{"could not create H264/24fps regression fixture"};
  }

  try {
    pulsar::application::ProjectDocument document;
    pulsar::application::CommandProcessor processor{document};
    pulsar::media::ffmpeg::FFmpegMediaProbe probe;
    const auto media = processor.import_asset(path, probe);
    const auto asset = document.find_asset(media);
    assert(asset.has_value());
    const auto source_start = pulsar::domain::MediaTime{2127, {1, 1000}};
    const auto source_end = pulsar::domain::MediaTime{11458, {1, 1000}};
    (void)processor.add_clip(media,
                             pulsar::domain::TimeRange{source_start, source_end},
                             pulsar::domain::MediaTime::zero());
    const auto snapshot = document.snapshot();
    const auto plan = pulsar::application::exporting::ExportPlan::build(snapshot);
    pulsar::media::ffmpeg::FFmpegOfflineVideoSourceFactory factory;
    pulsar::application::exporting::OfflineVideoRenderer renderer{plan, factory};
    for (std::int64_t index = 0; index < plan.frame_count(); ++index) {
      (void)renderer.render_frame(pulsar::domain::FrameNumber{index});
    }
    assert(renderer.metrics().output_frames == static_cast<std::uint64_t>(plan.frame_count()));
    assert(renderer.metrics().decoder_opens == 1);
  } catch (...) {
    std::filesystem::remove(path);
    throw;
  }
  std::filesystem::remove(path);
}

} // namespace

int main() {
  multilayer_real_decode_matches_single_track();
  renders_real_fixture_sequentially();
  normalizes_non_zero_stream_origin();
  renders_long_fixtures_with_bounded_state();
  renders_h264_24fps_non_frame_aligned_source_range();
}
