#include "duration_sample_ring.hpp"
#include "editor_session.hpp"
#include "project_path_resolution.hpp"

#include <pulsar/application/media/media_probe.hpp>
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

int main() {
  viewport_is_exactly_guarded();
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
  return 0;
}
