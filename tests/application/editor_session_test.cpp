#include "editor_session.hpp"

#include <pulsar/application/media/media_probe.hpp>

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
  session.processor().move_clip(clip, session.document().video_track_id(), MediaTime{2, Rational{1, 1}});
  session.rebuild_read_model();
  assert(session.timeline_views().front().selected);
  session.processor().remove_clip(clip);
  session.rebuild_read_model();
  assert(!session.selected_clip().has_value());
}

} // namespace

int main() {
  viewport_is_exactly_guarded();
  session_read_model_and_selection();
}
