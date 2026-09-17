#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <cassert>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
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
  assert(snapshot.assets.empty());
  assert(snapshot.history_state_id == snapshot.saved_history_state_id);
  assert(snapshot.document_generation == 0);
  assert(!document.is_dirty());
}

void import_preserves_asset_identity_and_metadata() {
  application::ProjectDocument document;
  FakeProbe probe{probe_result()};
  const std::filesystem::path path{"media/source.mp4"};

  const domain::MediaId first = document.import_asset(path, probe);
  const domain::MediaId second = document.import_asset(path, probe);
  assert(first == domain::MediaId{1});
  assert(second == domain::MediaId{2});

  const auto asset = document.find_asset(first);
  assert(asset.has_value());
  assert(asset->source_locator() == path);
  assert(asset->video_stream_index() == 2);
  const domain::MediaTime expected_duration{3003, domain::Rational{1, 1000}};
  assert(asset->playable_duration() == expected_duration);
  assert(asset->metadata().codec_name == "h264");
  assert(asset->metadata().width == 1920);
  assert(asset->metadata().height == 1080);
  assert(asset->availability() == application::MediaAvailability::Available);
  assert(document.assets().size() == 2);
  assert(document.is_dirty());
  assert(document.document_generation() == 2);
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
  FakeProbe probe{probe_result()};
  document.mark_current_state_saved();
  probe.fail();
  const auto before = document.snapshot();

  bool threw = false;
  try {
    (void)document.import_asset("media/failure.mp4", probe);
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

void ids_are_monotonic_and_snapshot_isolated() {
  application::ProjectDocument document;
  FakeProbe probe{probe_result()};
  const auto media_id = document.import_asset("media/one.mp4", probe);
  const auto saved_snapshot = document.snapshot();
  const auto clip_one = document.allocate_clip_id();
  const auto clip_two = document.allocate_clip_id();
  assert(media_id == domain::MediaId{1});
  assert(clip_one == domain::ClipId{1});
  assert(clip_two == domain::ClipId{2});
  assert(document.snapshot().allocator_state.next_clip_id == 3);
  assert(saved_snapshot.assets.size() == 1);
  assert(saved_snapshot.allocator_state.next_clip_id == 1);
  assert(saved_snapshot.document_generation == 1);
  assert(document.document_generation() == 3);
}

void saved_state_marker_tracks_logical_state() {
  application::ProjectDocument document;
  FakeProbe probe{probe_result()};
  assert(!document.is_dirty());
  (void)document.import_asset("media/one.mp4", probe);
  assert(document.is_dirty());
  const auto imported_state = document.history_state_id();
  document.mark_current_state_saved();
  assert(!document.is_dirty());
  assert(document.saved_history_state_id() == imported_state);
  (void)document.allocate_clip_id();
  assert(document.is_dirty());
  document.mark_current_state_saved();
  assert(!document.is_dirty());
}

} // namespace

int main() {
  creation_is_deterministic_and_clean();
  import_preserves_asset_identity_and_metadata();
  unavailable_asset_is_valid_value_data();
  failed_import_does_not_mutate_document();
  ids_are_monotonic_and_snapshot_isolated();
  saved_state_marker_tracks_logical_state();
}
