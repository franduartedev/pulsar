#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <cassert>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {
using namespace pulsar;

class FakeProbe final : public application::MediaProbe {
public:
  explicit FakeProbe(application::MediaProbeResult result) : result_{std::move(result)} {}
  [[nodiscard]] application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    if (failure_) {
      throw std::runtime_error{"fake probe failure"};
    }
    auto result = result_;
    result.path = path;
    return result;
  }
  void fail() noexcept { failure_ = true; }
private:
  application::MediaProbeResult result_;
  bool failure_{false};
};

application::MediaProbeResult probe_result() {
  application::MediaProbeResult result;
  result.container_name = "matroska";
  result.video_streams.push_back(application::VideoStreamInfo{
      2, "h264", 1920, 1080, domain::Rational{1, 1000},
      domain::MediaTime{3003, domain::Rational{1, 1000}},
      domain::FrameRate{30000, 1001}});
  return result;
}

void creation_is_deterministic_and_clean() {
  application::ProjectDocument document;
  const auto snapshot = document.snapshot();
  assert(snapshot.timeline.id() == domain::TimelineId{1});
  assert(snapshot.video_track_id == domain::TrackId{1});
  assert(snapshot.timeline.tracks().size() == 1);
  assert(snapshot.timeline.tracks().front().kind() == domain::TrackKind::Video);
  assert(snapshot.history_state_id == snapshot.saved_history_state_id);
  assert(snapshot.document_generation == 0);
  assert(!document.is_dirty());
  assert(document.next_clip_id() == domain::ClipId{1});
  assert(!document.is_dirty());
}

void import_is_history_owned_and_preserves_metadata() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  FakeProbe probe{probe_result()};
  const std::filesystem::path path{"media/source.mp4"};
  const auto first = processor.import_asset(path, probe);
  const auto second = processor.import_asset(path, probe);
  assert(first == domain::MediaId{1});
  assert(second == domain::MediaId{2});
  const auto asset = document.find_asset(first);
  assert(asset.has_value());
  assert(asset->source_locator() == path);
  assert(asset->video_stream_index() == 2);
  const domain::MediaTime expected_duration{3003, domain::Rational{1, 1000}};
  assert(asset->playable_duration() == expected_duration);
  assert(asset->metadata().codec_name == "h264");
  assert(asset->availability() == application::MediaAvailability::Available);
  assert(document.document_generation() == 2);
  assert(processor.undo());
  assert(!document.find_asset(second).has_value());
  assert(processor.redo());
  assert(document.find_asset(second).has_value());
}

void unavailable_asset_is_valid_value_data() {
  const application::MediaAsset asset{
      domain::MediaId{7}, "missing/source.mp4", 2,
      domain::MediaTime{3003, domain::Rational{1, 1000}},
      application::VideoAssetMetadata{"h264", 1920, 1080, domain::Rational{1, 1000},
                                      std::nullopt, std::nullopt},
      application::MediaAvailability::Unavailable, "source is missing"};
  assert(asset.availability() == application::MediaAvailability::Unavailable);
  assert(asset.availability_reason().has_value());
}

void failed_import_does_not_mutate_document() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  FakeProbe probe{probe_result()};
  probe.fail();
  const auto before = document.snapshot();
  bool threw = false;
  try {
    (void)processor.import_asset("media/failure.mp4", probe);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
  const auto after = document.snapshot();
  assert(after.assets.empty());
  assert(after.history_state_id == before.history_state_id);
  assert(after.document_generation == before.document_generation);
  assert(after.allocator_state.next_media_id == before.allocator_state.next_media_id);
  assert(!document.is_dirty());
}

void snapshot_isolated_and_clip_candidate_is_observation() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  FakeProbe probe{probe_result()};
  const auto media = processor.import_asset("media/one.mp4", probe);
  const auto saved_snapshot = document.snapshot();
  const auto state = document.history_state_id();
  const auto generation = document.document_generation();
  assert(document.next_clip_id() == domain::ClipId{1});
  assert(document.history_state_id() == state && document.document_generation() == generation);
  (void)processor.add_clip(media, domain::TimeRange{domain::MediaTime::zero(), domain::MediaTime{1, domain::Rational{1, 1}}}, domain::MediaTime::zero());
  assert(saved_snapshot.timeline.duration() == domain::MediaTime::zero());
  assert(saved_snapshot.assets.size() == 1);
  assert(saved_snapshot.allocator_state.next_clip_id == 1);
  assert(document.next_clip_id() == domain::ClipId{2});
}

} // namespace

int main() {
  creation_is_deterministic_and_clean();
  import_is_history_owned_and_preserves_metadata();
  unavailable_asset_is_valid_value_data();
  failed_import_does_not_mutate_document();
  snapshot_isolated_and_clip_candidate_is_observation();
}
