#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <cassert>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
using namespace pulsar;
namespace exporting = pulsar::application::exporting;
using exporting::ExportPlan;
using exporting::ExportPlanError;
using exporting::ExportPlanErrorCode;

class Probe final : public application::MediaProbe {
public:
  explicit Probe(application::MediaProbeResult result) : result_{std::move(result)} {}
  [[nodiscard]] application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    auto result = result_;
    result.path = path;
    return result;
  }

private:
  application::MediaProbeResult result_;
};

application::MediaProbeResult video_probe(domain::FrameRate rate,
                                          domain::MediaTime duration = domain::MediaTime{10, {1, 1}},
                                          int width = 1920, int height = 1080) {
  application::MediaProbeResult result;
  result.video_streams.push_back(application::VideoStreamInfo{
      0, "h264", width, height, domain::Rational{1, 1000}, duration, rate});
  return result;
}

application::MediaProbeResult audio_probe(domain::MediaTime duration) {
  application::MediaProbeResult result;
  result.audio_streams.push_back(application::AudioStreamInfo{
      0, "pcm_s16le", 48'000, 2, domain::Rational{1, 48'000}, duration});
  return result;
}

application::ProjectSnapshot project_with_video(domain::FrameRate rate,
                                                domain::MediaTime duration,
                                                int width = 1920, int height = 1080) {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto media = processor.import_asset("video.mp4", Probe{video_probe(rate, duration, width, height)});
  (void)processor.add_clip(media, domain::TimeRange{domain::MediaTime::zero(), duration},
                           domain::MediaTime::zero());
  return document.snapshot();
}

void expect_error(const application::ProjectSnapshot& snapshot, ExportPlanErrorCode code) {
  try {
    (void)ExportPlan::build(snapshot);
    assert(false);
  } catch (const ExportPlanError& error) {
    assert(error.code() == code);
  }
}

void frame_and_sample_helpers_are_exact() {
  for (const domain::FrameRate rate : {domain::FrameRate{24, 1}, domain::FrameRate{25, 1},
                                       domain::FrameRate{30, 1}, domain::FrameRate{30000, 1001},
                                       domain::FrameRate{60000, 1001}}) {
    const auto frame_time = exporting::output_frame_time(domain::FrameNumber{17}, rate);
    assert(exporting::output_frame_index_at(frame_time, rate, domain::RoundingMode::Floor) == 17);
    assert(exporting::output_frame_count(frame_time, rate) == 17);
  }

  const auto partial = domain::MediaTime{1, domain::Rational{2, 100}};
  assert(exporting::output_frame_count(partial, domain::FrameRate{30, 1}) == 1);
  assert(exporting::audio_sample_index_at(partial, domain::RoundingMode::Floor) == 960);
  assert(exporting::audio_sample_index_at(partial, domain::RoundingMode::Ceil) == 960);
  assert((exporting::audio_sample_time(48'000) == domain::MediaTime{1, {1, 1}}));
}

void plan_freezes_cfr_duration_geometry_and_snapshot() {
  const auto snapshot = project_with_video(domain::FrameRate{30000, 1001},
                                           domain::MediaTime{1001, {1, 30000}}, 1919, 1081);
  const auto first = ExportPlan::build(snapshot);
  const auto second = ExportPlan::build(snapshot);
  assert(first.snapshot().document_generation == second.snapshot().document_generation);
  assert(first.master_video_asset_id() == second.master_video_asset_id());
  assert((first.frame_rate().rational() == domain::Rational{30000, 1001}));
  assert(first.editorial_duration() == snapshot.timeline.duration());
  assert(first.frame_count() == 1);
  assert(first.output_duration() == exporting::output_frame_time(domain::FrameNumber{1}, first.frame_rate()));
  assert(first.output_width() == 1920);
  assert(first.output_height() == 1082);
  assert(first.audio_sample_count() == exporting::audio_sample_count(first.output_duration()));
  assert(first.profile().container() == exporting::ExportContainer::Mp4);
  assert(first.profile().video_gap_policy() == exporting::ExportVideoGapPolicy::Black);
  assert(first.profile().audio_gap_policy() == exporting::ExportAudioGapPolicy::Silence);
}

void empty_audio_only_and_invalid_projects_fail() {
  application::ProjectDocument empty;
  expect_error(empty.snapshot(), ExportPlanErrorCode::EmptyTimeline);

  application::ProjectDocument audio_only;
  application::CommandProcessor audio_processor{audio_only};
  const auto audio = audio_processor.import_asset("audio.wav", Probe{audio_probe({3, {1, 1}})});
  const auto audio_track = audio_processor.add_track(domain::TrackKind::Audio);
  (void)audio_processor.add_clip(audio_track, audio, domain::TimeRange{{0, {1, 1}}, {3, {1, 1}}},
                                 domain::MediaTime::zero());
  expect_error(audio_only.snapshot(), ExportPlanErrorCode::NoVideoMaster);

  auto missing = project_with_video(domain::FrameRate{30, 1}, {1, {1, 1}});
  missing.assets.front() = application::MediaAsset{
      missing.assets.front().id(), "missing.mp4", 0, {1, {1, 1}},
      application::VideoAssetMetadata{"h264", 1920, 1080, {1, 1000}, domain::FrameRate{30, 1},
                                      std::string{"mp4"}},
      application::MediaAvailability::Unavailable, std::string{"missing"}};
  expect_error(missing, ExportPlanErrorCode::MissingMedia);
}

void audio_longer_than_video_extends_black_video_and_audio_grid() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto video = processor.import_asset("video.mp4", Probe{video_probe({24, 1}, {1, {1, 1}})});
  const auto audio = processor.import_asset("audio.wav", Probe{audio_probe({3, {1, 1}})});
  const auto audio_track = processor.add_track(domain::TrackKind::Audio);
  (void)processor.add_clip(video, domain::TimeRange{{0, {1, 1}}, {1, {1, 1}}}, {0, {1, 1}});
  (void)processor.add_clip(audio_track, audio, domain::TimeRange{{0, {1, 1}}, {3, {1, 1}}},
                           {0, {1, 1}});
  const auto plan = ExportPlan::build(document.snapshot());
  assert((plan.editorial_duration() == domain::MediaTime{3, {1, 1}}));
  assert(plan.frame_count() == 72);
  assert((plan.output_duration() == domain::MediaTime{3, {1, 1}}));
  assert(plan.audio_sample_count() == 144'000);
}

void overflow_and_invalid_dimensions_are_typed() {
  const auto huge = project_with_video(domain::FrameRate{60'000, 1001},
                                       domain::MediaTime{std::numeric_limits<std::int64_t>::max(), {1, 1}});
  expect_error(huge, ExportPlanErrorCode::Overflow);

  const auto odd = project_with_video(domain::FrameRate{30, 1}, {1, {1, 1}}, 1, 3);
  const auto plan = ExportPlan::build(odd);
  assert(plan.output_width() == 2);
  assert(plan.output_height() == 4);
}

} // namespace

int main() {
  frame_and_sample_helpers_are_exact();
  plan_freezes_cfr_duration_geometry_and_snapshot();
  empty_audio_only_and_invalid_projects_fail();
  audio_longer_than_video_extends_black_video_and_audio_grid();
  overflow_and_invalid_dimensions_are_typed();
}
