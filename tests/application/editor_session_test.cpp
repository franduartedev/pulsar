#include "duration_sample_ring.hpp"
#include "editor_session.hpp"

#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/playback/playback_worker.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <limits>

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

int main() {
  viewport_is_exactly_guarded();
  editing_actions_use_document_commands();
  session_read_model_and_selection();
  final_pointer_edit_regressions();
  synchronization_diagnostics_semantics();
  duration_ring_wraparound();
}
