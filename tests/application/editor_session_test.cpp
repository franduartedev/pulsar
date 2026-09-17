#include "editor_session.hpp"

#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/playback/playback_worker.hpp>

#include <cassert>
#include <cmath>
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
  assert(worker_creations == 1);
  const auto generation_before = playback_session.document().document_generation();
  playback_session.select_clip(playback_clip);
  playback_session.select_clip(std::nullopt);
  playback_session.select_clip(playback_clip);
  assert(worker_creations == 1);
  assert(playback_session.document().document_generation() == generation_before);
}

} // namespace

int main() {
  viewport_is_exactly_guarded();
  session_read_model_and_selection();
}
