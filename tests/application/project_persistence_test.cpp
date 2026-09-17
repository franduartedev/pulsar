#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/project/project_codec.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/project_file/file_project_repository.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {
using namespace pulsar;
using application::CommandProcessor;
using application::MediaProbe;
using application::MediaProbeResult;
using application::ProjectCodec;
using application::ProjectCodecError;
using application::ProjectDocument;
using application::ProjectSaveStatus;
using application::VideoStreamInfo;
using domain::MediaId;
using domain::MediaTime;
using domain::Rational;
using domain::TimeRange;

class FakeProbe final : public MediaProbe {
public:
  MediaProbeResult probe(const std::filesystem::path& path) const override {
    MediaProbeResult result;
    result.path = path;
    result.container_name = "matroska";
    result.video_streams.push_back(VideoStreamInfo{2, "h264", 1920, 1080, Rational{1, 90000},
                                                   MediaTime{120, Rational{1, 1}},
                                                   domain::FrameRate{30000, 1001}});
    return result;
  }
};

MediaTime t(std::int64_t value, Rational base = Rational{1, 1}) { return MediaTime{value, base}; }

ProjectDocument make_document() {
  ProjectDocument document;
  CommandProcessor processor{document};
  const auto first = processor.import_asset("media dir/clip \\\"one\\\"\\n.mp4", FakeProbe{});
  const auto second = processor.import_asset("second.mp4", FakeProbe{});
  (void)processor.add_clip(first, TimeRange{t(1001, Rational{1, 30000}), t(2002, Rational{1, 30000})}, t(0));
  (void)processor.add_clip(second, TimeRange{t(3, Rational{1, 2}), t(5, Rational{1, 2})}, t(4));
  return document;
}

void codec_roundtrip_and_canonical_output() {
  auto document = make_document();
  const auto snapshot = document.snapshot();
  const auto bytes = ProjectCodec::encode(snapshot);
  const auto state = ProjectCodec::decode(bytes);
  auto restored = ProjectDocument::restore(state);
  assert(ProjectCodec::encode(restored.snapshot()) == bytes);
  assert(!restored.is_dirty());
  assert(restored.history_state_id() == 1);
  assert(restored.saved_history_state_id() == 1);
  assert(restored.document_generation() == 0);
  assert(bytes.find("VIDEO_TRACK") != std::string::npos);
  assert(bytes.find("CLIP ") != std::string::npos);
}

void golden_fixture_decodes() {
  std::ifstream input{std::filesystem::path{PULSAR_SOURCE_DIR} / "tests" / "fixtures" /
                      "project_v1_golden.pulsar"};
  const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
  assert(!bytes.empty());
  auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  assert(restored.assets().size() == 2);
  assert(restored.timeline().tracks().front().clips().size() == 2);
  assert(ProjectCodec::encode(restored.snapshot()) == bytes);
}

void strict_decode_rejects_malformed_input() {
  const auto good = ProjectCodec::encode(ProjectDocument{}.snapshot());
  const std::vector<std::string> malformed{
      "PULSAR_PROJECT 2\n", "PULSAR_PROJECT 1\nTIMELINE 1\nEND\n",
      good.substr(0, good.size() - 4),
      good + "JUNK\n",
  };
  for (const auto& text : malformed) {
    bool rejected = false;
    try {
      (void)ProjectCodec::decode(text);
    } catch (const ProjectCodecError&) {
      rejected = true;
    }
    assert(rejected);
  }
}

void allocator_high_water_and_restore_validation() {
  auto document = make_document();
  auto snapshot = document.snapshot();
  snapshot.allocator_state.next_clip_id = 100;
  snapshot.allocator_state.next_media_id = 100;
  auto restored = ProjectDocument::restore(ProjectCodec::decode(ProjectCodec::encode(snapshot)));
  CommandProcessor processor{restored};
  const auto id = processor.add_clip(MediaId{1}, TimeRange{t(6), t(7)}, t(8));
  assert(id == domain::ClipId{100});

  snapshot.allocator_state.next_clip_id = 0;
  bool rejected = false;
  try {
    (void)ProjectDocument::restore(ProjectCodec::decode(ProjectCodec::encode(snapshot)));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void repository_atomic_save_and_missing_media_refresh() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("pulsar-project-test-" + std::to_string(
                        std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto media = root / "source clip.mp4";
  const auto project = root / "session.pulsar";
  { std::ofstream output{media}; output << "media"; }

  ProjectDocument document;
  CommandProcessor processor{document};
  const auto media_id = processor.import_asset(media, FakeProbe{});
  (void)processor.add_clip(media_id, TimeRange{t(0), t(2)}, t(0));
  pulsar::project_file::FileProjectRepository repository;
  const auto result = repository.save(project, document.snapshot());
  assert(result.status == ProjectSaveStatus::Durable);
  const auto canonical = ProjectCodec::encode(document.snapshot());
  {
    std::ifstream input{project, std::ios::binary};
    const std::string installed{std::istreambuf_iterator<char>{input}, {}};
    assert(installed == canonical);
  }
  const auto missing_parent = root / "does-not-exist" / "failed.pulsar";
  bool save_failed = false;
  try {
    (void)repository.save(missing_parent, document.snapshot());
  } catch (const application::ProjectRepositoryError&) {
    save_failed = true;
  }
  assert(save_failed);
  assert(!std::filesystem::exists(missing_parent));
  const auto state = repository.load(project);
  assert(state.assets.size() == 1);
  assert(state.assets.front().availability() == application::MediaAvailability::Available);

  std::filesystem::remove(media);
  const auto missing = repository.load(project);
  assert(missing.assets.front().availability() == application::MediaAvailability::Unavailable);
  auto restored = ProjectDocument::restore(missing);
  assert(restored.timeline().find_clip(domain::ClipId{1}) != nullptr);
  assert(restored.timeline().duration() == t(2));
  std::filesystem::remove_all(root);
}

} // namespace

int main() {
  codec_roundtrip_and_canonical_output();
  golden_fixture_decodes();
  strict_decode_rejects_malformed_input();
  allocator_high_water_and_restore_validation();
  repository_atomic_save_and_missing_media_refresh();
}
