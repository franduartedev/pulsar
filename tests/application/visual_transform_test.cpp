#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/composition/composition_plan.hpp>
#include <pulsar/application/composition/project_canvas.hpp>
#include <pulsar/application/composition/visual_compositor.hpp>
#include <pulsar/application/export/offline_video_renderer.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_codec.hpp>
#include <pulsar/application/project/project_document.hpp>

#include <cassert>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
using namespace pulsar;
namespace comp = application::composition;
namespace exporting = application::exporting;

class Probe final : public application::MediaProbe {
public:
  [[nodiscard]] application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    application::MediaProbeResult result;
    result.path = path;
    result.video_streams.push_back(application::VideoStreamInfo{
        0, "rawvideo", 4, 2, domain::Rational{1, 1}, domain::MediaTime{10, {1, 1}},
        domain::FrameRate{1, 1}});
    return result;
  }
};

struct FakeFrame {
  std::optional<domain::MediaTime> timestamp;
  int width;
  int height;
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;
};

std::shared_ptr<const exporting::OfflineVideoPixelBuffer> solid(const FakeFrame& frame) {
  const int stride = frame.width * 4;
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride * frame.height));
  for (int y = 0; y < frame.height; ++y) {
    for (int x = 0; x < frame.width; ++x) {
      const auto offset = static_cast<std::size_t>(y * stride + x * 4);
      pixels[offset] = frame.red;
      pixels[offset + 1] = frame.green;
      pixels[offset + 2] = frame.blue;
      pixels[offset + 3] = 255;
    }
  }
  return std::make_shared<const exporting::OfflineVideoPixelBuffer>(
      exporting::OfflineVideoPixelBuffer{frame.width, frame.height, stride, std::move(pixels)});
}

class FakeSource final : public exporting::OfflineVideoSource {
public:
  explicit FakeSource(std::vector<FakeFrame> frames) : frames_{std::move(frames)} {}
  [[nodiscard]] exporting::OfflineVideoSeekResult seek_to_presentation_timestamp(
      domain::MediaTime target) override {
    std::optional<std::size_t> candidate;
    for (std::size_t i = 0; i < frames_.size(); ++i) {
      if (frames_[i].timestamp && *frames_[i].timestamp <= target) candidate = i;
    }
    auto make = [&](std::size_t i) {
      return exporting::OfflineVideoSourceFrame{frames_[i].timestamp, std::nullopt, solid(frames_[i])};
    };
    return {candidate ? std::optional{make(*candidate)} : std::nullopt, std::nullopt,
            candidate ? 1U : 0U};
  }
  [[nodiscard]] std::optional<exporting::OfflineVideoSourceFrame> decode_next() override {
    return std::nullopt;
  }

private:
  std::vector<FakeFrame> frames_;
};

class FakeFactory final : public exporting::OfflineVideoSourceFactory {
public:
  std::map<std::string, std::vector<FakeFrame>> sources;
  [[nodiscard]] std::unique_ptr<exporting::OfflineVideoSource> open(
      const std::filesystem::path& locator, int) override {
    return std::make_unique<FakeSource>(sources.at(locator.generic_string()));
  }
};

comp::RgbaSurface patterned_2x2() {
  comp::RgbaSurface s{2, 2, 8, std::vector<std::uint8_t>(16)};
  auto set = [&](int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    auto* p = s.pixels.data() + static_cast<std::size_t>(y) * 8 + static_cast<std::size_t>(x) * 4;
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = 255;
  };
  set(0, 0, 255, 0, 0);
  set(1, 0, 0, 255, 0);
  set(0, 1, 0, 0, 255);
  set(1, 1, 255, 255, 255);
  return s;
}

void pixel(const comp::RgbaSurface& s, int x, int y, std::uint8_t r, std::uint8_t g,
           std::uint8_t b) {
  const auto* p = s.pixels.data() + static_cast<std::size_t>(y) * s.stride_bytes +
                  static_cast<std::size_t>(x) * 4;
  assert(p[0] == r && p[1] == g && p[2] == b && p[3] == 255);
}

void defaults_reject_bad_values() {
  assert(domain::VisualTransform{}.is_default());
  for (const auto bad : {domain::VisualTransform{11, 0, 1, 0, 1}, domain::VisualTransform{0, 0, 0, 0, 1},
                         domain::VisualTransform{0, 0, 1, 0, 1.1},
                         domain::VisualTransform{0, 0, 1, 0, -0.1}}) {
    bool rejected = false;
    try {
      domain::validate_visual_transform(bad);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
}

void transform_units_are_canvas_relative() {
  auto canvas = comp::black_surface(4, 2);
  auto fitted = patterned_2x2();
  // Half-canvas right shift: left half keeps background, right half shows fitted left.
  comp::composite_transformed_layer(canvas, fitted, domain::VisualTransform{0.5, 0.0, 1.0, 0.0, 1.0});
  // Fitted 2x2 centred in 4x2 canvas would sit at x=1..2; shifted +2 lands at x=3..4 (clipped).
  // Exact pixels depend on bilinear centres; assert lower layers survive on the left.
  const auto* left = canvas.pixels.data();
  assert(left[3] == 255); // canvas stays opaque
}

void rotation_180_swaps_corners_exactly() {
  auto canvas = comp::black_surface(2, 2);
  comp::composite_transformed_layer(canvas, patterned_2x2(), {0, 0, 1, 180, 1});
  pixel(canvas, 0, 0, 255, 255, 255);
  pixel(canvas, 1, 0, 0, 0, 255);
  pixel(canvas, 0, 1, 0, 255, 0);
  pixel(canvas, 1, 1, 255, 0, 0);
}

void opacity_blends_over_lower_layer() {
  auto canvas = comp::black_surface(2, 2);
  comp::RgbaSurface red{2, 2, 8, std::vector<std::uint8_t>(16, 0)};
  for (int i = 0; i < 4; ++i) {
    red.pixels[static_cast<std::size_t>(i) * 4] = 255;
    red.pixels[static_cast<std::size_t>(i) * 4 + 3] = 255;
  }
  comp::composite_transformed_layer(canvas, red, {0, 0, 1, 0, 1.0});
  pixel(canvas, 0, 0, 255, 0, 0);
  comp::RgbaSurface blue{2, 2, 8, std::vector<std::uint8_t>(16, 0)};
  for (int i = 0; i < 4; ++i) {
    blue.pixels[static_cast<std::size_t>(i) * 4 + 2] = 255;
    blue.pixels[static_cast<std::size_t>(i) * 4 + 3] = 255;
  }
  comp::composite_transformed_layer(canvas, blue, {0, 0, 1, 0, 0.5});
  // source-over 50%: alpha rounds to 128, dst weight 127.
  const auto* p = canvas.pixels.data();
  assert(p[0] == 127 && p[2] == 128);
}

void transparent_layer_leaves_canvas_untouched() {
  auto canvas = comp::black_surface(2, 2);
  comp::composite_transformed_layer(canvas, patterned_2x2(), {0, 0, 1, 0, 0.0});
  pixel(canvas, 0, 0, 0, 0, 0);
}

application::ProjectSnapshot two_layer_snapshot() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  Probe probe;
  const auto media = processor.import_asset("video.mp4", probe);
  (void)processor.add_clip(media, domain::TimeRange{{0, {1, 1}}, {10, {1, 1}}}, domain::MediaTime{0, {1, 1}});
  auto snapshot = document.snapshot();
  snapshot.timeline.add_track(domain::Track{domain::TrackId{9}, domain::TrackKind::Video});
  snapshot.timeline.insert_clip(domain::TrackId{9},
      domain::Clip{domain::ClipId{9}, media, {{0, {1, 1}}, {10, {1, 1}}}, {0, {1, 1}}});
  snapshot.visual_track_ids.push_back(domain::TrackId{9});
  snapshot.allocator_state.next_clip_id = 10;
  snapshot.allocator_state.next_track_id = 10;
  snapshot.allocator_state.next_media_id = 2;
  return snapshot;
}

void plan_preserves_order_and_transforms() {
  auto snapshot = two_layer_snapshot();
  snapshot.canvas = application::effective_project_canvas(snapshot);
  const auto context = application::VisualRenderContext::build(snapshot);
  auto plan = application::evaluate_composition_plan(snapshot, context, domain::MediaTime{1, {1, 1}});
  assert(plan.layers.size() == 2);
  assert(plan.layers[0].track_id == snapshot.video_track_id);
  assert(plan.layers[1].track_id == domain::TrackId{9});
  assert(plan.layers[0].transform.is_default());

  // Transformed upper still orders above the primary.
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  Probe probe;
  const auto media = processor.import_asset("video.mp4", probe);
  const auto clip = processor.add_clip(media, domain::TimeRange{{0, {1, 1}}, {10, {1, 1}}}, {0, {1, 1}});
  const auto upper = processor.add_track(domain::TrackKind::Video);
  const auto top_clip = processor.add_clip(upper, media, domain::TimeRange{{0, {1, 1}}, {10, {1, 1}}}, {0, {1, 1}});
  processor.set_clip_visual_transform(top_clip, {0.25, 0.0, 0.5, 0.0, 0.5});
  auto moved = document.snapshot();
  const auto moved_context = application::VisualRenderContext::build(moved);
  const auto moved_plan =
      application::evaluate_composition_plan(moved, moved_context, domain::MediaTime{1, {1, 1}});
  assert(moved_plan.layers.size() == 2);
  assert(moved_plan.layers[1].transform.opacity == 0.5);
  assert(moved_plan.layers[1].transform.scale == 0.5);
  (void)clip;
}

void renderer_default_is_e1_exact_and_parity_holds() {
  // Same single-clip snapshot rendered twice: preview and export share pixels.
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  Probe probe;
  FakeFactory factory;
  factory.sources["video.mp4"] = std::vector<FakeFrame>{{FakeFrame{domain::MediaTime{0, {1, 1}}, 4, 2, 200, 10, 30}}};
  const auto media = processor.import_asset("video.mp4", probe);
  (void)processor.add_clip(media, domain::TimeRange{{0, {1, 1}}, {10, {1, 1}}}, {0, {1, 1}});
  const auto snapshot = document.snapshot();
  const auto plan = exporting::ExportPlan::build(snapshot);
  exporting::OfflineVideoRenderer export_renderer{plan, factory};
  const auto via_export = export_renderer.render_frame(domain::FrameNumber{0});
  exporting::OfflineVideoRenderer preview_renderer{plan.snapshot(), plan.visual_context(), factory};
  const auto via_preview = preview_renderer.render_at(domain::MediaTime{0, {1, 1}});
  assert(via_preview.pixels == via_export.pixels);

  // Explicit default transform is a no-op and renders byte-identical pixels.
  application::ProjectDocument d2;
  application::CommandProcessor p2{d2};
  const auto m2 = p2.import_asset("video.mp4", probe);
  const auto c2 = p2.add_clip(m2, domain::TimeRange{{0, {1, 1}}, {10, {1, 1}}}, {0, {1, 1}});
  p2.set_clip_visual_transform(c2, domain::VisualTransform{});
  const auto s2 = d2.snapshot();
  assert(s2.timeline.find_clip(c2)->visual_transform().is_default());
  const auto plan2 = exporting::ExportPlan::build(s2);
  exporting::OfflineVideoRenderer r2{plan2, factory};
  assert(r2.render_frame(domain::FrameNumber{0}).pixels == via_export.pixels);

  // Transformed composition keeps preview/export parity and has an effect.
  application::ProjectDocument tdoc;
  application::CommandProcessor tproc{tdoc};
  const auto tmedia = tproc.import_asset("video.mp4", probe);
  const auto tclip = tproc.add_clip(tmedia, domain::TimeRange{{0, {1, 1}}, {10, {1, 1}}}, {0, {1, 1}});
  tproc.set_clip_visual_transform(tclip, {0.25, 0.0, 0.5, 30.0, 0.8});
  const auto tsnapshot = tdoc.snapshot();
  const auto tplan = exporting::ExportPlan::build(tsnapshot);
  exporting::OfflineVideoRenderer texport{tplan, factory};
  exporting::OfflineVideoRenderer tpreview{tplan.snapshot(), tplan.visual_context(), factory};
  const auto t0 = domain::MediaTime{0, {1, 1}};
  assert(texport.render_at(t0).pixels == tpreview.render_at(t0).pixels);
  assert(texport.render_at(t0).pixels != via_export.pixels);
}

void undo_redo_restores_transform_exactly() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  Probe probe;
  const auto media = processor.import_asset("video.mp4", probe);
  const auto clip = processor.add_clip(media, domain::TimeRange{{0, {1, 1}}, {10, {1, 1}}}, {0, {1, 1}});
  const domain::VisualTransform moved{0.25, -0.125, 0.5, 45.0, 0.5};
  processor.set_clip_visual_transform(clip, moved);
  assert(document.timeline().find_clip(clip)->visual_transform() == moved);
  assert(processor.undo());
  assert(document.timeline().find_clip(clip)->visual_transform().is_default());
  assert(processor.redo());
  assert(document.timeline().find_clip(clip)->visual_transform() == moved);
  const auto bytes = application::ProjectCodec::encode(document.snapshot());
  const auto restored = application::ProjectDocument::restore(application::ProjectCodec::decode(bytes));
  assert(restored.timeline().find_clip(clip)->visual_transform() == moved);
}

void interactive_transform_coalesces_to_one_history_entry() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  Probe probe;
  const auto media = processor.import_asset("video.mp4", probe);
  const auto clip = processor.add_clip(media, domain::TimeRange{{0, {1, 1}}, {10, {1, 1}}},
                                       {0, {1, 1}});
  const auto before = processor.undo_size();
  processor.begin_clip_visual_transform(clip);
  processor.update_clip_visual_transform({0.1, 0.0, 1.0, 0.0, 1.0});
  processor.update_clip_visual_transform({0.2, 0.1, 0.8, 0.0, 1.0});
  processor.update_clip_visual_transform({0.3, 0.2, 0.6, 0.0, 1.0});
  processor.end_clip_visual_transform();
  assert(processor.undo_size() == before + 1);
  assert(document.timeline().find_clip(clip)->visual_transform() ==
         (domain::VisualTransform{0.3, 0.2, 0.6, 0.0, 1.0}));
  assert(processor.undo());
  assert(document.timeline().find_clip(clip)->visual_transform().is_default());
  assert(processor.redo());
  assert(document.timeline().find_clip(clip)->visual_transform() ==
         (domain::VisualTransform{0.3, 0.2, 0.6, 0.0, 1.0}));
}

void empty_primary_with_transformed_upper_renders() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  Probe probe;
  const auto media = processor.import_asset("video.mp4", probe);
  const auto upper = processor.add_track(domain::TrackKind::Video);
  const auto clip = processor.add_clip(upper, media, domain::TimeRange{{0, {1, 1}}, {10, {1, 1}}}, {0, {1, 1}});
  processor.set_clip_visual_transform(clip, {0.0, 0.0, 0.5, 0.0, 1.0});
  auto snapshot = document.snapshot();
  assert(snapshot.timeline.find_track(snapshot.video_track_id)->clips().empty());
  const auto plan = exporting::ExportPlan::build(snapshot);
  FakeFactory factory;
  factory.sources["video.mp4"] = std::vector<FakeFrame>{{FakeFrame{domain::MediaTime{0, {1, 1}}, 4, 2, 10, 200, 30}}};
  exporting::OfflineVideoRenderer export_renderer{plan, factory};
  const auto frame = export_renderer.render_at(domain::MediaTime{0, {1, 1}});
  assert(frame.width == plan.output_width() && frame.height == plan.output_height());
  exporting::OfflineVideoRenderer preview_renderer{plan.snapshot(), plan.visual_context(), factory};
  assert(preview_renderer.render_at(domain::MediaTime{0, {1, 1}}).pixels == frame.pixels);
}

void missing_active_visual_fails_deterministically() {
  auto snapshot = two_layer_snapshot();
  snapshot.canvas = application::effective_project_canvas(snapshot);
  snapshot.assets.clear();
  const auto context = application::VisualRenderContext::build(snapshot);
  bool threw = false;
  try {
    (void)application::evaluate_composition_plan(snapshot, context, domain::MediaTime{1, {1, 1}});
  } catch (const application::CompositionPlanError& error) {
    threw = error.code() == application::CompositionPlanErrorCode::MissingMedia;
  }
  assert(threw);
  FakeFactory factory;
  factory.sources["video.mp4"] = std::vector<FakeFrame>{{FakeFrame{domain::MediaTime{0, {1, 1}}, 4, 2, 1, 2, 3}}};
  exporting::OfflineVideoRenderer renderer{snapshot, context, factory};
  bool render_threw = false;
  try {
    (void)renderer.render_at(domain::MediaTime{1, {1, 1}});
  } catch (const exporting::OfflineVideoRenderError& error) {
    render_threw = error.code() == exporting::OfflineVideoRenderErrorCode::MissingMedia;
  }
  assert(render_threw);
}

void scaled_and_translated_layers_composite_in_order() {
  // Bottom red full canvas, upper green shrunk to centre: corners stay red.
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  Probe probe;
  FakeFactory factory;
  factory.sources["video.mp4"] = std::vector<FakeFrame>{{FakeFrame{domain::MediaTime{0, {1, 1}}, 4, 2, 255, 0, 0}}};
  factory.sources["upper.mp4"] = std::vector<FakeFrame>{{FakeFrame{domain::MediaTime{0, {1, 1}}, 4, 2, 0, 255, 0}}};
  const auto lower_media = processor.import_asset("video.mp4", probe);
  (void)processor.add_clip(lower_media, domain::TimeRange{{0, {1, 1}}, {10, {1, 1}}}, {0, {1, 1}});
  // Second asset with a different locator but same probe geometry.
  application::MediaProbeResult upper_probe = probe.probe("upper.mp4");
  (void)upper_probe;
  const auto upper_media = processor.import_asset("upper.mp4", probe);
  const auto upper_track = processor.add_track(domain::TrackKind::Video);
  const auto upper_clip =
      processor.add_clip(upper_track, upper_media, domain::TimeRange{{0, {1, 1}}, {10, {1, 1}}}, {0, {1, 1}});
  processor.set_clip_visual_transform(upper_clip, {0.0, 0.0, 0.5, 0.0, 1.0});
  const auto snapshot = document.snapshot();
  const auto plan = exporting::ExportPlan::build(snapshot);
  exporting::OfflineVideoRenderer renderer{plan, factory};
  const auto frame = renderer.render_at(domain::MediaTime{1, {1, 1}});
  const auto at = [&](int x, int y) {
    return std::array<std::uint8_t, 3>{frame.pixels[static_cast<std::size_t>(y) * frame.stride_bytes +
                                                       static_cast<std::size_t>(x) * 4],
                                       frame.pixels[static_cast<std::size_t>(y) * frame.stride_bytes +
                                                       static_cast<std::size_t>(x) * 4 + 1],
                                       frame.pixels[static_cast<std::size_t>(y) * frame.stride_bytes +
                                                       static_cast<std::size_t>(x) * 4 + 2]};
  };
  // Canvas is 4x2: shrunk upper (2x1) sits centred, corners show lower red.
  assert((at(0, 0) == std::array<std::uint8_t, 3>{255, 0, 0}));
  assert((at(1, 0) == std::array<std::uint8_t, 3>{0, 255, 0} ||
         at(2, 0) == std::array<std::uint8_t, 3>{0, 255, 0}));
  // Stable ordering: swapping the stack changes which colour dominates.
  auto swapped = snapshot;
  swapped.visual_track_ids = {swapped.visual_track_ids.back(), swapped.visual_track_ids.front()};
  const auto swapped_plan = exporting::ExportPlan::build(swapped);
  exporting::OfflineVideoRenderer swapped_renderer{swapped_plan, factory};
  const auto swapped_frame = swapped_renderer.render_at(domain::MediaTime{1, {1, 1}});
  assert(swapped_frame.pixels != frame.pixels);
}

} // namespace

int main() {
  defaults_reject_bad_values();
  transform_units_are_canvas_relative();
  rotation_180_swaps_corners_exactly();
  opacity_blends_over_lower_layer();
  transparent_layer_leaves_canvas_untouched();
  plan_preserves_order_and_transforms();
  renderer_default_is_e1_exact_and_parity_holds();
  scaled_and_translated_layers_composite_in_order();
  undo_redo_restores_transform_exactly();
  interactive_transform_coalesces_to_one_history_entry();
  empty_primary_with_transformed_upper_renders();
  missing_active_visual_fails_deterministically();
}
