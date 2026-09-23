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
  [[nodiscard]] pulsar::application::MediaProbeResult probe(
      const std::filesystem::path& path) const override {
    pulsar::application::MediaProbeResult result;
    result.path = path;
    result.video_streams.push_back(pulsar::application::VideoStreamInfo{
        0, "h264", 320, 180, pulsar::domain::Rational{1, 1000},
        pulsar::domain::MediaTime{1, {1, 1}}, pulsar::domain::FrameRate{25, 1}});
    return result;
  }
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
    OriginProbe probe;
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

} // namespace

int main() {
  renders_real_fixture_sequentially();
  normalizes_non_zero_stream_origin();
}
