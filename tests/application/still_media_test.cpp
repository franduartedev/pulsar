// Still media (photographs, colour cards): asset rules, canvas/master choice, project round trip.
#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/media/still_media.hpp>
#include <pulsar/application/project/project_codec.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

namespace {
using namespace pulsar;
using domain::MediaTime;
using domain::Rational;
using domain::TimeRange;

MediaTime secs(std::int64_t value) { return MediaTime{value, Rational{1, 1}}; }

class Probe final : public application::MediaProbe {
public:
  Probe(int width, int height, std::int64_t seconds, std::optional<std::string> container, int fps = 30)
      : width_{width}, height_{height}, seconds_{seconds}, container_{std::move(container)}, fps_{fps} {}
  [[nodiscard]] application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    application::MediaProbeResult result;
    result.path = path;
    result.container_name = container_;
    result.video_streams.push_back(application::VideoStreamInfo{0, "png", width_, height_, Rational{1, 25},
                                                                 secs(seconds_), domain::FrameRate{fps_, 1}});
    return result;
  }

private:
  int width_, height_;
  std::int64_t seconds_;
  std::optional<std::string> container_;
  int fps_;
};

bool throws_invalid(const auto& body) {
  try { body(); } catch (const std::invalid_argument&) { return true; } catch (...) { return false; }
  return false;
}

void locator_helpers() {
  using namespace application;
  assert(is_still_image_container("png_pipe") && is_still_image_container("image2") &&
         is_still_image_container("webp_pipe") && is_still_image_container("jpeg_pipe"));
  assert(!is_still_image_container("mov,mp4,m4a,3gp,3g2,mj2") && !is_still_image_container("gif") &&
         !is_still_image_container(""));
  assert(color_card_locator(0xFF8800) == "pulsar-color:ff8800" && color_card_locator(0) == "pulsar-color:000000");
  assert(color_card_rgb("pulsar-color:FF8800") == std::optional<std::uint32_t>{0xFF8800u});
  assert(!color_card_rgb("pulsar-color:ff88").has_value() && !color_card_rgb("pulsar-color:gg8800").has_value() &&
         !color_card_rgb("photo.png").has_value());
  assert(still_default_duration() == secs(5) && still_playable_duration() == secs(3600));
}

void asset_rules() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto photo = processor.import_asset("photo.png", Probe{4000, 3000, 3600, "png_pipe"});
  const auto video = processor.import_asset("clip.mp4", Probe{1280, 720, 10, "mov,mp4,m4a,3gp,3g2,mj2", 25});
  const auto color = processor.import_asset("pulsar-color:102030", Probe{1920, 1080, 3600, "pulsar-color"});
  const auto photo_asset = *document.find_asset(photo);
  const auto video_asset = *document.find_asset(video);
  const auto color_asset = *document.find_asset(color);
  assert(photo_asset.is_still() && !photo_asset.is_color_card() && photo_asset.default_clip_duration() == secs(5));
  assert(color_asset.is_still() && color_asset.is_color_card() && color_asset.default_clip_duration() == secs(5));
  assert(!video_asset.is_still() && video_asset.default_clip_duration() == secs(10));

  // A 12 MP photo is fitted into 1080p; ordinary video keeps its size; sizes stay even.
  assert((application::master_canvas_size(photo_asset) == std::pair<int, int>{1440, 1080}));
  assert((application::master_canvas_size(video_asset) == std::pair<int, int>{1280, 720}));
  assert((application::master_canvas_size(color_asset) == std::pair<int, int>{1920, 1080}));
  application::ProjectDocument portrait_document;
  application::CommandProcessor portrait_processor{portrait_document};
  const auto portrait = portrait_processor.import_asset("p.jpg", Probe{3000, 4000, 3600, "image2"});
  const auto portrait_size = application::master_canvas_size(*portrait_document.find_asset(portrait));
  assert(portrait_size.first == 810 && portrait_size.second == 1080);

  // A still can be held far longer than its default, but not past its playable span.
  (void)processor.add_clip(photo, TimeRange{MediaTime::zero(), secs(20)}, MediaTime::zero());
  assert(throws_invalid([&] { (void)processor.add_clip(photo, TimeRange{MediaTime::zero(), secs(3601)}, secs(30)); }));
}

void master_and_plan() {
  using application::exporting::ExportPlan;
  using application::exporting::ExportProfile;
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto photo = processor.import_asset("photo.png", Probe{4000, 3000, 3600, "png_pipe"});
  const auto video = processor.import_asset("clip.mp4", Probe{1280, 720, 10, "mov,mp4,m4a,3gp,3g2,mj2", 25});
  (void)processor.add_clip(photo, TimeRange{MediaTime::zero(), secs(5)}, MediaTime::zero());
  // Only a photo so far: it defines a bounded 4:3 canvas at the still frame rate.
  const auto only_photo = ExportPlan::build(document.snapshot(), ExportProfile::beta_v01());
  assert(only_photo.output_width() == 1440 && only_photo.output_height() == 1080);
  // A real video after it takes over as the master (size and frame rate).
  (void)processor.add_clip(video, TimeRange{MediaTime::zero(), secs(10)}, secs(5));
  const auto with_video = ExportPlan::build(document.snapshot(), ExportProfile::beta_v01());
  assert(with_video.output_width() == 1280 && with_video.output_height() == 720);
  assert(with_video.output_duration() == secs(15));
}

void project_round_trip() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto photo = processor.import_asset("photo.png", Probe{640, 360, 3600, "png_pipe"});
  const auto color = processor.import_asset("pulsar-color:ff8800", Probe{1920, 1080, 3600, "pulsar-color"});
  (void)processor.add_clip(photo, TimeRange{MediaTime::zero(), secs(7)}, MediaTime::zero());
  (void)processor.add_clip(color, TimeRange{secs(1), secs(4)}, secs(7));
  const auto bytes = application::ProjectCodec::encode(document.snapshot());
  auto restored = application::ProjectDocument::restore(application::ProjectCodec::decode(bytes));
  const auto photo_asset = restored.find_asset(photo);
  const auto color_asset = restored.find_asset(color);
  assert(photo_asset && photo_asset->is_still() && photo_asset->playable_duration() == secs(3600));
  assert(color_asset && color_asset->is_color_card() && color_asset->source_locator() == "pulsar-color:ff8800");
  assert(restored.timeline().duration() == secs(10));
}

} // namespace

int main() {
  locator_helpers();
  asset_rules();
  master_and_plan();
  project_round_trip();
  return 0;
}
