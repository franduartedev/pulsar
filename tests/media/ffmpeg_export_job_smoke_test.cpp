#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/export/export_job.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_export_media_writer.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_audio_source.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_offline_video_source.hpp>
#include <pulsar/project_file/filesystem_export_output_publisher.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using namespace pulsar;
namespace exporting = application::exporting;

class WriterFactory final : public exporting::ExportMediaWriterFactory {
public:
  std::unique_ptr<exporting::ExportMediaWriter> open(
      const exporting::ExportPlan& plan,
      const std::filesystem::path& temporary_output) override {
    return std::make_unique<media::ffmpeg::FFmpegExportMediaWriter>(plan, temporary_output);
  }
};

void real_e2_e3_e4_export_smoke() {
  const auto fixture = std::filesystem::path{PULSAR_SOURCE_DIR} /
                       "tests/fixtures/audio_decode_fixture.mp4";
  const auto destination = std::filesystem::temp_directory_path() /
                           "pulsar-e5-real-export-smoke.mp4";
  std::filesystem::remove(destination);

  project_file::FilesystemExportOutputPublisher publisher;
  {
    std::ofstream old_file{destination};
    old_file << "old";
    const auto replacement = publisher.create_temporary_sibling(destination);
    std::ofstream new_file{replacement};
    new_file << "new";
    new_file.close();
    publisher.publish_replace_existing_safely(replacement, destination);
    std::ifstream published{destination};
    std::string content;
    published >> content;
    assert(content == "new");
  }

  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  media::ffmpeg::FFmpegMediaProbe probe;
  const auto media = processor.import_asset(fixture, probe);
  const auto asset = document.find_asset(media);
  assert(asset.has_value());
  (void)processor.add_clip(media,
                           domain::TimeRange{domain::MediaTime::zero(), asset->playable_duration()},
                           domain::MediaTime::zero());
  const auto snapshot = document.snapshot();
  const auto plan = exporting::ExportPlan::build(snapshot);

  media::ffmpeg::FFmpegOfflineVideoSourceFactory video_sources;
  media::ffmpeg::FFmpegOfflineAudioSourceFactory audio_sources;
  WriterFactory writers;
  exporting::ExportJob job{video_sources, audio_sources, writers, publisher};
  std::vector<double> progress;
  const auto result = job.run(exporting::ExportRequest{snapshot, destination,
                                                       exporting::ExportProfile::beta_v01()},
                              {}, [&](const exporting::ExportProgress& value) {
                                progress.push_back(value.fraction);
                              });

  assert(result.progress.fraction == 1.0);
  assert(std::filesystem::exists(destination));
  assert(std::filesystem::file_size(destination) > 0);
  for (std::size_t index = 1; index < progress.size(); ++index) {
    assert(progress[index] >= progress[index - 1]);
  }
  const auto parent = destination.parent_path();
  for (const auto& entry : std::filesystem::directory_iterator(parent)) {
    assert(entry.path().filename().string().find("pulsar-e5-real-export-smoke.mp4.pulsar-") ==
           std::string::npos);
  }

  const auto output = probe.probe(destination);
  assert(output.video_streams.size() == 1);
  assert(output.audio_streams.size() == 1);
  assert(output.video_streams.front().codec_name == "h264");
  assert(output.audio_streams.front().codec_name == "aac");
  assert(output.audio_streams.front().duration.has_value());
  assert(*output.audio_streams.front().duration == plan.output_duration());
  std::filesystem::remove(destination);
}

} // namespace

int main() {
  real_e2_e3_e4_export_smoke();
}
