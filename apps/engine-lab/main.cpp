#include <pulsar/application/bootstrap.hpp>
#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/domain/time/frame_number.hpp>
#include <pulsar/domain/time/frame_rate.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/time_range.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/timeline.hpp>
#include <pulsar/domain/timeline/track.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>
#include <pulsar/application/media/media_probe_error.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

void print_seconds(const std::optional<pulsar::domain::MediaTime>& time) {
  if (!time.has_value()) {
    std::cout << "unknown";
    return;
  }

  const auto seconds = time->seconds();
  std::cout << seconds.numerator();
  if (seconds.denominator() != 1) {
    std::cout << '/' << seconds.denominator();
  }
  std::cout << " s";
}

void print_probe_result(const pulsar::application::MediaProbeResult& result) {
  std::cout << "PULSAR Engine Lab\n";
  std::cout << "Media Probe\n";
  std::cout << "File: " << result.path.filename().string() << '\n';
  std::cout << "Container: "
            << (result.container_name.has_value() ? *result.container_name : "unknown") << '\n';
  std::cout << "Duration: ";
  print_seconds(result.duration);
  std::cout << "\n";

  for (const auto& stream : result.video_streams) {
    std::cout << "Video stream #" << stream.stream_index << '\n';
    std::cout << "Codec: " << stream.codec_name << '\n';
    std::cout << "Resolution: " << stream.width << 'x' << stream.height << '\n';
    std::cout << "Time base: " << stream.time_base.numerator() << '/'
              << stream.time_base.denominator() << '\n';
    std::cout << "Duration: ";
    print_seconds(stream.duration);
    std::cout << "\n";
    std::cout << "Nominal FPS: ";
    if (stream.nominal_frame_rate.has_value()) {
      const auto rate = stream.nominal_frame_rate->rational();
      std::cout << rate.numerator() << '/' << rate.denominator();
    } else {
      std::cout << "unknown";
    }
    std::cout << "\n";
  }

  for (const auto& stream : result.audio_streams) {
    std::cout << "Audio stream #" << stream.stream_index << '\n';
    std::cout << "Codec: " << stream.codec_name << '\n';
    std::cout << "Sample rate: " << stream.sample_rate << '\n';
    std::cout << "Channels: " << stream.channel_count << '\n';
    std::cout << "Time base: " << stream.time_base.numerator() << '/'
              << stream.time_base.denominator() << '\n';
    std::cout << "Duration: ";
    print_seconds(stream.duration);
    std::cout << "\n";
  }
}

int run_probe(const std::filesystem::path& path) {
  const auto probe = pulsar::media::ffmpeg::FFmpegMediaProbe{};
  const auto result = probe.probe(path);
  print_probe_result(result);
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string_view{argv[1]} == "--probe") {
    try {
      return run_probe(std::filesystem::path{argv[2]});
    } catch (const pulsar::application::MediaProbeError& error) {
      std::cerr << "Media probe failed: " << error.what() << '\n';
      return 2;
    } catch (const std::exception& error) {
      std::cerr << "Media probe failed: " << error.what() << '\n';
      return 2;
    }
  }

  if (argc != 1) {
    std::cerr << "Usage: pulsar-engine-lab [--probe <local-media-file>]\n";
    return 2;
  }

  using namespace pulsar::domain;

  const auto rate = pulsar::domain::FrameRate{30000, 1001};
  const auto frame = pulsar::domain::FrameNumber{30000};
  const auto time = pulsar::domain::MediaTime::from_frame(frame, rate);
  const auto seconds = time.seconds();
  auto timeline = Timeline{TimelineId{1}};
  timeline.add_track(Track{TrackId{1}, TrackKind::Video});
  timeline.add_track(Track{TrackId{2}, TrackKind::Audio});
  timeline.insert_clip(TrackId{1},
                       Clip{ClipId{1}, MediaId{1},
                            TimeRange{MediaTime{0, Rational{1, 1}}, MediaTime{5, Rational{1, 1}}},
                            MediaTime{0, Rational{1, 1}}});
  timeline.insert_clip(TrackId{1},
                       Clip{ClipId{2}, MediaId{2},
                            TimeRange{MediaTime{0, Rational{1, 1}}, MediaTime{5, Rational{1, 1}}},
                            MediaTime{5, Rational{1, 1}}});
  timeline.insert_clip(TrackId{2},
                       Clip{ClipId{3}, MediaId{3},
                            TimeRange{MediaTime{0, Rational{1, 1}}, MediaTime{10, Rational{1, 1}}},
                            MediaTime{0, Rational{1, 1}}});
  const auto timeline_duration = timeline.duration().seconds();
  std::size_t clip_count = 0;
  for (const auto& track : timeline.tracks()) {
    clip_count += track.clips().size();
  }

  std::cout << "PULSAR Engine Lab\n";
  std::cout << "Foundation build OK\n";
  std::cout << "Time model\n";
  std::cout << "Rate: " << rate.rational().numerator() << '/' << rate.rational().denominator()
            << " fps\n";
  std::cout << "Frame: " << frame.value() << '\n';
  std::cout << "Time: " << seconds.numerator();
  if (seconds.denominator() != 1) {
    std::cout << '/' << seconds.denominator();
  }
  std::cout << " s\n";
  std::cout << "Timeline model\n";
  std::cout << "Tracks: " << timeline.tracks().size() << '\n';
  std::cout << "Clips: " << clip_count << '\n';
  std::cout << "Duration: " << timeline_duration.numerator();
  if (timeline_duration.denominator() != 1) {
    std::cout << '/' << timeline_duration.denominator();
  }
  std::cout << " s\n";

  auto command_timeline = Timeline{TimelineId{10}};
  command_timeline.add_track(Track{TrackId{10}, TrackKind::Video});
  auto processor = pulsar::application::CommandProcessor{command_timeline};
  processor.execute(pulsar::application::AddClipCommand{
      TrackId{10},
      Clip{ClipId{10}, MediaId{10},
           TimeRange{MediaTime{0, Rational{1, 1}}, MediaTime{1, Rational{1, 1}}},
           MediaTime{0, Rational{1, 1}}}});
  const bool add_ok = command_timeline.find_clip(ClipId{10}) != nullptr;
  const bool undo_ok = processor.undo() && command_timeline.find_clip(ClipId{10}) == nullptr;
  const bool redo_ok = processor.redo() && command_timeline.find_clip(ClipId{10}) != nullptr;

  std::cout << "Command model\n";
  std::cout << "Add clip: " << (add_ok ? "OK" : "FAILED") << '\n';
  std::cout << "Undo: " << (undo_ok ? "OK" : "FAILED") << '\n';
  std::cout << "Redo: " << (redo_ok ? "OK" : "FAILED") << '\n';
  return 0;
}
