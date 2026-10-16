#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/composition/composition_plan.hpp>
#include <pulsar/application/composition/visual_render_context.hpp>
#include <pulsar/application/export/offline_video_renderer.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_codec.hpp>

#include <cassert>
#include <array>
#include <cmath>
#include <map>
#include <memory>

namespace {
using namespace pulsar;

class Probe final : public application::MediaProbe {
public:
  application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    application::MediaProbeResult result;
    result.path = path;
    result.video_streams.push_back({0, "raw", 1920, 1080, {1, 30},
                                    domain::MediaTime{20, {1, 1}}, domain::FrameRate{30, 1}});
    return result;
  }
};

class Source final : public application::exporting::OfflineVideoSource {
public:
  explicit Source(std::shared_ptr<const application::exporting::OfflineVideoPixelBuffer> image)
      : image_{std::move(image)} {}
  application::exporting::OfflineVideoSeekResult seek_to_presentation_timestamp(domain::MediaTime) override {
    return {application::exporting::OfflineVideoSourceFrame{domain::MediaTime::zero(),
             domain::MediaTime{20, {1, 1}}, image_}, {}, 1};
  }
  std::optional<application::exporting::OfflineVideoSourceFrame> decode_next() override {
    return std::nullopt;
  }
private:
  std::shared_ptr<const application::exporting::OfflineVideoPixelBuffer> image_;
};

class Factory final : public application::exporting::OfflineVideoSourceFactory {
public:
  std::map<std::string, std::array<std::uint8_t, 3>> colors;
  std::unique_ptr<application::exporting::OfflineVideoSource> open(const std::filesystem::path& path,
                                                                    int) override {
    const auto color = colors.at(path.string());
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(1920 * 1080 * 4));
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
      pixels[i] = color[0]; pixels[i + 1] = color[1]; pixels[i + 2] = color[2]; pixels[i + 3] = 255;
    }
    auto image = std::make_shared<const application::exporting::OfflineVideoPixelBuffer>(
        application::exporting::OfflineVideoPixelBuffer{1920, 1080, 1920 * 4, std::move(pixels)});
    return std::make_unique<Source>(std::move(image));
  }
};

void cross_dissolve_is_explicit_persistent_and_undoable() {
  application::ProjectDocument document;
  application::CommandProcessor commands{document};
  Probe probe;
  const auto media = commands.import_asset("red.mp4", probe);
  const auto next_media = commands.import_asset("blue.mp4", probe);
  const auto outgoing = commands.add_clip(media, {{0, {1, 1}}, {5, {1, 1}}}, {0, {1, 1}});
  const auto incoming = commands.add_clip(next_media, {{5, {1, 1}}, {10, {1, 1}}}, {5, {1, 1}});
  const domain::Transition transition{document.video_track_id(), outgoing, incoming,
                                      domain::TransitionType::CrossDissolve, {1, {1, 1}}};
  const auto history = commands.undo_size();
  commands.add_transition(transition);
  assert(commands.undo_size() == history + 1 && document.transitions().size() == 1);

  const auto snapshot = document.snapshot();
  const auto context = application::VisualRenderContext::build(snapshot);
  const auto plan = application::evaluate_composition_plan(snapshot, context,
                                                            {9, {1, 2}}); // 4.5 s
  assert(plan.layers.size() == 2);
  assert(plan.layers[0].clip_id == outgoing && plan.layers[1].clip_id == incoming);
  assert((plan.layers[1].source_time == domain::MediaTime{9, {1, 2}}));
  assert(std::abs(plan.layers[1].transform.opacity - 0.5) < 1e-12);

  Factory factory;
  factory.colors["red.mp4"] = {255, 0, 0};
  factory.colors["blue.mp4"] = {0, 0, 255};
  application::exporting::OfflineVideoRenderer preview{snapshot, context, factory};
  application::exporting::OfflineVideoRenderer export_path{snapshot, context, factory};
  const auto preview_frame = preview.render_at({9, {1, 2}});
  const auto export_frame = export_path.render_at({9, {1, 2}});
  assert(preview_frame.pixels == export_frame.pixels);
  assert(preview_frame.pixels[0] == 127 && preview_frame.pixels[2] == 128);

  const auto encoded = application::ProjectCodec::encode(snapshot);
  assert(encoded.rfind("PULSAR_PROJECT 6\n", 0) == 0);
  assert(encoded.find("TRANSITION CROSS_DISSOLVE") != std::string::npos);
  const auto restored = application::ProjectDocument::restore(application::ProjectCodec::decode(encoded));
  assert(restored.transitions() == std::vector<domain::Transition>{transition});

  bool rejected = false;
  try { commands.trim_clip_end(outgoing, {4, {1, 1}}); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert((rejected && document.timeline().find_clip(outgoing)->timeline_range().end() ==
                         domain::MediaTime{5, {1, 1}}));
  assert(commands.undo());
  assert(document.transitions().empty());
  assert(commands.redo());
  assert(document.transitions().size() == 1);
}

void invalid_transition_is_rejected_atomically() {
  application::ProjectDocument document;
  application::CommandProcessor commands{document};
  Probe probe;
  const auto media = commands.import_asset("video.mp4", probe);
  const auto a = commands.add_clip(media, {{0, {1, 1}}, {5, {1, 1}}}, {0, {1, 1}});
  const auto b = commands.add_clip(media, {{0, {1, 1}}, {5, {1, 1}}}, {5, {1, 1}});
  const auto history = commands.undo_size();
  bool rejected = false;
  try {
    commands.add_transition({document.video_track_id(), a, b,
                             domain::TransitionType::CrossDissolve, {1, {1, 1}}});
  } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected && document.transitions().empty() && commands.undo_size() == history);
}
} // namespace

int main() {
  cross_dissolve_is_explicit_persistent_and_undoable();
  invalid_transition_is_rejected_atomically();
}
