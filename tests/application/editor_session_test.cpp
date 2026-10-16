#include "duration_sample_ring.hpp"
#include "editor_session.hpp"
#include "viewer_transform_mapping.hpp"
#include "project_path_resolution.hpp"
#include "timeline_zoom.hpp"

#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/audio/audio_contributor_resolver.hpp>
#include <pulsar/application/playback/playback_worker.hpp>
#include <pulsar/application/project/project_codec.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/track.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <limits>
#include <variant>

namespace {
using namespace pulsar;
using desktop::EditorSession;
using desktop::TimelineViewport;
using application::MediaProbe;
using application::MediaProbeResult;
using application::VideoStreamInfo;
using domain::MediaTime;
using domain::Rational;
using domain::TimeRange;


class CountingWorker final : public application::PlaybackMediaWorker {
public:
  explicit CountingWorker(int& count) : count_{count} { ++count_; }
  application::VideoSourceDescriptor describe() const override {
    return application::VideoSourceDescriptor{application::StreamTimestamp{MediaTime::zero()},
                                              Rational{1, 1}, MediaTime{100, Rational{1, 1}}, true};
  }
  void request_read(application::PlaybackGeneration) override {}
  void request_seek_for_presentation(application::PlaybackGeneration,
                                     application::StreamTimestamp) override {}
  void cancel_pending() override {}
  std::optional<application::PlaybackWorkerEvent> poll_event() override { return std::nullopt; }
private:
  int& count_;
};

bool same_range(const TimeRange& lhs, const TimeRange& rhs) { return lhs.start() == rhs.start() && lhs.end() == rhs.end(); }

class Probe final : public MediaProbe {
public:
  MediaProbeResult probe(const std::filesystem::path& path) const override {
    MediaProbeResult result;
    result.path = path;
    result.video_streams.push_back(VideoStreamInfo{0, "h264", 1920, 1080, Rational{1, 90000},
                                                   MediaTime{100, Rational{1, 1}}, std::nullopt});
    return result;
  }
};

void viewport_is_exactly_guarded() {
  TimelineViewport viewport{MediaTime{5, Rational{1, 1}}};
  assert(viewport.time_at(0.0, 100.0).value() == MediaTime::zero());
  const auto midpoint = MediaTime{2'500'000, Rational{1, 1'000'000}};
  const auto endpoint = MediaTime{5, Rational{1, 1}};
  assert(viewport.time_at(50.0, 100.0).value() == midpoint);
  assert(viewport.time_at(100.0, 100.0).value() == endpoint);
  assert(viewport.time_at(25.5, 100.0) == viewport.time_at(25.5, 100.0));
  assert(!viewport.time_at(std::numeric_limits<double>::quiet_NaN(), 100.0).has_value());
  assert(!viewport.time_at(std::numeric_limits<double>::infinity(), 100.0).has_value());
  assert(!viewport.time_at(1.0, 0.0).has_value());
  assert(!viewport.time_at(1.0, -1.0).has_value());
  assert(viewport.time_at(-10.0, 100.0).value() == MediaTime::zero());
  assert(viewport.time_at(110.0, 100.0).value() == endpoint);
  assert(viewport.pixel_at(MediaTime{1001, Rational{1, 30000}}, 1000.0) >= 0.0);
}

void zoom_and_pan_preserve_editorial_time_mapping() {
  const auto start = MediaTime{3, Rational{1, 1}};
  const auto duration = MediaTime{2, Rational{1, 1}};
  TimelineViewport viewport{start, duration};
  assert(viewport.time_at(0.0, 1000.0).value() == start);
  assert(viewport.time_at(1000.0, 1000.0).value() == start + duration);
  assert(viewport.pixel_at(start, 1000.0) == 0.0);
  assert(viewport.pixel_at(start + duration, 1000.0) == 1000.0);
  // The viewport changes the visible window, not the clip's source interval.
  const auto source_start = MediaTime{7, Rational{1, 1}};
  const auto source_end = MediaTime{9, Rational{1, 1}};
  assert(source_end - source_start == duration);
}


void editing_actions_use_document_commands() {
  Probe probe;
  EditorSession session;
  const auto media = session.processor().import_asset("edit.mp4", probe);
  const auto clip = session.processor().add_clip(media, TimeRange{MediaTime::zero(), MediaTime{10, Rational{1, 1}}}, MediaTime::zero());
  session.synchronize_document_state();
  session.select_clip(clip);
  session.move_clip(clip, 20.0, 30.0, 100.0);
  const auto one = MediaTime{1, Rational{1, 1}};
  assert(session.document().timeline().find_clip(clip)->timeline_start() == one);
  const auto source_before = session.document().timeline().find_clip(clip)->source_range();
  const auto trim_target = session.time_at(30.0, 100.0).value();
  session.trim_clip_start(clip, 30.0, 100.0);
  auto current = session.document().timeline().find_clip(clip);
  assert(current->source_range().start() == source_before.start() + (trim_target - one));
  assert(current->id() == clip && current->source_range().end() == source_before.end());
  session.seek(MediaTime{5, Rational{1, 1}});
  assert(session.can_split());
  const auto right = session.split_selected_at_playhead();
  assert(right != clip);
  assert(session.selected_clip() == clip);
  assert(session.document().timeline().find_clip(right) != nullptr);
  session.delete_selected();
  assert(session.document().timeline().find_clip(clip) == nullptr);
  assert(session.document().find_asset(media).has_value());
  assert(!session.selected_clip().has_value());
  assert(session.undo());
  assert(session.document().timeline().find_clip(clip) != nullptr);
  assert(session.redo());
  assert(session.document().timeline().find_clip(clip) == nullptr);
}

void live_audio_property_edit_preserves_playback_position_and_intent() {
  Probe probe;
  EditorSession session;
  const auto media = session.processor().import_asset("live.mp4", probe);
  const auto clip = session.processor().add_clip(
      media, TimeRange{MediaTime::zero(), MediaTime{10, Rational{1, 1}}}, MediaTime::zero());
  session.synchronize_document_state();
  session.seek(MediaTime{3, Rational{1, 1}});
  session.play();
  session.set_clip_audio_gain(clip, -6.0);
  const auto expected = MediaTime{3, Rational{1, 1}};
  const auto upper = MediaTime{3001, Rational{1, 1000}};
  assert(session.playback_position() >= expected && session.playback_position() < upper);
  assert(session.playback_intent() == application::PlaybackIntent::Playing);
  session.set_clip_muted(clip, true);
  assert(session.playback_position() >= expected && session.playback_position() < upper);
  assert(session.playback_intent() == application::PlaybackIntent::Playing);
}

void session_read_model_and_selection() {
  EditorSession session;
  Probe probe;
  const auto media = session.processor().import_asset("offline/source.mp4", probe);
  const auto clip = session.processor().add_clip(media, TimeRange{MediaTime::zero(), MediaTime{5, Rational{1, 1}}}, MediaTime::zero());
  session.rebuild_read_model();
  assert(session.timeline_views().size() == 1);
  assert(session.timeline_views().front().key == std::to_string(clip.value()));
  assert(!session.timeline_views().front().missing);
  const std::uint64_t large_id = 1ULL << 54;
  const std::string opaque_key = std::to_string(large_id);
  assert(std::stoull(opaque_key) == large_id);
  session.select_clip(clip);
  assert(session.timeline_views().front().selected);
  const auto generation_before_selection = session.document().document_generation();
  session.select_clip(std::nullopt);
  session.select_clip(clip);
  assert(session.document().document_generation() == generation_before_selection);
  session.processor().move_clip(clip, session.document().video_track_id(), MediaTime{2, Rational{1, 1}});
  session.rebuild_read_model();
  assert(session.timeline_views().front().selected);
  session.processor().remove_clip(clip);
  session.synchronize_document_state();
  assert(!session.selected_clip().has_value());

  EditorSession rounded;
  const auto rounded_media = rounded.processor().import_asset("rounded.mp4", probe);
  (void)rounded.processor().add_clip(rounded_media,
      TimeRange{MediaTime::zero(), MediaTime{19996, Rational{1, 10000}}}, MediaTime::zero());
  rounded.synchronize_document_state();
  assert(rounded.timeline_views().front().end == "00:00:02.000");

  EditorSession project;
  const auto first = project.processor().import_asset("A.mp4", probe);
  const auto first_clip = project.processor().add_clip(first, TimeRange{MediaTime::zero(), MediaTime{2, Rational{1, 1}}}, MediaTime::zero());
  const auto second = project.processor().import_asset("B.mp4", probe);
  assert(project.document().assets().size() == 2);
  assert(project.document().timeline().find_clip(first_clip) != nullptr);
  (void)second;

  int worker_creations = 0;
  EditorSession playback_session{[&worker_creations](const std::filesystem::path&, int) {
    return std::make_unique<CountingWorker>(worker_creations);
  }};
  const auto playback_media = playback_session.processor().import_asset("playback.mp4", probe);
  const auto playback_clip = playback_session.processor().add_clip(
      playback_media, TimeRange{MediaTime::zero(), MediaTime{5, Rational{1, 1}}}, MediaTime::zero());
  playback_session.synchronize_document_state();
  playback_session.seek(MediaTime{1, Rational{1, 1}});
  playback_session.play();
  assert(playback_session.playback_intent() == application::PlaybackIntent::Playing);
  (void)playback_session.update_playback();
  playback_session.pause();
  assert(playback_session.playback_intent() == application::PlaybackIntent::Paused);
  assert(worker_creations == 1);
  const auto generation_before = playback_session.document().document_generation();
  playback_session.select_clip(playback_clip);
  playback_session.select_clip(std::nullopt);
  playback_session.select_clip(playback_clip);
  assert(worker_creations == 1);
  assert(playback_session.document().document_generation() == generation_before);
}


void final_pointer_edit_regressions() {
  Probe probe;

  EditorSession moves;
  const auto media = moves.processor().import_asset("moves.mp4", probe);
  const auto first = moves.processor().add_clip(media, TimeRange{MediaTime::zero(), MediaTime{5, Rational{1, 1}}}, MediaTime::zero());
  const auto second = moves.processor().add_clip(media, TimeRange{MediaTime::zero(), MediaTime{5, Rational{1, 1}}}, MediaTime{10, Rational{1, 1}});
  moves.synchronize_document_state();
  moves.select_clip(first);
  const auto source = moves.document().timeline().find_clip(second)->source_range();
  const auto generation_before_move = moves.document().document_generation();
  const auto history_before_move = moves.processor().undo_size();
  moves.move_clip(second, 80.0, 90.0, 100.0);
  assert(moves.selected_clip() == second);
  assert(moves.processor().undo_size() == history_before_move + 1);
  assert(same_range(moves.document().timeline().find_clip(second)->source_range(), source));
  assert(moves.document().document_generation() == generation_before_move + 1);

  moves.select_clip(first);
  const auto failed_generation = moves.document().document_generation();
  const auto failed_history = moves.processor().undo_size();
  bool rejected = false;
  try { moves.move_clip(second, 80.0, 0.0, 100.0); } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected && moves.selected_clip() == first);
  assert(moves.document().document_generation() == failed_generation);
  assert(moves.processor().undo_size() == failed_history);
  rejected = false;
  try { moves.move_clip(second, 80.0, 20.0, 100.0); } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected && moves.selected_clip() == first);
  assert(same_range(moves.document().timeline().find_clip(second)->source_range(), source));

  EditorSession trims;
  const auto trim_media = trims.processor().import_asset("trim.mp4", probe);
  const auto trim_clip = trims.processor().add_clip(trim_media,
      TimeRange{MediaTime{5, Rational{1, 1}}, MediaTime{15, Rational{1, 1}}}, MediaTime::zero());
  (void)trims.processor().add_clip(trim_media,
      TimeRange{MediaTime::zero(), MediaTime{1, Rational{1, 1}}}, MediaTime{12, Rational{1, 1}});
  trims.synchronize_document_state();
  trims.select_clip(trim_clip);
  const auto trim_target = trims.time_at(60.0, 100.0).value();
  trims.trim_clip_end(trim_clip, 60.0, 100.0);
  auto trimmed = trims.document().timeline().find_clip(trim_clip);
  const auto expected_source_end = MediaTime{5, Rational{1, 1}} + trim_target;
  assert(trimmed->timeline_range().end() == trim_target);
  assert(trimmed->source_range().end() == expected_source_end);
  const auto after_trim = trimmed->source_range();
  bool trim_rejected = false;
  try { trims.trim_clip_end(trim_clip, 90.0, 100.0); } catch (const std::invalid_argument&) { trim_rejected = true; }
  assert(trim_rejected);
  assert(same_range(trims.document().timeline().find_clip(trim_clip)->source_range(), after_trim));
  assert(trims.undo());
  const auto source_fifteen = MediaTime{15, Rational{1, 1}};
  assert(trims.document().timeline().find_clip(trim_clip)->source_range().end() == source_fifteen);
  assert(trims.redo());
  assert(same_range(trims.document().timeline().find_clip(trim_clip)->source_range(), after_trim));

  EditorSession splits;
  const auto split_media = splits.processor().import_asset("split.mp4", probe);
  const auto split_clip = splits.processor().add_clip(split_media,
      TimeRange{MediaTime::zero(), MediaTime{10, Rational{1, 1}}}, MediaTime::zero());
  splits.synchronize_document_state();
  splits.select_clip(split_clip);
  const auto next_before = splits.document().next_clip_id();
  splits.seek(MediaTime::zero());
  bool split_rejected = false;
  try { (void)splits.split_selected_at_playhead(); } catch (const std::invalid_argument&) { split_rejected = true; }
  assert(split_rejected && splits.document().next_clip_id() == next_before);
  splits.seek(MediaTime{10, Rational{1, 1}});
  split_rejected = false;
  try { (void)splits.split_selected_at_playhead(); } catch (const std::invalid_argument&) { split_rejected = true; }
  assert(split_rejected && splits.document().next_clip_id() == next_before);
  splits.seek(MediaTime{5, Rational{1, 1}});
  const auto right = splits.split_selected_at_playhead();
  assert(splits.selected_clip() == split_clip);
  assert(splits.undo());
  assert(splits.document().timeline().find_clip(right) == nullptr);
  assert(splits.document().timeline().find_clip(split_clip) != nullptr);
  assert(splits.redo());
  assert(splits.document().timeline().find_clip(right) != nullptr);
}

} // namespace

void synchronization_diagnostics_semantics() {
  Probe probe;
  EditorSession session;
  // The constructor performs the initial synchronization.
  auto baseline = session.synchronization_diagnostics();
  assert(baseline.performed_count == 1);
  assert(baseline.skipped_count == 0);
  assert(baseline.recent_durations.size() == 1);

  // No document mutation: early-return path only.
  session.synchronize_document_state();
  const auto idle = session.synchronization_diagnostics();
  assert(idle.performed_count == 1);
  assert(idle.skipped_count == 1);
  assert(idle.recent_durations.size() == 1);

  // A real edit performs exactly one synchronization with one more sample.
  (void)session.processor().import_asset("diag.mp4", probe);
  session.synchronize_document_state();
  const auto edited = session.synchronization_diagnostics();
  assert(edited.performed_count == 2);
  assert(edited.skipped_count == 1);
  assert(edited.recent_durations.size() == 2);

  // Settled again: skipped only.
  session.synchronize_document_state();
  const auto settled = session.synchronization_diagnostics();
  assert(settled.performed_count == 2);
  assert(settled.skipped_count == 2);
  assert(settled.recent_durations.size() == 2);
}

class FakeProjectRepository final : public application::ProjectRepository {
 public:
  [[nodiscard]] application::ProjectSaveResult save(
      const std::filesystem::path& destination,
      const application::ProjectSnapshot& snapshot) const override {
    last_destination = destination;
    last_snapshot = snapshot;
    if (on_save) {
      on_save();
    }
    if (throw_on_save) {
      throw application::ProjectRepositoryError{"fake save failure"};
    }
    return result_to_return;
  }

  [[nodiscard]] application::ProjectRestoreState load(
      const std::filesystem::path&) const override {
    throw application::ProjectRepositoryError{"fake load unsupported"};
  }

  mutable std::optional<std::filesystem::path> last_destination;
  mutable std::optional<application::ProjectSnapshot> last_snapshot;
  application::ProjectSaveResult result_to_return{application::ProjectSaveStatus::Durable};
  bool throw_on_save{false};
  std::function<void()> on_save;
};

void save_project_marks_captured_history_state() {
  Probe probe;
  EditorSession session;
  (void)session.processor().import_asset("save.mp4", probe);
  assert(session.document().is_dirty());
  const auto current = session.document().history_state_id();
  const auto generation = session.document().document_generation();
  const bool had_undo = session.can_undo();
  const bool had_redo = session.can_redo();

  FakeProjectRepository repository;
  const auto result = session.save_project("project.pulsar", repository);
  assert(result.status == application::ProjectSaveStatus::Durable);
  assert(repository.last_destination == std::filesystem::path{"project.pulsar"});
  assert(repository.last_snapshot.has_value());
  assert(repository.last_snapshot->history_state_id == current);
  assert(session.document().saved_history_state_id() == current);
  assert(!session.document().is_dirty());
  assert(session.document().document_generation() == generation);
  assert(session.can_undo() == had_undo);
  assert(session.can_redo() == had_redo);
}

void save_project_uncertain_durability_still_commits() {
  Probe probe;
  EditorSession session;
  (void)session.processor().import_asset("uncertain.mp4", probe);
  const auto current = session.document().history_state_id();

  FakeProjectRepository repository;
  repository.result_to_return = application::ProjectSaveResult{
      application::ProjectSaveStatus::CommittedDurabilityUncertain};
  const auto result = session.save_project("project.pulsar", repository);
  assert(result.status == application::ProjectSaveStatus::CommittedDurabilityUncertain);
  assert(session.document().saved_history_state_id() == current);
  assert(!session.document().is_dirty());
}

void save_project_failure_marks_nothing_saved() {
  Probe probe;
  EditorSession session;
  (void)session.processor().import_asset("failing.mp4", probe);
  const auto current = session.document().history_state_id();
  const auto saved_before = session.document().saved_history_state_id();
  const auto generation = session.document().document_generation();
  const bool had_undo = session.can_undo();
  const bool had_redo = session.can_redo();

  FakeProjectRepository repository;
  repository.throw_on_save = true;
  bool failed = false;
  try {
    (void)session.save_project("project.pulsar", repository);
  } catch (const application::ProjectRepositoryError&) {
    failed = true;
  }
  assert(failed);
  assert(session.document().saved_history_state_id() == saved_before);
  assert(session.document().history_state_id() == current);
  assert(session.document().is_dirty());
  assert(session.document().document_generation() == generation);
  assert(session.can_undo() == had_undo);
  assert(session.can_redo() == had_redo);
}

void save_project_marks_captured_snapshot_not_current_state() {
  Probe probe;
  EditorSession session;
  (void)session.processor().import_asset("reentrant.mp4", probe);
  const auto state_a = session.document().history_state_id();

  FakeProjectRepository repository;
  repository.on_save = [&session, &probe, state_a] {
    assert(session.document().history_state_id() == state_a);
    (void)session.processor().import_asset("second.mp4", probe);
    assert(session.document().history_state_id() != state_a);
  };
  const auto result = session.save_project("project.pulsar", repository);
  assert(result.status == application::ProjectSaveStatus::Durable);
  assert(repository.last_snapshot.has_value());
  assert(repository.last_snapshot->history_state_id == state_a);
  assert(session.document().history_state_id() != state_a);
  assert(session.document().saved_history_state_id() == state_a);
  assert(session.document().is_dirty());
  assert(session.is_dirty());
}

void editor_session_dirty_mirrors_saved_identity() {
  Probe probe;
  EditorSession session;
  assert(!session.is_dirty());
  (void)session.processor().import_asset("dirty.mp4", probe);
  assert(session.is_dirty());
  FakeProjectRepository repository;
  (void)session.save_project("project.pulsar", repository);
  assert(!session.is_dirty());
  (void)session.processor().add_clip(
      session.document().find_asset(domain::MediaId{1}).value().id(),
      domain::TimeRange{domain::MediaTime::zero(), domain::MediaTime{1, domain::Rational{1, 1}}},
      domain::MediaTime::zero());
  assert(session.is_dirty());
  assert(session.undo());
  assert(!session.is_dirty());
  assert(session.redo());
  assert(session.is_dirty());
}

void duration_ring_wraparound() {
  DurationSampleRing ring;
  assert(ring.size() == 0);
  for (std::uint64_t i = 0; i < DurationSampleRing::capacity + 6; ++i) {
    ring.push(std::chrono::nanoseconds{static_cast<long long>(i)});
  }
  assert(ring.size() == DurationSampleRing::capacity);
  const auto snapshot = ring.snapshot();
  assert(snapshot.size() == DurationSampleRing::capacity);
  // Oldest-first: the first 6 samples fell off.
  assert(snapshot.front() == std::chrono::nanoseconds{6});
  assert(snapshot.back() ==
         std::chrono::nanoseconds{static_cast<long long>(DurationSampleRing::capacity + 6 - 1)});
}

application::ProjectRestoreState make_restore_state(
    domain::TimelineId timeline_id, domain::TrackId video_track,
    std::vector<application::MediaAsset> assets, domain::Track track,
    application::ProjectAllocatorState allocator) {
  application::ProjectRestoreTrack restore_track{track.id(), domain::TrackKind::Video, {}};
  for (const auto& clip : track.clips()) {
    restore_track.clips.push_back(clip);
  }
  application::ProjectRestoreState state{timeline_id, {restore_track}, video_track,
                                         std::move(assets), allocator};
  return state;
}

application::VideoAssetMetadata test_metadata() {
  return application::VideoAssetMetadata{"h264", 1920, 1080, Rational{1, 90000}, std::nullopt,
                                         std::nullopt};
}

void restored_session_matches_persisted_editorial_state() {
  Probe probe;
  EditorSession original;
  const auto media_a = original.processor().import_asset("a.mp4", probe);
  const auto media_b = original.processor().import_asset("b.mp4", probe);
  const auto clip_a = original.processor().add_clip(
      media_a, TimeRange{MediaTime::zero(), MediaTime{4, Rational{1, 1}}}, MediaTime::zero());
  const auto clip_b = original.processor().add_clip(
      media_b, TimeRange{MediaTime::zero(), MediaTime{4, Rational{1, 1}}}, MediaTime{4, Rational{1, 1}});
  original.processor().move_clip(clip_b, original.document().video_track_id(),
                                 MediaTime{6, Rational{1, 1}});
  original.processor().trim_clip_start(clip_a, MediaTime{1, Rational{1, 1}});
  const auto right = original.processor().split_clip(clip_a, MediaTime{2, Rational{1, 1}});
  assert(right != clip_a);
  original.synchronize_document_state();

  const auto bytes = application::ProjectCodec::encode(original.document().snapshot());
  auto state = application::ProjectCodec::decode(bytes);
  int worker_calls = 0;
  EditorSession loaded(std::move(state), [&worker_calls](const std::filesystem::path&, int) {
    ++worker_calls;
    return std::unique_ptr<application::PlaybackMediaWorker>{};
  });

  assert(application::ProjectCodec::encode(loaded.document().snapshot()) == bytes);
  assert(loaded.document().assets().size() == 2);
  assert(loaded.document().find_asset(media_a)->source_locator() == std::filesystem::path{"a.mp4"});
  assert(loaded.document().find_asset(media_b)->source_locator() == std::filesystem::path{"b.mp4"});
  const auto* loaded_a = loaded.document().timeline().find_clip(clip_a);
  const auto* loaded_right = loaded.document().timeline().find_clip(right);
  const auto* loaded_b = loaded.document().timeline().find_clip(clip_b);
  assert(loaded_a != nullptr && loaded_right != nullptr && loaded_b != nullptr);
  assert((loaded_a->timeline_start() == MediaTime{1, Rational{1, 1}}));
  assert((loaded_a->source_range().start() == MediaTime{1, Rational{1, 1}}));
  assert((loaded_b->timeline_start() == MediaTime{6, Rational{1, 1}}));
  assert(loaded.document().next_clip_id() == original.document().next_clip_id());
  assert(loaded.document().history_state_id() == 1);
  assert(loaded.document().saved_history_state_id() == 1);
  assert(loaded.document().document_generation() == 0);
  assert(!loaded.document().is_dirty());
  assert(!loaded.can_undo());
  assert(!loaded.can_redo());
  assert(!loaded.selected_clip().has_value());
  assert(worker_calls == 0);

  // Fresh history root: one new edit is undoable without touching the baseline.
  loaded.processor().move_clip(clip_b, loaded.document().video_track_id(),
                               MediaTime{7, Rational{1, 1}});
  assert(loaded.can_undo());
  assert(loaded.undo());
  assert((loaded.document().timeline().find_clip(clip_b)->timeline_start() ==
         MediaTime{6, Rational{1, 1}}));
  assert(!loaded.can_undo());
}

void missing_media_survives_restore_without_workers() {
  application::VideoAssetMetadata metadata{"h264", 1920, 1080, Rational{1, 90000}, std::nullopt,
                                           std::nullopt};
  application::MediaAsset asset{domain::MediaId{7}, "media/offline.mp4", 0,
                               MediaTime{10, Rational{1, 1}}, metadata,
                               application::MediaAvailability::Unavailable, std::string{"gone"}};
  domain::Track track{domain::TrackId{3}, domain::TrackKind::Video};
  track.insert_clip(domain::Clip{domain::ClipId{9}, domain::MediaId{7},
                                 TimeRange{MediaTime::zero(), MediaTime{4, Rational{1, 1}}},
                                 MediaTime::zero()});
  application::ProjectAllocatorState allocator;
  allocator.next_media_id = 8;
  allocator.next_clip_id = 10;
  allocator.next_timeline_id = 6;
  allocator.next_track_id = 4;
  auto state = make_restore_state(domain::TimelineId{5}, domain::TrackId{3}, {asset}, track, allocator);

  int worker_calls = 0;
  EditorSession loaded(std::move(state), [&worker_calls](const std::filesystem::path&, int) {
    ++worker_calls;
    return std::unique_ptr<application::PlaybackMediaWorker>{};
  });
  assert(loaded.document().find_asset(domain::MediaId{7}).has_value());
  assert(loaded.document().find_asset(domain::MediaId{7})->source_locator() ==
         std::filesystem::path{"media/offline.mp4"});
  const auto* clip = loaded.document().timeline().find_clip(domain::ClipId{9});
  assert(clip != nullptr);
  assert(clip->id() == domain::ClipId{9});
  assert(clip->media_id() == domain::MediaId{7});
  assert((clip->source_range().start() == MediaTime::zero()));
  assert((clip->source_range().end() == MediaTime{4, Rational{1, 1}}));
  assert((clip->timeline_start() == MediaTime::zero()));
  const auto restored_asset = loaded.document().find_asset(domain::MediaId{7});
  assert(restored_asset.has_value());
  assert(restored_asset->availability() == application::MediaAvailability::Unavailable);
  assert(restored_asset->availability_reason() == std::optional<std::string>{"gone"});
  assert((clip->timeline_range().start() == MediaTime::zero()));
  assert((loaded.document().timeline().duration() == MediaTime{4, Rational{1, 1}}));
  assert(loaded.timeline_views().size() == 1);
  assert(loaded.timeline_views().front().missing);
  assert(worker_calls == 0);
  const auto presentation = loaded.update_playback();
  assert(std::holds_alternative<application::TimelineMissingMediaPresentation>(presentation));
  assert(worker_calls == 0);
  assert(!loaded.document().is_dirty());
  assert(!loaded.can_undo());
  assert(!loaded.can_redo());
}

void failed_candidate_preserves_active_session() {
  Probe probe;
  auto active = std::make_unique<EditorSession>();
  const auto media = active->processor().import_asset("keep.mp4", probe);
  const auto clip = active->processor().add_clip(
      media, TimeRange{MediaTime::zero(), MediaTime{4, Rational{1, 1}}}, MediaTime::zero());
  active->synchronize_document_state();
  active->select_clip(clip);
  // One more edit undone immediately: redo becomes non-empty while the
  // document stays dirty from earlier unsaved work.
  active->processor().move_clip(clip, active->document().video_track_id(),
                                MediaTime{1, Rational{1, 1}});
  assert(active->undo());
  assert(active->processor().undo_size() > 0);
  assert(active->processor().redo_size() > 0);

  const auto editorial_before = application::ProjectCodec::encode(active->document().snapshot());
  const auto history_before = active->document().history_state_id();
  const auto saved_before = active->document().saved_history_state_id();
  const auto generation_before = active->document().document_generation();
  const auto selection_before = active->selected_clip();
  const auto undo_before = active->processor().undo_size();
  const auto redo_before = active->processor().redo_size();
  assert(active->document().is_dirty());
  assert(selection_before == clip);

  application::VideoAssetMetadata metadata{"h264", 1920, 1080, Rational{1, 90000}, std::nullopt,
                                           std::nullopt};
  application::MediaAsset first{domain::MediaId{7}, "a.mp4", 0, MediaTime{10, Rational{1, 1}},
                               metadata};
  application::MediaAsset second{domain::MediaId{7}, "b.mp4", 0, MediaTime{10, Rational{1, 1}},
                                metadata};
  auto bad = make_restore_state(domain::TimelineId{5}, domain::TrackId{3}, {first, second},
                                domain::Track{domain::TrackId{3}, domain::TrackKind::Video},
                                application::ProjectAllocatorState{});
  bool rejected = false;
  try {
    auto candidate = std::make_unique<EditorSession>(
        std::move(bad), [](const std::filesystem::path&, int) {
          return std::unique_ptr<application::PlaybackMediaWorker>{};
        });
    active = std::move(candidate);
  } catch (const std::exception&) {
    rejected = true;
  }
  assert(rejected);
  assert(application::ProjectCodec::encode(active->document().snapshot()) == editorial_before);
  assert(active->document().timeline().find_clip(clip) != nullptr);
  assert(active->document().history_state_id() == history_before);
  assert(active->document().saved_history_state_id() == saved_before);
  assert(active->document().document_generation() == generation_before);
  assert(active->document().is_dirty());
  assert(active->selected_clip() == selection_before);
  assert(active->processor().undo_size() == undo_before);
  assert(active->processor().redo_size() == redo_before);
  assert(active->can_undo());
}

void runtime_media_path_resolution() {
  using pulsar::desktop::resolve_runtime_media_path;
  assert(resolve_runtime_media_path("/tmp/project/session.pulsar", "media/clip.mp4") ==
         std::filesystem::path{"/tmp/project/media/clip.mp4"});
  assert(resolve_runtime_media_path("/tmp/project/session.pulsar", "/abs/clip.mp4") ==
         std::filesystem::path{"/abs/clip.mp4"});
  assert(resolve_runtime_media_path("session.pulsar", "media/clip.mp4") ==
         std::filesystem::path{"media/clip.mp4"});
}

// ---- QA-001: multi-video ingest --------------------------------------------

class SecondsProbe final : public MediaProbe {
public:
  explicit SecondsProbe(std::int64_t seconds, bool video = true) : seconds_{seconds}, video_{video} {}
  MediaProbeResult probe(const std::filesystem::path& path) const override {
    MediaProbeResult result;
    result.path = path;
    const MediaTime duration{seconds_, Rational{1, 1}};
    if (video_) {
      result.video_streams.push_back(VideoStreamInfo{0, "h264", 1280, 720, Rational{1, 90000},
                                                     duration, std::nullopt});
    }
    result.audio_streams.push_back(application::AudioStreamInfo{1, "aac", 48'000, 2,
                                                                 Rational{1, 48'000}, duration});
    return result;
  }

private:
  std::int64_t seconds_;
  bool video_;
};

MediaTime secs(std::int64_t value) { return MediaTime{value, Rational{1, 1}}; }

std::vector<std::pair<std::int64_t, std::int64_t>> video_layout(const EditorSession& session) {
  std::vector<std::pair<std::int64_t, std::int64_t>> layout;
  const auto* track = session.document().timeline().find_track(session.document().video_track_id());
  for (const auto& clip : track->clips()) {
    layout.emplace_back(clip.timeline_range().start().seconds().numerator() /
                            clip.timeline_range().start().seconds().denominator(),
                        clip.timeline_range().end().seconds().numerator() /
                            clip.timeline_range().end().seconds().denominator());
  }
  return layout;
}
using Layout = std::vector<std::pair<std::int64_t, std::int64_t>>;

std::size_t video_clip_count(const EditorSession& session) {
  return session.document().timeline().find_track(session.document().video_track_id())->clips().size();
}

// A: second video goes into a gap; B: third video appends; the audio track
// being longer than VIDEO 1 must not move the append point.
void ingest_inserts_into_gap_and_appends_after_video_end() {
  EditorSession session;
  const auto a = session.processor().import_asset("a.mp4", SecondsProbe{10});
  (void)session.processor().add_clip(a, TimeRange{MediaTime::zero(), secs(10)}, MediaTime::zero());
  session.synchronize_document_state();
  const auto b = session.processor().import_asset("b.mp4", SecondsProbe{3});
  const auto c = session.processor().import_asset("c.mp4", SecondsProbe{2});

  const auto inserted = session.insert_video_asset(b, secs(12));
  assert((video_layout(session) == Layout{{0, 10}, {12, 15}}));
  const auto* clip_b = session.document().timeline().find_clip(inserted);
  assert(clip_b != nullptr && clip_b->media_id() == b);
  assert(clip_b->source_range().start() == MediaTime::zero());
  assert(clip_b->source_range().end() == secs(3));
  assert(session.selected_clip() == std::optional<domain::ClipId>{inserted});

  // Long independent audio must not influence "append".
  const auto audio_track = session.processor().add_track(domain::TrackKind::Audio);
  const auto music = session.processor().import_asset("music.wav", SecondsProbe{60, false});
  (void)session.processor().add_clip(audio_track, music, TimeRange{MediaTime::zero(), secs(60)}, MediaTime::zero());
  session.synchronize_document_state();
  assert(session.timeline_duration() == secs(60));
  assert(session.video_track_end() == secs(15));
  (void)session.append_video_asset(c);
  assert((video_layout(session) == Layout{{0, 10}, {12, 15}, {15, 17}}));
}

void ingest_into_empty_track_starts_at_zero() {
  EditorSession session;
  const auto a = session.processor().import_asset("a.mp4", SecondsProbe{4});
  assert(session.video_track_end() == MediaTime::zero());
  (void)session.append_video_asset(a);
  assert((video_layout(session) == Layout{{0, 4}}));
}

// C: overlap is a clean refusal: no clip, no history entry, no generation or
// dirty change.
void ingest_overlap_is_refused_without_mutation() {
  EditorSession session;
  const auto a = session.processor().import_asset("a.mp4", SecondsProbe{10});
  (void)session.processor().add_clip(a, TimeRange{MediaTime::zero(), secs(10)}, MediaTime::zero());
  session.synchronize_document_state();
  const auto b = session.processor().import_asset("b.mp4", SecondsProbe{3});

  const auto undo_before = session.processor().undo_size();
  const auto redo_before = session.processor().redo_size();
  const auto history_before = session.document().history_state_id();
  const auto generation_before = session.document().document_generation();
  const auto dirty_before = session.is_dirty();
  const auto layout_before = video_layout(session);

  for (const auto start : {secs(5), secs(0), secs(8), MediaTime{9'999, Rational{1, 1000}}}) {
    bool refused = false;
    try {
      (void)session.insert_video_asset(b, start);
    } catch (const desktop::VideoIngestException& error) {
      refused = error.code() == desktop::VideoIngestError::NoSpace;
    }
    assert(refused);
  }
  // Touching at the half-open boundary is fine (start == end of A).
  assert(video_layout(session) == layout_before);
  assert(session.processor().undo_size() == undo_before);
  assert(session.processor().redo_size() == redo_before);
  assert(session.document().history_state_id() == history_before);
  assert(session.document().document_generation() == generation_before);
  assert(session.is_dirty() == dirty_before);
  (void)session.insert_video_asset(b, secs(10));
  assert((video_layout(session) == Layout{{0, 10}, {10, 13}}));
}

// A clip that would fit before an existing one but spills over it is refused too.
void ingest_refuses_overlap_with_following_clip() {
  EditorSession session;
  const auto a = session.processor().import_asset("a.mp4", SecondsProbe{4});
  (void)session.processor().add_clip(a, TimeRange{MediaTime::zero(), secs(4)}, secs(10));
  session.synchronize_document_state();
  const auto b = session.processor().import_asset("b.mp4", SecondsProbe{3});
  bool refused = false;
  try { (void)session.insert_video_asset(b, secs(8)); }
  catch (const desktop::VideoIngestException& e) { refused = e.code() == desktop::VideoIngestError::NoSpace; }
  assert(refused);
  (void)session.insert_video_asset(b, secs(7));  // [7,10) touches the next clip
  assert((video_layout(session) == Layout{{7, 10}, {10, 14}}));
}

// D + E: insertion and append are single undoable edits; the asset outlives
// the clip.
void ingest_participates_in_undo_redo_and_dirty_state() {
  for (const bool append : {false, true}) {
    EditorSession session;
    const auto a = session.processor().import_asset("a.mp4", SecondsProbe{10});
    (void)session.processor().add_clip(a, TimeRange{MediaTime::zero(), secs(10)}, MediaTime::zero());
    session.synchronize_document_state();
    const auto b = session.processor().import_asset("b.mp4", SecondsProbe{3});
    const auto undo_before = session.processor().undo_size();
    const auto layout_before = video_layout(session);

    (void)(append ? session.append_video_asset(b) : session.insert_video_asset(b, secs(12)));
    const auto layout_after = video_layout(session);
    assert(layout_after.size() == 2);
    assert(session.processor().undo_size() == undo_before + 1);
    assert(session.is_dirty());

    assert(session.undo());
    assert(video_layout(session) == layout_before);
    assert(session.document().find_asset(b).has_value());  // asset stays in the library
    assert(session.timeline_views().size() == 1);

    assert(session.redo());
    assert(video_layout(session) == layout_after);
    assert(session.timeline_views().size() == 2);
  }
}

// Saving after ingest marks the document clean and a new ingest dirties it.
void ingest_dirty_follows_saved_identity() {
  EditorSession session;
  const auto a = session.processor().import_asset("a.mp4", SecondsProbe{4});
  (void)session.append_video_asset(a);
  session.document().mark_current_state_saved();
  assert(!session.is_dirty());
  const auto b = session.processor().import_asset("b.mp4", SecondsProbe{4});
  session.document().mark_current_state_saved();
  assert(!session.is_dirty());
  (void)session.append_video_asset(b);
  assert(session.is_dirty());
  assert(session.undo());
  assert(!session.is_dirty());
}

// F: audio-only media can never land on VIDEO 1 (session API, command API and
// persisted state).
void audio_only_asset_is_rejected_for_video_track() {
  EditorSession session;
  const auto music = session.processor().import_asset("music.wav", SecondsProbe{5, false});
  const auto undo_before = session.processor().undo_size();
  bool refused = false;
  try { (void)session.append_video_asset(music); }
  catch (const desktop::VideoIngestException& e) { refused = e.code() == desktop::VideoIngestError::NotVideoAsset; }
  assert(refused);
  assert(video_clip_count(session) == 0);
  assert(session.processor().undo_size() == undo_before);

  bool core_refused = false;
  try {
    (void)session.processor().add_clip(session.document().video_track_id(), music,
                                       TimeRange{MediaTime::zero(), secs(5)}, MediaTime::zero());
  } catch (const std::invalid_argument&) { core_refused = true; }
  assert(core_refused);
  assert(video_clip_count(session) == 0);

  // A malformed project placing audio-only media on VIDEO 1 is refused on load.
  application::MediaAsset audio_asset = application::MediaAsset::audio_only(
      domain::MediaId{1}, "music.wav", secs(5),
      application::AudioAssetMetadata{0, 48'000, 2, Rational{1, 48'000}, secs(5)});
  domain::Track track{domain::TrackId{3}, domain::TrackKind::Video};
  track.insert_clip(domain::Clip{domain::ClipId{1}, domain::MediaId{1},
                                 TimeRange{MediaTime::zero(), secs(5)}, MediaTime::zero()});
  application::ProjectAllocatorState allocator;
  allocator.next_media_id = 2; allocator.next_clip_id = 2; allocator.next_timeline_id = 6; allocator.next_track_id = 4;
  auto state = make_restore_state(domain::TimelineId{5}, domain::TrackId{3}, {audio_asset}, track, allocator);
  bool load_refused = false;
  try { EditorSession loaded(std::move(state), [](const std::filesystem::path&, int) { return std::unique_ptr<application::PlaybackMediaWorker>{}; }); }
  catch (const std::invalid_argument&) { load_refused = true; }
  assert(load_refused);
}

// G (session part): the first video is auto-placed by the desktop; asking to
// insert the same asset at the same spot afterwards must not duplicate it.
void ingest_does_not_duplicate_an_already_placed_first_video() {
  EditorSession session;
  const auto a = session.processor().import_asset("a.mp4", SecondsProbe{6});
  (void)session.processor().add_clip(a, TimeRange{MediaTime::zero(), secs(6)}, MediaTime::zero());
  session.synchronize_document_state();
  bool refused = false;
  try { (void)session.insert_video_asset_at_playhead(a); }
  catch (const desktop::VideoIngestException& e) { refused = e.code() == desktop::VideoIngestError::NoSpace; }
  assert(refused);
  assert(video_clip_count(session) == 1);
}

// H: A, gap, B, C(append) survive save -> reload with identical layout, ranges
// and assets.
void multi_video_project_roundtrips() {
  EditorSession session;
  const auto a = session.processor().import_asset("a.mp4", SecondsProbe{10});
  (void)session.processor().add_clip(a, TimeRange{MediaTime::zero(), secs(10)}, MediaTime::zero());
  session.synchronize_document_state();
  const auto b = session.processor().import_asset("b.mp4", SecondsProbe{3});
  const auto c = session.processor().import_asset("c.mp4", SecondsProbe{2});
  (void)session.insert_video_asset(b, secs(12));
  (void)session.append_video_asset(c);
  const auto expected = Layout{{0, 10}, {12, 15}, {15, 17}};
  assert(video_layout(session) == expected);

  const auto bytes = application::ProjectCodec::encode(session.document().snapshot());
  EditorSession loaded(application::ProjectCodec::decode(bytes),
                       [](const std::filesystem::path&, int) { return std::unique_ptr<application::PlaybackMediaWorker>{}; });
  assert(video_layout(loaded) == expected);
  assert(loaded.document().asset_list().size() == 3);
  const auto* track = loaded.document().timeline().find_track(loaded.document().video_track_id());
  assert(track->clips().size() == 3);
  for (const auto& clip : track->clips()) {
    assert(clip.source_range().start() == MediaTime::zero());
    const auto asset = loaded.document().find_asset(clip.media_id());
    assert(asset.has_value() && clip.source_range().end() == asset->playable_duration());
  }
  assert(application::ProjectCodec::encode(loaded.document().snapshot()) == bytes);
  // The reloaded session keeps ingesting without id collisions.
  const auto d = loaded.processor().import_asset("d.mp4", SecondsProbe{1});
  (void)loaded.append_video_asset(d);
  assert(video_clip_count(loaded) == 4);
}

// I: an asset whose media is unavailable is refused clearly.
void ingest_refuses_unavailable_assets() {
  application::VideoAssetMetadata metadata{"h264", 1920, 1080, Rational{1, 90000}, std::nullopt, std::nullopt};
  application::MediaAsset offline{domain::MediaId{7}, "media/offline.mp4", 0, secs(10), metadata,
                                  application::MediaAvailability::Unavailable, std::string{"gone"}};
  domain::Track track{domain::TrackId{3}, domain::TrackKind::Video};
  application::ProjectAllocatorState allocator;
  allocator.next_media_id = 8; allocator.next_clip_id = 10; allocator.next_timeline_id = 6; allocator.next_track_id = 4;
  auto state = make_restore_state(domain::TimelineId{5}, domain::TrackId{3}, {offline}, track, allocator);
  EditorSession loaded(std::move(state), [](const std::filesystem::path&, int) { return std::unique_ptr<application::PlaybackMediaWorker>{}; });
  const auto generation = loaded.document().document_generation();
  bool refused = false;
  try { (void)loaded.append_video_asset(domain::MediaId{7}); }
  catch (const desktop::VideoIngestException& e) { refused = e.code() == desktop::VideoIngestError::UnavailableAsset; }
  assert(refused);
  bool unknown = false;
  try { (void)loaded.append_video_asset(domain::MediaId{99}); }
  catch (const desktop::VideoIngestException& e) { unknown = e.code() == desktop::VideoIngestError::UnknownAsset; }
  assert(unknown);
  assert(video_clip_count(loaded) == 0);
  assert(loaded.document().document_generation() == generation);
}

// The read model's asset revision moves only when the asset set changes.
void asset_revision_tracks_asset_set_only() {
  EditorSession session;
  const auto r0 = session.document().asset_revision();
  const auto a = session.processor().import_asset("a.mp4", SecondsProbe{4});
  const auto r1 = session.document().asset_revision();
  assert(r1 != r0);
  (void)session.append_video_asset(a);
  assert(session.document().asset_revision() == r1);  // clip edits do not touch assets
  assert(session.undo());                              // undo the clip
  assert(session.document().asset_revision() == r1);
  assert(session.undo());                              // undo the import
  assert(session.document().asset_revision() != r1);
  assert(session.document().asset_list().empty());
}

// ---- Beta editorization: snapping + exact append ---------------------------

MediaTime ms(std::int64_t value) { return MediaTime{value, Rational{1, 1000}}; }

struct SnapFixture {
  EditorSession session;
  domain::ClipId a{1}, b{1}, c{1};
  SnapFixture() {
    const auto ma = session.processor().import_asset("a.mp4", SecondsProbe{10});
    const auto mb = session.processor().import_asset("b.mp4", SecondsProbe{3});
    const auto mc = session.processor().import_asset("c.mp4", SecondsProbe{4});
    a = session.processor().add_clip(ma, TimeRange{MediaTime::zero(), secs(10)}, MediaTime::zero());
    b = session.processor().add_clip(mb, TimeRange{MediaTime::zero(), secs(3)}, secs(20));
    c = session.processor().add_clip(mc, TimeRange{MediaTime::zero(), secs(4)}, secs(30));
    session.synchronize_document_state();
  }
  [[nodiscard]] MediaTime start_of(domain::ClipId id) const {
    return session.document().timeline().find_clip(id)->timeline_start();
  }
};

// A drag that ends within tolerance of a neighbour's edge lands exactly on it.
void snap_aligns_start_to_previous_clip_end_exactly() {
  SnapFixture f;
  // B at 20 dragged left by 9.96 s -> desired start 10.04 s, 40 ms from A's end.
  const auto resolution = f.session.resolve_move(f.b, ms(0), ms(-9'960), ms(100));
  assert(resolution.snapped && resolution.new_start == secs(10) && resolution.guide == secs(10));
  f.session.move_clip(f.b, ms(0), ms(-9'960), ms(100));
  assert(f.start_of(f.b) == secs(10));  // exactly A's end: no micro-gap
  // Undo restores the original place.
  assert(f.session.undo());
  assert(f.start_of(f.b) == secs(20));
}

// The clip's own END snaps too (to the next clip's start).
void snap_aligns_end_to_next_clip_start() {
  SnapFixture f;
  // B (3 s) at 20; moving +6.95 s puts its end at 29.95 s, 50 ms from C's start (30 s).
  const auto resolution = f.session.resolve_move(f.b, ms(0), ms(6'950), ms(100));
  assert(resolution.snapped && resolution.new_start == secs(27) && resolution.guide == secs(30));
}

void snap_ignores_edges_outside_tolerance_and_zero_tolerance_disables_it() {
  SnapFixture f;
  const auto outside = f.session.resolve_move(f.b, ms(0), ms(-9'800), ms(100));  // 200 ms away
  assert(!outside.snapped && outside.new_start == ms(10'200));
  const auto off = f.session.resolve_move(f.b, ms(0), ms(-9'960), MediaTime::zero());
  assert(!off.snapped && off.new_start == ms(10'040));
}

// A snap target that would overlap a clip on the same track is discarded;
// nothing else is ever moved.
void snap_never_creates_overlap_or_ripple() {
  SnapFixture f;
  f.session.seek(secs(5));  // playhead inside A: snapping B's start to 5 s would overlap A
  const auto resolution = f.session.resolve_move(f.b, ms(0), ms(-14'990), ms(100));
  assert(resolution.new_start == secs(10));  // confined flush after A, never at the playhead (5 s)
  assert(!(resolution.snapped && resolution.guide == secs(5)));
  assert(f.start_of(f.a) == MediaTime::zero());
  assert(f.start_of(f.c) == secs(30));
  // The same drag with the playhead in free space does snap to it.
  f.session.seek(secs(14));
  const auto free_space = f.session.resolve_move(f.b, ms(0), ms(-5'990), ms(100));
  assert(free_space.snapped && free_space.new_start == secs(14));
}

// The tolerance is defined in pixels: converted with the current viewport, the
// same on-screen distance snaps at any zoom, and a farther one never does.
void snap_tolerance_is_constant_in_pixels_across_zoom() {
  SnapFixture f;
  for (const auto view_ms : {std::int64_t{40'000}, std::int64_t{4'000}, std::int64_t{400}}) {
    TimelineViewport viewport{ms(0), ms(view_ms)};
    const double width = 1000.0;
    const auto tolerance = viewport.time_at(8.0, width).value() - viewport.time_at(0.0, width).value();
    const auto micro = [](std::int64_t value) { return MediaTime{value, Rational{1, 1'000'000}}; };
    const std::int64_t per_pixel_us = view_ms;  // view_ms over 1000 px, in microseconds
    // 5 px short of A's end snaps; 20 px short does not.
    const auto drag_near = micro(-10'000'000 + per_pixel_us * 5);
    const auto drag_far = micro(-10'000'000 + per_pixel_us * 20);
    assert(f.session.resolve_move(f.b, ms(0), drag_near, tolerance).snapped);
    assert(!f.session.resolve_move(f.b, ms(0), drag_far, tolerance).snapped);
  }
}

void trim_snaps_to_neighbour_edges() {
  SnapFixture f;
  // Trim C's start from 30 s toward B's end (23 s): target 23.04 s snaps to 23 s... clipped by source,
  // so check the pure resolution: the edge a few ms away is returned exactly.
  const auto resolution = f.session.resolve_trim_edge(f.c, ms(23'040), ms(100));
  assert(resolution.snapped && resolution.time == secs(23));
  assert(!f.session.resolve_trim_edge(f.c, ms(23'500), ms(100)).snapped);
  // End trim snaps to the playhead.
  f.session.seek(ms(32'500));
  const auto end = f.session.resolve_trim_edge(f.c, ms(32'470), ms(100));
  assert(end.snapped && end.time == ms(32'500));
}

// Dragging into a neighbour stops flush against it (exact), never overlaps and
// never jumps over it; neighbours stay where they are.
void move_stops_flush_at_neighbours() {
  SnapFixture f;
  // B (3 s) at 20 dragged far left: stops at A's end (10 s).
  const auto left = f.session.resolve_move(f.b, ms(0), ms(-18'000), MediaTime::zero());
  assert(left.new_start == secs(10));
  // Dragged far right: stops with its end at C's start (30 s) -> start 27 s.
  const auto right = f.session.resolve_move(f.b, ms(0), ms(+18'000), MediaTime::zero());
  assert(right.new_start == secs(27));
  // Committing the clamped move works (no error) and is exact.
  f.session.move_clip(f.b, ms(0), ms(-18'000), ms(100));
  assert(f.start_of(f.b) == secs(10));
  assert(f.start_of(f.a) == MediaTime::zero() && f.start_of(f.c) == secs(30));
  assert(f.session.undo());
  assert(f.start_of(f.b) == secs(20));
}

// Dragging beyond zero clamps at zero instead of failing.
void move_clamps_at_zero() {
  SnapFixture f;
  const auto resolution = f.session.resolve_move(f.a, ms(0), ms(-50'000), MediaTime::zero());
  assert(resolution.new_start == MediaTime::zero());
}

// "Al final" is exact for non-trivial rational durations: no micro-gap, ever.
void append_is_gapless_for_rational_durations() {
  EditorSession session;
  const MediaTime d1{1001, Rational{1, 250}};          // 4.004 s
  const MediaTime d2{10'957'333, Rational{1, 1'000'000}};  // 10.957333 s
  const MediaTime d3{7, Rational{30'000, 1001}};      // 7 frames at 29.97 fps
  class RationalProbe final : public MediaProbe {
  public:
    explicit RationalProbe(MediaTime duration) : duration_{duration} {}
    MediaProbeResult probe(const std::filesystem::path& path) const override {
      MediaProbeResult result;
      result.path = path;
      result.video_streams.push_back(VideoStreamInfo{0, "h264", 1280, 720, Rational{1, 90000}, duration_, std::nullopt});
      return result;
    }
  private:
    MediaTime duration_;
  };
  const auto m1 = session.processor().import_asset("1.mp4", RationalProbe{d1});
  const auto m2 = session.processor().import_asset("2.mp4", RationalProbe{d2});
  const auto m3 = session.processor().import_asset("3.mp4", RationalProbe{d3});
  const auto c1 = session.append_video_asset(m1);
  const auto c2 = session.append_video_asset(m2);
  const auto c3 = session.append_video_asset(m3);
  const auto& timeline = session.document().timeline();
  assert(timeline.find_clip(c1)->timeline_range().end() == timeline.find_clip(c2)->timeline_start());
  assert(timeline.find_clip(c2)->timeline_range().end() == timeline.find_clip(c3)->timeline_start());
  assert(timeline.find_clip(c1)->timeline_start() == MediaTime::zero());
  assert(session.video_track_end() == timeline.find_clip(c3)->timeline_range().end());
}

// ---- Separar audio ----------------------------------------------------------

class CustomAudioProbe final : public MediaProbe {
public:
  CustomAudioProbe(std::int64_t video_s, std::optional<std::int64_t> audio_s) : video_{video_s}, audio_{audio_s} {}
  MediaProbeResult probe(const std::filesystem::path& path) const override {
    MediaProbeResult result;
    result.path = path;
    result.video_streams.push_back(VideoStreamInfo{0, "h264", 1280, 720, Rational{1, 90000}, secs(video_), std::nullopt});
    if (audio_.has_value()) {
      result.audio_streams.push_back(application::AudioStreamInfo{1, "aac", 48'000, 2, Rational{1, 48'000}, secs(*audio_)});
    }
    return result;
  }
private:
  std::int64_t video_;
  std::optional<std::int64_t> audio_;
};

std::size_t audio_track_count(const EditorSession& session) {
  std::size_t count = 0;
  for (const auto& track : session.document().timeline().tracks()) {
    if (track.kind() == domain::TrackKind::Audio) ++count;
  }
  return count;
}
const domain::Track* first_audio_track(const EditorSession& session) {
  for (const auto& track : session.document().timeline().tracks()) {
    if (track.kind() == domain::TrackKind::Audio) return &track;
  }
  return nullptr;
}

// Same range, embedded audio muted, one undo/redo step, exact restore.
void separate_audio_creates_synced_clip_as_one_undo_step() {
  EditorSession session;
  const auto media = session.processor().import_asset("a.mp4", SecondsProbe{10});
  // A trimmed clip placed late: the audio must follow the *clip*, not the asset.
  const auto video = session.processor().add_clip(media, TimeRange{secs(2), secs(9)}, secs(5));
  session.processor().set_clip_audio_gain(video, -6.0);
  session.synchronize_document_state();
  const auto undo_before = session.processor().undo_size();
  const auto history_before = session.document().history_state_id();

  const auto audio = session.separate_audio(video);
  const auto* video_clip = session.document().timeline().find_clip(video);
  const auto* audio_clip = session.document().timeline().find_clip(audio);
  assert(video_clip != nullptr && audio_clip != nullptr);
  assert(audio_clip->media_id() == media);
  assert(audio_clip->timeline_start() == video_clip->timeline_start());
  assert(audio_clip->timeline_range().start() == video_clip->timeline_range().start());
  assert(audio_clip->timeline_range().end() == video_clip->timeline_range().end());
  assert(audio_clip->source_range().start() == video_clip->source_range().start());
  assert(audio_clip->source_range().end() == video_clip->source_range().end());
  assert(video_clip->muted());                 // embedded audio silenced
  assert(!audio_clip->muted());                // the separated copy is audible
  assert(audio_clip->audio_gain_db() == -6.0); // gain moved with the sound
  assert(audio_track_count(session) == 1);
  assert(session.selected_clip() == std::optional<domain::ClipId>{audio});
  assert(session.processor().undo_size() == undo_before + 4);  // add_track, add_clip, gain, mute ...
  assert(!session.can_separate_audio(video));  // already separated / silenced

  // ONE undo restores everything (clip, track, mute), bit for bit.
  assert(session.undo());
  assert(session.document().timeline().find_clip(audio) == nullptr);
  assert(!session.document().timeline().find_clip(video)->muted());
  assert(audio_track_count(session) == 0);
  assert(session.document().history_state_id() == history_before);
  assert(session.timeline_views().size() == 1);  // only the video clip remains
  assert(session.document().timeline().find_clip(video)->audio_gain_db() == -6.0);
  assert(session.processor().undo_size() == undo_before);
  // ONE redo brings it all back.
  assert(session.redo());
  assert(session.document().timeline().find_clip(audio) != nullptr);
  assert(session.document().timeline().find_clip(video)->muted());
  assert(audio_track_count(session) == 1);
}

// It survives save -> reload (the audio is an ordinary independent clip).
void separate_audio_persists() {
  EditorSession session;
  const auto media = session.processor().import_asset("a.mp4", SecondsProbe{10});
  const auto video = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(10)}, MediaTime::zero());
  session.synchronize_document_state();
  const auto audio = session.separate_audio(video);
  const auto bytes = application::ProjectCodec::encode(session.document().snapshot());
  EditorSession loaded(application::ProjectCodec::decode(bytes),
                       [](const std::filesystem::path&, int) { return std::unique_ptr<application::PlaybackMediaWorker>{}; });
  const auto* a = loaded.document().timeline().find_clip(audio);
  const auto* v = loaded.document().timeline().find_clip(video);
  assert(a != nullptr && v != nullptr && v->muted() && !a->muted());
  assert(a->source_range().end() == v->source_range().end() && a->timeline_start() == v->timeline_start());
  assert(application::ProjectCodec::encode(loaded.document().snapshot()) == bytes);
}

// Reuses a free audio track; otherwise adds one. Never overlaps existing audio.
void separate_audio_reuses_or_adds_audio_tracks() {
  EditorSession session;
  const auto m1 = session.processor().import_asset("a.mp4", SecondsProbe{4});
  const auto m2 = session.processor().import_asset("b.mp4", SecondsProbe{4});
  const auto v1 = session.processor().add_clip(m1, TimeRange{MediaTime::zero(), secs(4)}, MediaTime::zero());
  const auto v2 = session.processor().add_clip(m2, TimeRange{MediaTime::zero(), secs(4)}, secs(4));
  session.synchronize_document_state();
  (void)session.separate_audio(v1);
  (void)session.separate_audio(v2);          // [4,8) is free on the same audio track
  assert(audio_track_count(session) == 1);
  assert(first_audio_track(session)->clips().size() == 2);
  // Music already occupying [8,12) of the first track forces a new track for a clip at [8,12).
  const auto music = session.processor().import_asset("m.wav", SecondsProbe{60, false});
  (void)session.processor().add_clip(first_audio_track(session)->id(), music, TimeRange{MediaTime::zero(), secs(4)}, secs(8));
  const auto m3 = session.processor().import_asset("c.mp4", SecondsProbe{4});
  const auto v3 = session.processor().add_clip(m3, TimeRange{MediaTime::zero(), secs(4)}, secs(8));
  session.synchronize_document_state();
  (void)session.separate_audio(v3);
  assert(audio_track_count(session) == 2);
}

// Auxiliary visual tracks are silent by design, but their source audio can be
// detached into an ordinary audible track without changing timing or format.
void separate_audio_from_video_2_is_synced_undoable_and_persistent() {
  EditorSession session;
  const auto media = session.processor().import_asset("overlay.mp4", SecondsProbe{20});
  const auto video_2 = session.add_visual_track();
  const auto source = TimeRange{secs(3), secs(11)};
  const auto start = secs(5);
  const auto video = session.processor().add_clip(video_2, media, source, start);
  session.processor().set_clip_audio_gain(video, -4.0);
  session.processor().set_clip_fades(video, secs(1), secs(2));
  session.synchronize_document_state();
  assert(session.can_separate_audio(video));

  const auto audio = session.separate_audio(video);
  const auto* video_clip = session.document().timeline().find_clip(video);
  const auto* audio_clip = session.document().timeline().find_clip(audio);
  assert(video_clip != nullptr && audio_clip != nullptr && video_clip->muted());
  assert(audio_clip->timeline_start() == start && same_range(audio_clip->source_range(), source));
  assert(same_range(audio_clip->timeline_range(), video_clip->timeline_range()));
  assert(audio_clip->audio_gain_db() == -4.0);
  assert(audio_clip->fade_in() == secs(1) && audio_clip->fade_out() == secs(2));
  const auto audio_view = std::find_if(session.timeline_views().begin(), session.timeline_views().end(),
      [&](const auto& view) { return view.id == audio; });
  assert(audio_view != session.timeline_views().end() && audio_view->has_audio);

  const auto contributors = application::resolve_audio_contributors(
      session.document().snapshot(), video_clip->timeline_range());
  assert(contributors.contributors.size() == 1);
  assert(contributors.contributors.front().clip_id == audio);
  assert(contributors.contributors.front().stream_index == 1);

  assert(session.undo());
  assert(session.document().timeline().find_clip(audio) == nullptr);
  assert(!session.document().timeline().find_clip(video)->muted());
  assert(session.redo());
  assert(session.document().timeline().find_clip(audio) != nullptr);
  assert(session.document().timeline().find_clip(video)->muted());

  const auto bytes = application::ProjectCodec::encode(session.document().snapshot());
  assert(bytes.rfind("PULSAR_PROJECT 6\n", 0) == 0);
  EditorSession loaded(application::ProjectCodec::decode(bytes),
      [](const std::filesystem::path&, int) {
        return std::unique_ptr<application::PlaybackMediaWorker>{};
      });
  const auto* loaded_video = loaded.document().timeline().find_clip(video);
  const auto* loaded_audio = loaded.document().timeline().find_clip(audio);
  assert(loaded_video != nullptr && loaded_audio != nullptr && loaded_video->muted());
  assert(loaded_audio->timeline_start() == start && same_range(loaded_audio->source_range(), source));
}

void separate_audio_refusals_leave_no_trace() {
  EditorSession session;
  const auto with_audio = session.processor().import_asset("a.mp4", SecondsProbe{10});
  const auto silent = session.processor().import_asset("silent.mp4", CustomAudioProbe{10, std::nullopt});
  const auto short_audio = session.processor().import_asset("short.mp4", CustomAudioProbe{10, 8});
  const auto music = session.processor().import_asset("m.wav", SecondsProbe{10, false});
  const auto v_audio = session.processor().add_clip(with_audio, TimeRange{MediaTime::zero(), secs(10)}, MediaTime::zero());
  const auto v_silent = session.processor().add_clip(silent, TimeRange{MediaTime::zero(), secs(10)}, secs(10));
  const auto v_short = session.processor().add_clip(short_audio, TimeRange{MediaTime::zero(), secs(10)}, secs(20));
  const auto track = session.processor().add_track(domain::TrackKind::Audio);
  const auto a_clip = session.processor().add_clip(track, music, TimeRange{MediaTime::zero(), secs(10)}, MediaTime::zero());
  session.processor().set_clip_muted(v_audio, true);
  session.synchronize_document_state();

  const auto undo_before = session.processor().undo_size();
  const auto generation = session.document().document_generation();
  const auto expect = [&](domain::ClipId clip, desktop::SeparateAudioError code) {
    assert(!session.can_separate_audio(clip));
    bool refused = false;
    try { (void)session.separate_audio(clip); }
    catch (const desktop::SeparateAudioException& e) { refused = e.code() == code; }
    assert(refused);
  };
  expect(v_audio, desktop::SeparateAudioError::AlreadySilenced);
  expect(v_silent, desktop::SeparateAudioError::NoEmbeddedAudio);
  expect(v_short, desktop::SeparateAudioError::AudioShorterThanClip);
  expect(a_clip, desktop::SeparateAudioError::NotAVideoTrackClip);
  expect(domain::ClipId{999}, desktop::SeparateAudioError::UnknownClip);
  assert(session.processor().undo_size() == undo_before);
  assert(session.document().document_generation() == generation);
}

// Grouped edits: a failure half way leaves nothing behind; history trimming
// never keeps half a group.
void grouped_edit_rolls_back_and_trims_whole_groups() {
  EditorSession session;
  const auto media = session.processor().import_asset("a.mp4", SecondsProbe{10});
  const auto undo_before = session.processor().undo_size();
  bool threw = false;
  try {
    session.processor().run_as_one_edit([&] {
      (void)session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(5)}, MediaTime::zero());
      (void)session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(5)}, secs(1));  // overlaps -> throws
    });
  } catch (const std::exception&) { threw = true; }
  assert(threw);
  assert(session.processor().undo_size() == undo_before && session.processor().redo_size() == 0);
  assert(video_clip_count(session) == 0);  // the first add_clip of the group was rolled back

  // Capacity 3: a 2-command group followed by 2 single edits must not leave half the group.
  application::ProjectDocument document;
  application::CommandProcessor small{document, 3};
  const auto m = small.import_asset("a.mp4", SecondsProbe{10});
  small.run_as_one_edit([&] {
    (void)small.add_clip(m, TimeRange{MediaTime::zero(), secs(1)}, MediaTime::zero());
    (void)small.add_clip(m, TimeRange{MediaTime::zero(), secs(1)}, secs(2));
  });
  (void)small.add_clip(m, TimeRange{MediaTime::zero(), secs(1)}, secs(4));
  (void)small.add_clip(m, TimeRange{MediaTime::zero(), secs(1)}, secs(6));
  // Oldest entries (import + the whole group) were dropped; remaining history is two single edits.
  assert(small.undo_size() <= 3);
  while (small.undo()) {}
  assert(document.timeline().find_track(document.video_track_id())->clips().size() <= 2);
}

void wheel_zoom_keeps_the_anchor_time_under_the_cursor() {
  using desktop::ZoomView;
  const double total = 40.0;
  // Time under anchor a for a view: start + a * duration.
  const auto time_under = [&](ZoomView view, double anchor) {
    const double duration = total / view.zoom;
    return (total - duration) * view.scroll + anchor * duration;
  };
  for (const double anchor : {0.0, 0.25, 0.5, 0.9, 1.0}) {
    ZoomView view{1.0, 0.0};
    for (int step = 0; step < 12; ++step) {
      const double before = time_under(view, anchor);
      view = desktop::anchored_zoom(total, view, 1.5, anchor);
      assert(view.zoom > 1.0 && view.scroll >= 0.0 && view.scroll <= 1.0);
      assert(std::abs(time_under(view, anchor) - before) < 1e-9);
    }
    // Zooming back out never leaves the valid range and ends in the fitted view.
    for (int step = 0; step < 40; ++step) view = desktop::anchored_zoom(total, view, 1.0 / 1.5, anchor);
    assert(view.zoom == desktop::kMinTimelineZoom && view.scroll == 0.0);
  }
  // Clamped at the maximum; invalid input leaves the view untouched.
  ZoomView deep{desktop::kMaxTimelineZoom, 0.5};
  assert(desktop::anchored_zoom(total, deep, 100.0, 0.5).zoom == desktop::kMaxTimelineZoom);
  const auto same = desktop::anchored_zoom(0.0, ZoomView{2.0, 0.3}, 2.0, 0.5);
  assert(same.zoom == 2.0 && same.scroll == 0.3);
  assert(desktop::anchored_zoom(total, ZoomView{2.0, 0.3}, std::nan(""), 0.5).zoom == 2.0);
}

// ---- Beta editing power: ripple delete, trim to playhead, selection order --------

std::string layout_signature(const EditorSession& session) {
  std::string out;
  std::size_t track_index = 0;
  for (const auto& track : session.document().timeline().tracks()) {
    for (const auto& clip : track.clips()) {
      const auto start = clip.timeline_range().start().seconds();
      const auto end = clip.timeline_range().end().seconds();
      out += std::to_string(track_index) + ":" + std::to_string(clip.id().value()) + "@" +
             std::to_string(start.numerator()) + "/" + std::to_string(start.denominator()) + "-" +
             std::to_string(end.numerator()) + "/" + std::to_string(end.denominator()) + ";";
    }
    ++track_index;
  }
  return out;
}

MediaTime third(std::int64_t thirds) { return MediaTime{thirds, Rational{1, 3}}; }

domain::MediaTime start_of(const EditorSession& session, domain::ClipId id) {
  return session.document().timeline().find_clip(id)->timeline_start();
}

void ripple_delete_closes_the_gap_across_all_tracks_and_keeps_sync() {
  EditorSession session;
  const auto media = session.processor().import_asset("v.mp4", SecondsProbe{30});
  const auto music_media = session.processor().import_asset("m.wav", SecondsProbe{60, false});
  // Video 1: A 0..10, B 10..(10 + 7/3), C right after B (exact rational boundaries), D with a gap.
  const auto a = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(10)}, MediaTime::zero());
  const auto b_start = secs(10);
  const auto b = session.processor().add_clip(media, TimeRange{third(1), third(8)}, b_start);
  const auto b_end = session.document().timeline().find_clip(b)->timeline_range().end();
  const auto c = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(4)}, b_end);
  const auto d = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(2)}, b_end + secs(4) + secs(5));
  const auto audio1 = session.processor().add_track(domain::TrackKind::Audio);
  const auto audio2 = session.processor().add_track(domain::TrackKind::Audio);
  const auto audio3 = session.processor().add_track(domain::TrackKind::Audio);
  // Music straddles the cut and must stay; the effect inside the cut starts before t1 and stays.
  const auto music = session.processor().add_clip(audio1, music_media, TimeRange{MediaTime::zero(), secs(60)}, MediaTime::zero());
  const auto inside = session.processor().add_clip(audio3, media, TimeRange{MediaTime::zero(), secs(1)}, secs(11));
  // C's audio detached by the user earlier: same start as C, must keep that relationship.
  const auto c_audio = session.processor().add_clip(audio2, media, TimeRange{MediaTime::zero(), secs(4)}, b_end);
  session.synchronize_document_state();
  const auto before = layout_signature(session);
  const auto history_before = session.document().history_state_id();
  const auto undo_before = session.processor().undo_size();
  const auto duration = b_end - b_start;  // 7/3 s

  session.select_clip(b);
  assert(session.can_ripple_delete());
  session.ripple_delete_selected();
  assert(session.document().timeline().find_clip(b) == nullptr);
  assert(start_of(session, a) == MediaTime::zero());
  assert(start_of(session, c) == b_start);                       // exactly closes the gap
  assert(start_of(session, d) == b_end + secs(9) - duration);
  assert(start_of(session, c_audio) == start_of(session, c));    // sync preserved
  assert(start_of(session, music) == MediaTime::zero());         // straddling clip stays
  assert(start_of(session, inside) == secs(11));                 // starts before t1: stays
  assert(!session.selected_clip().has_value());
  const auto after = layout_signature(session);
  assert(after != before);
  assert(session.processor().undo_size() == undo_before + 1 + 3);  // remove + C, C audio, D

  assert(session.undo());                                         // ONE step restores everything
  assert(layout_signature(session) == before);
  assert(session.document().history_state_id() == history_before);
  assert(session.processor().undo_size() == undo_before);
  assert(session.redo());
  assert(layout_signature(session) == after);
  assert(!session.redo());
}

void ripple_delete_edges_first_last_and_lone_clip() {
  EditorSession session;
  const auto media = session.processor().import_asset("v.mp4", SecondsProbe{10});
  const auto a = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(4)}, MediaTime::zero());
  const auto b = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(4)}, secs(4));
  const auto c = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(4)}, secs(8));
  session.synchronize_document_state();
  session.select_clip(a);
  session.ripple_delete_selected();                              // first: everything moves back by 4
  assert((video_layout(session) == Layout{{0, 4}, {4, 8}}));
  assert(start_of(session, b) == MediaTime::zero() && start_of(session, c) == secs(4));
  session.select_clip(c);
  const auto moves_before = session.processor().undo_size();
  session.ripple_delete_selected();                              // last: only the removal
  assert((video_layout(session) == Layout{{0, 4}}));
  assert(session.processor().undo_size() == moves_before + 1);
  session.select_clip(b);
  session.ripple_delete_selected();                              // lone clip
  assert(video_clip_count(session) == 0);
  assert(session.undo() && video_clip_count(session) == 1);
}

void ripple_delete_refuses_overlap_without_trace() {
  EditorSession session;
  const auto media = session.processor().import_asset("v.mp4", SecondsProbe{60});
  const auto b = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(3)}, secs(10));
  const auto audio = session.processor().add_track(domain::TrackKind::Audio);
  // Music ends at 20 and stays (starts before t1 = 13); the next audio clip at 21 would land on 18.
  (void)session.processor().add_clip(audio, media, TimeRange{MediaTime::zero(), secs(20)}, MediaTime::zero());
  (void)session.processor().add_clip(audio, media, TimeRange{MediaTime::zero(), secs(2)}, secs(21));
  session.synchronize_document_state();
  session.select_clip(b);
  const auto before = layout_signature(session);
  const auto history = session.document().history_state_id();
  const auto undo_before = session.processor().undo_size();
  assert(!session.can_ripple_delete());
  bool refused = false;
  try { session.ripple_delete_selected(); }
  catch (const desktop::RippleDeleteException& error) { refused = error.code() == desktop::RippleDeleteError::WouldOverlap; }
  assert(refused);
  assert(layout_signature(session) == before);
  assert(session.document().history_state_id() == history && session.processor().undo_size() == undo_before);
  assert(session.selected_clip() == std::optional<domain::ClipId>{b});
  // Plain delete still works there, and without a selection ripple is refused.
  session.delete_selected();
  assert(video_clip_count(session) == 0);
  bool nothing = false;
  try { session.ripple_delete_selected(); }
  catch (const desktop::RippleDeleteException& error) { nothing = error.code() == desktop::RippleDeleteError::NothingSelected; }
  assert(nothing);
}

// A ripple that moves more clips than the history capacity is still ONE whole step.
void ripple_delete_beyond_history_capacity_stays_one_step() {
  EditorSession session;
  constexpr int count = 300;
  const auto media = session.processor().import_asset("v.mp4", SecondsProbe{1});
  std::vector<domain::ClipId> ids;
  for (int i = 0; i < count; ++i) {
    ids.push_back(session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(1)}, secs(i)));
  }
  session.synchronize_document_state();
  const auto before = layout_signature(session);
  session.select_clip(ids.front());
  session.ripple_delete_selected();
  const auto after = layout_signature(session);
  assert(video_clip_count(session) == count - 1);
  assert(start_of(session, ids[1]) == MediaTime::zero() && start_of(session, ids.back()) == secs(count - 2));
  assert(session.processor().undo_size() >= static_cast<std::size_t>(count));  // whole group kept
  assert(session.undo());
  assert(layout_signature(session) == before);                    // not "half a ripple"
  assert(session.redo());
  assert(layout_signature(session) == after);

  // A later edit may evict the whole ripple from history (never half of it).
  session.select_clip(ids[1]);
  session.set_clip_muted(ids[1], true);
  assert(session.processor().undo_size() <= 256);
  while (session.undo()) {}
  assert(layout_signature(session) == after);                      // either fully undone or untouched
}

// A grouped edit that fails half way leaves nothing behind, even when it is bigger than the
// capacity, and does not cost the older history.
void failed_oversize_group_rolls_back_completely() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document, 8};
  const auto media = processor.import_asset("v.mp4", SecondsProbe{1});
  std::vector<domain::ClipId> ids;
  for (int i = 0; i < 5; ++i) ids.push_back(processor.add_clip(media, TimeRange{MediaTime::zero(), secs(1)}, secs(i * 2)));
  const auto undo_before = processor.undo_size();
  const auto track = document.video_track_id();
  const auto start_before = document.timeline().find_clip(ids[4])->timeline_start();
  bool threw = false;
  try {
    processor.run_as_one_edit([&] {
      for (int round = 0; round < 12; ++round) {                    // 12 commands > capacity 8
        processor.move_clip(ids[4], track, secs(20 + round));
      }
      processor.move_clip(ids[0], track, secs(2));                  // collides with ids[1]: throws
    });
  } catch (const std::exception&) { threw = true; }
  assert(threw);
  assert(document.timeline().find_clip(ids[4])->timeline_start() == start_before);
  assert(processor.undo_size() == undo_before && processor.redo_size() == 0);
}

void trim_to_playhead_q_and_w() {
  EditorSession session;
  const auto media = session.processor().import_asset("v.mp4", SecondsProbe{20});
  const auto clip = session.processor().add_clip(media, TimeRange{secs(2), secs(12)}, secs(5));  // timeline 5..15
  session.synchronize_document_state();
  assert(!session.trim_selected_to_playhead(true));                 // no selection
  session.select_clip(clip);
  session.seek(secs(5));
  assert(!session.can_trim_to_playhead() && !session.trim_selected_to_playhead(true));   // on the start edge
  session.seek(secs(15));
  assert(!session.trim_selected_to_playhead(false));                // on the end edge
  session.seek(secs(2));
  assert(!session.trim_selected_to_playhead(true));                 // before the clip
  session.seek(secs(20));
  assert(!session.trim_selected_to_playhead(false));                // after the clip
  assert(start_of(session, clip) == secs(5));

  session.seek(secs(8));
  const auto undo_before = session.processor().undo_size();
  assert(session.trim_selected_to_playhead(true));                   // Q
  const auto* trimmed = session.document().timeline().find_clip(clip);
  assert(trimmed->timeline_range().start() == secs(8) && trimmed->timeline_range().end() == secs(15));
  assert(trimmed->source_range().start() == secs(5) && trimmed->source_range().end() == secs(12));
  session.seek(secs(12));
  assert(session.trim_selected_to_playhead(false));                  // W
  trimmed = session.document().timeline().find_clip(clip);
  assert(trimmed->timeline_range().start() == secs(8) && trimmed->timeline_range().end() == secs(12));
  assert(session.processor().undo_size() == undo_before + 2);
  assert(session.undo() && session.undo());
  assert(session.document().timeline().find_clip(clip)->timeline_range().start() == secs(5));
  assert(session.document().timeline().find_clip(clip)->timeline_range().end() == secs(15));
  assert(session.redo() && session.redo());

  // An audio-only clip trims the same way.
  const auto audio_track = session.processor().add_track(domain::TrackKind::Audio);
  const auto audio = session.processor().add_clip(audio_track, media, TimeRange{MediaTime::zero(), secs(6)}, secs(1));
  session.synchronize_document_state();
  session.select_clip(audio);
  session.seek(secs(3));
  assert(session.trim_selected_to_playhead(true));
  assert(start_of(session, audio) == secs(3));
}

void select_adjacent_clip_follows_editorial_order() {
  EditorSession session;
  assert(!session.select_adjacent_clip(true) && !session.select_adjacent_clip(false));  // empty timeline
  const auto media = session.processor().import_asset("v.mp4", SecondsProbe{30});
  const auto v1 = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(4)}, MediaTime::zero());
  const auto v2 = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(4)}, secs(6));
  const auto t1 = session.processor().add_track(domain::TrackKind::Audio);
  const auto t2 = session.processor().add_track(domain::TrackKind::Audio);
  const auto a2 = session.processor().add_clip(t2, media, TimeRange{MediaTime::zero(), secs(2)}, MediaTime::zero());  // ties with v1 at 0
  const auto a1 = session.processor().add_clip(t1, media, TimeRange{MediaTime::zero(), secs(2)}, secs(3));
  session.synchronize_document_state();
  // Order: start 0 -> video track before audio tracks (v1, a2); then 3 (a1); then 6 (v2).
  assert(session.select_adjacent_clip(true) && session.selected_clip() == std::optional<domain::ClipId>{v1});  // no selection: first
  assert(session.select_adjacent_clip(true) && session.selected_clip() == std::optional<domain::ClipId>{a2});
  assert(session.select_adjacent_clip(true) && session.selected_clip() == std::optional<domain::ClipId>{a1});
  assert(session.select_adjacent_clip(true) && session.selected_clip() == std::optional<domain::ClipId>{v2});
  assert(!session.select_adjacent_clip(true) && session.selected_clip() == std::optional<domain::ClipId>{v2});  // stays at the end
  assert(session.select_adjacent_clip(false) && session.selected_clip() == std::optional<domain::ClipId>{a1});
  session.select_clip(std::nullopt);
  assert(session.select_adjacent_clip(false) && session.selected_clip() == std::optional<domain::ClipId>{v2});  // no selection: last
  session.select_clip(v1);
  assert(session.select_adjacent_clip(false) == false && session.selected_clip() == std::optional<domain::ClipId>{v1});
  assert(session.processor().undo_size() > 0);   // selection never enters history
}

void titles_in_the_session_and_ripple() {
  EditorSession session;
  const auto media = session.processor().import_asset("v.mp4", SecondsProbe{60});
  const auto a = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(10)}, MediaTime::zero());
  const auto b = session.processor().add_clip(media, TimeRange{MediaTime::zero(), secs(10)}, secs(10));
  (void)a;
  session.synchronize_document_state();
  // No title where there is no video; at the playhead otherwise, shortened to the free room.
  EditorSession empty;
  bool no_video = false;
  try { (void)empty.add_title_at_playhead(); } catch (const desktop::TitleException& e) { no_video = e.code() == desktop::TitleError::NoVideo; }
  assert(no_video);
  session.seek(secs(2));
  const auto first = session.add_title_at_playhead();
  assert(session.titles().size() == 1 && session.titles()[0].start() == secs(2) && session.titles()[0].end() == secs(5));
  assert(session.selected_title() == std::optional<domain::TitleId>{first} && !session.selected_clip().has_value());
  session.seek(secs(3));
  bool no_space = false;
  try { (void)session.add_title_at_playhead(); } catch (const desktop::TitleException& e) { no_space = e.code() == desktop::TitleError::NoSpace; }
  assert(no_space);
  session.seek(secs(18));
  const auto second = session.add_title_at_playhead();                  // clamped to the end of the video (20 s)
  assert(session.document().find_title(second)->end() == secs(20));
  // Selecting a clip deselects the title and vice versa; undo of an add drops the selection.
  session.select_clip(b);
  assert(!session.selected_title().has_value());
  session.select_title(second);
  assert(session.selected_title() == std::optional<domain::TitleId>{second} && !session.selected_clip().has_value());
  session.replace_title(session.document().find_title(second)->with_text("Cierre"));
  assert(session.document().find_title(second)->text() == "Cierre");
  assert(session.undo() && session.document().find_title(second)->text() == "Título");
  assert(session.undo() && session.document().find_title(second) == nullptr && !session.selected_title().has_value());
  assert(session.redo() && session.redo());

  // Ripple: deleting clip a (0..10) moves the title that starts after it, keeps earlier ones.
  const auto a_id = session.document().timeline().find_track(session.document().video_track_id())->clips().front().id();
  session.select_clip(a_id);
  const auto before = layout_signature(session);
  session.ripple_delete_selected();
  assert(session.document().find_title(first)->start() == secs(2));          // started before the cut end: stays
  assert(session.document().find_title(second)->start() == secs(8) && session.document().find_title(second)->end() == secs(10));
  assert(session.undo());
  assert(session.document().find_title(second)->start() == secs(18) && layout_signature(session) == before);
  assert(session.redo() && session.document().find_title(second)->start() == secs(8));
  assert(session.undo());

  // Refused when a shifted title would land on one that stays: nothing changes.
  session.select_title(first);
  session.replace_title(session.document().find_title(first)->with_range(TimeRange{secs(9), secs(15)}));
  session.select_clip(a_id);
  assert(!session.can_ripple_delete());
}

void duplicate_and_fades_in_the_session() {
  EditorSession session;
  const auto media = session.processor().import_asset("v.mp4", SecondsProbe{20});
  const auto a = session.processor().add_clip(media, TimeRange{secs(2), secs(6)}, MediaTime::zero());   // 4 s on 0..4
  session.processor().set_clip_audio_gain(a, -3.0);
  session.synchronize_document_state();
  session.select_clip(a);

  // ---- Fades: one undo step, exact values, refused where there is no sound -------------------
  const auto half = MediaTime{500, Rational{1, 1000}};
  const auto undo_before = session.processor().undo_size();
  session.set_clip_fades(a, half, secs(1));
  assert(session.processor().undo_size() == undo_before + 1);
  assert(session.document().timeline().find_clip(a)->fade_in() == half && session.document().timeline().find_clip(a)->fade_out() == secs(1));
  session.set_clip_fades(a, half, secs(1));                                           // unchanged: no history entry
  assert(session.processor().undo_size() == undo_before + 1);
  assert(session.undo() && session.document().timeline().find_clip(a)->fade_in() == MediaTime::zero());
  assert(session.redo() && session.document().timeline().find_clip(a)->fade_out() == secs(1));
  bool refused = false;
  try { session.set_clip_fades(a, secs(61), MediaTime::zero()); } catch (const desktop::FadeException& e) { refused = e.code() == desktop::FadeError::TooLong; }
  assert(refused);

  // ---- They survive save/reopen (version 4 only when there are fades) ------------------------
  const auto bytes = application::ProjectCodec::encode(session.document().snapshot());
  assert(bytes.rfind("PULSAR_PROJECT 4\n", 0) == 0 && bytes.find("CLIP_FADE") != std::string::npos);
  const auto restored = application::ProjectDocument::restore(application::ProjectCodec::decode(bytes));
  assert(restored.timeline().find_clip(a)->fade_in() == half && restored.timeline().find_clip(a)->fade_out() == secs(1));
  assert(restored.timeline().find_clip(a)->audio_gain_db() == -3.0);
  assert(application::ProjectCodec::encode(restored.snapshot()) == bytes);

  // ---- Split: the left half keeps the fade-in, the right half the fade-out --------------------
  session.seek(secs(2));
  const auto right = session.split_selected_at_playhead();
  assert(session.document().timeline().find_clip(a)->fade_in() == half && session.document().timeline().find_clip(a)->fade_out() == MediaTime::zero());
  assert(session.document().timeline().find_clip(right)->fade_in() == MediaTime::zero() && session.document().timeline().find_clip(right)->fade_out() == secs(1));
  assert(session.undo());

  // ---- Duplicate: right after the original, same settings, one undo step ---------------------
  session.select_clip(a);
  const auto before = layout_signature(session);
  const auto undo_mark = session.processor().undo_size();
  session.duplicate_selected();
  const auto copy = *session.selected_clip();
  assert(copy != a);
  const auto* original_clip = session.document().timeline().find_clip(a);
  const auto* copied_clip = session.document().timeline().find_clip(copy);
  assert(copied_clip->timeline_start() == original_clip->timeline_range().end());
  assert(copied_clip->source_range().start() == original_clip->source_range().start() && copied_clip->source_range().end() == original_clip->source_range().end());
  assert(copied_clip->audio_gain_db() == -3.0 && copied_clip->fade_in() == half && copied_clip->fade_out() == secs(1) && !copied_clip->muted());
  assert(session.processor().undo_size() > undo_mark);
  assert(session.undo() && session.document().timeline().find_clip(copy) == nullptr && layout_signature(session) == before);   // ONE step
  assert(session.redo() && session.document().timeline().find_clip(copy) != nullptr);

  // No room: the original is followed by the copy; duplicating the first again must refuse.
  session.select_clip(a);
  bool no_room = false;
  try { session.duplicate_selected(); } catch (const desktop::DuplicateException& e) { no_room = e.code() == desktop::DuplicateError::NoSpace; }
  assert(no_room && session.document().timeline().find_clip(a) != nullptr);

  // Nothing selected.
  session.select_clip(std::nullopt);
  bool nothing = false;
  try { session.duplicate_selected(); } catch (const desktop::DuplicateException& e) { nothing = e.code() == desktop::DuplicateError::NothingSelected; }
  assert(nothing);

  // Titles duplicate too, and refuse when the next title is in the way.
  session.seek(secs(1));
  const auto title = session.add_title_at_playhead();
  session.select_title(title);
  session.duplicate_selected();
  assert(session.titles().size() == 2 && session.titles()[1].start() == session.titles()[0].end() && session.titles()[1].text() == session.titles()[0].text());
  session.select_title(session.titles()[0].id());
  bool title_blocked = false;
  try { session.duplicate_selected(); } catch (const desktop::DuplicateException& e) { title_blocked = e.code() == desktop::DuplicateError::NoSpace; }
  assert(title_blocked && session.titles().size() == 2);

  // A photo has no sound to fade.
  EditorSession stills;
  class StillProbe final : public MediaProbe {
  public:
    MediaProbeResult probe(const std::filesystem::path& path) const override {
      MediaProbeResult result;
      result.path = path;
      result.container_name = "png_pipe";
      result.video_streams.push_back(VideoStreamInfo{0, "png", 640, 360, Rational{1, 25}, secs(3600), domain::FrameRate{30, 1}});
      return result;
    }
  };
  const auto photo = stills.processor().import_asset("p.png", StillProbe{});
  const auto photo_clip = stills.insert_video_asset(photo, MediaTime::zero());
  assert(stills.document().timeline().find_clip(photo_clip)->duration() == secs(5));
  bool no_audio = false;
  try { stills.set_clip_fades(photo_clip, half, MediaTime::zero()); } catch (const desktop::FadeException& e) { no_audio = e.code() == desktop::FadeError::NoAudio; }
  assert(no_audio);
  stills.select_clip(photo_clip);
  stills.duplicate_selected();                                                       // a photo duplicates like any clip
  assert(stills.timeline_duration() == secs(10));
}

void viewer_mapping_ignores_letterbox_offsets_and_resize() {
  const auto small = desktop::viewer_delta_to_canvas(120.0, -45.0, 600.0, 300.0);
  const auto large = desktop::viewer_delta_to_canvas(240.0, -90.0, 1200.0, 600.0);
  assert(std::abs(small.first - 0.2) < 1e-12);
  assert(std::abs(small.second + 0.15) < 1e-12);
  assert(small == large);
  bool refused = false;
  try { (void)desktop::viewer_delta_to_canvas(1.0, 1.0, 0.0, 100.0); }
  catch (const std::invalid_argument&) { refused = true; }
  assert(refused);
}

int main() {
  {
    EditorSession session;
    const auto media = session.processor().import_asset("visual.mp4", Probe{});
    const auto upper = session.add_visual_track();
    const auto clip = session.insert_video_asset(media, MediaTime::zero(), upper);
    assert(session.document().timeline().find_track(session.document().video_track_id())->clips().empty());
    assert(session.timeline_views().size() == 1 && session.timeline_views()[0].track_id == upper);
    assert(session.track_views().front().id == upper);
    const auto primary = session.document().video_track_id();
    session.move_clip_to_visual_track(clip, primary);
    assert(session.timeline_views()[0].track_id == primary);
    assert(session.undo() && session.timeline_views()[0].track_id == upper);
    session.move_clip_to_visual_track(clip, primary);
    session.remove_empty_visual_track(upper);
    assert(session.track_views().size() == 1);
    assert(session.undo() && session.track_views().front().id == upper);
  }
  viewport_is_exactly_guarded();
  zoom_and_pan_preserve_editorial_time_mapping();
  editing_actions_use_document_commands();
  live_audio_property_edit_preserves_playback_position_and_intent();
  session_read_model_and_selection();
  final_pointer_edit_regressions();
  synchronization_diagnostics_semantics();
  duration_ring_wraparound();
  save_project_marks_captured_history_state();
  save_project_uncertain_durability_still_commits();
  save_project_failure_marks_nothing_saved();
  save_project_marks_captured_snapshot_not_current_state();
  editor_session_dirty_mirrors_saved_identity();
  restored_session_matches_persisted_editorial_state();
  missing_media_survives_restore_without_workers();
  failed_candidate_preserves_active_session();
  runtime_media_path_resolution();
  ingest_inserts_into_gap_and_appends_after_video_end();
  ingest_into_empty_track_starts_at_zero();
  ingest_overlap_is_refused_without_mutation();
  ingest_refuses_overlap_with_following_clip();
  ingest_participates_in_undo_redo_and_dirty_state();
  ingest_dirty_follows_saved_identity();
  audio_only_asset_is_rejected_for_video_track();
  ingest_does_not_duplicate_an_already_placed_first_video();
  multi_video_project_roundtrips();
  ingest_refuses_unavailable_assets();
  asset_revision_tracks_asset_set_only();
  snap_aligns_start_to_previous_clip_end_exactly();
  snap_aligns_end_to_next_clip_start();
  snap_ignores_edges_outside_tolerance_and_zero_tolerance_disables_it();
  snap_never_creates_overlap_or_ripple();
  snap_tolerance_is_constant_in_pixels_across_zoom();
  trim_snaps_to_neighbour_edges();
  move_clamps_at_zero();
  move_stops_flush_at_neighbours();
  append_is_gapless_for_rational_durations();
  separate_audio_creates_synced_clip_as_one_undo_step();
  separate_audio_persists();
  separate_audio_reuses_or_adds_audio_tracks();
  separate_audio_from_video_2_is_synced_undoable_and_persistent();
  separate_audio_refusals_leave_no_trace();
  grouped_edit_rolls_back_and_trims_whole_groups();
  wheel_zoom_keeps_the_anchor_time_under_the_cursor();
  ripple_delete_closes_the_gap_across_all_tracks_and_keeps_sync();
  ripple_delete_edges_first_last_and_lone_clip();
  ripple_delete_refuses_overlap_without_trace();
  ripple_delete_beyond_history_capacity_stays_one_step();
  failed_oversize_group_rolls_back_completely();
  trim_to_playhead_q_and_w();
  select_adjacent_clip_follows_editorial_order();
  titles_in_the_session_and_ripple();
  duplicate_and_fades_in_the_session();
  viewer_mapping_ignores_letterbox_offsets_and_resize();
  return 0;
}
