// Titles: domain rules, undoable commands, project file round trip, compositor, export sizing.
#include <pulsar/application/audio/output_stage.hpp>
#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/composition/title_compositor.hpp>
#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_codec.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/domain/timeline/title.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace pulsar;
using domain::MediaTime;
using domain::Rational;
using domain::TimeRange;
using domain::Title;
using domain::TitleId;
namespace composition = application::composition;

MediaTime secs(std::int64_t value) { return MediaTime{value, Rational{1, 1}}; }
MediaTime ms(std::int64_t value) { return MediaTime{value, Rational{1, 1000}}; }

class Probe final : public application::MediaProbe {
public:
  Probe(int width, int height, std::int64_t seconds) : width_{width}, height_{height}, seconds_{seconds} {}
  [[nodiscard]] application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    application::MediaProbeResult result;
    result.path = path;
    result.video_streams.push_back(application::VideoStreamInfo{0, "h264", width_, height_, Rational{1, 1000},
                                                                 secs(seconds_), domain::FrameRate{30, 1}});
    return result;
  }

private:
  int width_, height_;
  std::int64_t seconds_;
};

bool throws_invalid(const std::function<void()>& body) {
  try { body(); } catch (const std::invalid_argument&) { return true; } catch (...) { return false; }
  return false;
}

void title_domain_rules() {
  const Title ok{TitleId{1}, "Hola", TimeRange{secs(1), secs(4)}};
  assert(ok.size_permille() == 70 && ok.color_rgba() == 0xFFFFFFFFu && ok.shadow());
  assert(ok.active_at(secs(1)) && ok.active_at(ms(3999)) && !ok.active_at(secs(4)) && !ok.active_at(ms(999)));
  assert(throws_invalid([] { Title t{TitleId{1}, "", TimeRange{secs(0), secs(1)}}; }));
  assert(throws_invalid([] { Title t{TitleId{1}, std::string(401, 'x'), TimeRange{secs(0), secs(1)}}; }));
  assert(throws_invalid([] { Title t{TitleId{1}, "a\tb", TimeRange{secs(0), secs(1)}}; }));
  assert(throws_invalid([] { Title t{TitleId{1}, "x", TimeRange{secs(0), secs(1)}, 5}; }));
  assert(throws_invalid([] { Title t{TitleId{1}, "x", TimeRange{secs(0), secs(1)}, 500}; }));
  assert(!throws_invalid([] { Title t{TitleId{1}, "dos\nlíneas ñ ✓", TimeRange{secs(0), secs(1)}}; }));
  // Layout identity ignores timing, not look.
  assert(ok.look_key() == ok.with_range(TimeRange{secs(2), secs(3)}).look_key());
  assert(ok.look_key() != ok.with_text("Chau").look_key());
  assert(ok.look_key() != ok.with_style(90, 0xFFFFFFFFu, domain::TitleHAlign::Center, domain::TitleVAlign::Bottom, true).look_key());
}

void title_commands_undo_redo_and_overlap() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto before = document.history_state_id();
  const auto id = processor.add_title(Title{document.next_title_id(), "Uno", TimeRange{secs(1), secs(3)}});
  assert(document.titles().size() == 1 && document.is_dirty());
  const auto second = processor.add_title(Title{document.next_title_id(), "Dos", TimeRange{secs(5), secs(6)}});
  assert(second != id && document.titles().size() == 2 && document.titles().front().text() == "Uno");

  // Overlap is refused and leaves no trace; a stale id is refused too.
  const auto undo_size = processor.undo_size();
  assert(throws_invalid([&] { (void)processor.add_title(Title{document.next_title_id(), "X", TimeRange{secs(2), secs(5)}}); }));
  assert(throws_invalid([&] { (void)processor.add_title(Title{id, "dup", TimeRange{secs(7), secs(8)}}); }));
  assert(throws_invalid([&] { processor.replace_title(document.titles().front().with_range(TimeRange{secs(4), secs(6)})); }));
  assert(processor.undo_size() == undo_size && document.titles().size() == 2);

  // One replace edits text, time and style in ONE step; undo/redo restore exactly.
  const auto edited = document.titles().front().with_text("Uno editado").with_range(TimeRange{secs(0), secs(2)})
                          .with_style(100, 0xFF8800FFu, domain::TitleHAlign::Left, domain::TitleVAlign::Top, false);
  processor.replace_title(edited);
  assert(document.find_title(id)->text() == "Uno editado" && document.find_title(id)->start() == secs(0));
  assert(processor.undo());
  assert(document.find_title(id)->text() == "Uno" && document.find_title(id)->start() == secs(1) && document.find_title(id)->shadow());
  assert(processor.redo());
  assert(document.find_title(id)->color_rgba() == 0xFF8800FFu && !document.find_title(id)->shadow());

  processor.remove_title(second);
  assert(document.titles().size() == 1);
  assert(processor.undo() && document.titles().size() == 2 && document.find_title(second)->text() == "Dos");
  assert(processor.redo() && document.titles().size() == 1);
  while (processor.undo()) {}
  assert(document.titles().empty() && document.history_state_id() == before && !document.is_dirty());
  // Ids are never reused (allocator survives undo).
  const auto third = processor.add_title(Title{document.next_title_id(), "Tres", TimeRange{secs(1), secs(2)}});
  assert(third.value() > second.value());
}

void title_project_file_round_trip() {
  application::ProjectDocument document;
  application::CommandProcessor processor{document};
  const auto media = processor.import_asset("v.mp4", Probe{1280, 720, 30});
  (void)processor.add_clip(media, TimeRange{MediaTime::zero(), secs(30)}, MediaTime::zero());
  const std::string tricky = "Línea 1 \"citada\"\nLínea 2 \\ barra ñ ✓ 🎬";
  (void)processor.add_title(Title{document.next_title_id(), tricky, TimeRange{MediaTime{1, Rational{1, 3}}, MediaTime{10, Rational{1, 3}}},
                                  120, 0x12345678u, domain::TitleHAlign::Right, domain::TitleVAlign::Middle, false});
  (void)processor.add_title(Title{document.next_title_id(), "Final", TimeRange{secs(20), secs(25)}});
  const auto bytes = application::ProjectCodec::encode(document.snapshot());
  assert(bytes.rfind("PULSAR_PROJECT 3\n", 0) == 0);
  const auto state = application::ProjectCodec::decode(bytes);
  assert(state.titles.size() == 2 && state.allocator_state.next_title_id == 3);
  const auto restored = application::ProjectDocument::restore(state);
  assert(restored.titles().size() == 2);
  const auto& first = restored.titles()[0];
  assert(first.text() == tricky && first.start() == MediaTime(1, Rational{1, 3}) && first.end() == MediaTime(10, Rational{1, 3}));
  assert(first.size_permille() == 120 && first.color_rgba() == 0x12345678u);
  assert(first.horizontal() == domain::TitleHAlign::Right && first.vertical() == domain::TitleVAlign::Middle && !first.shadow());
  assert(application::ProjectCodec::encode(restored.snapshot()) == bytes);   // stable bytes

  // A project without titles keeps the version-2 format byte for byte.
  application::ProjectDocument plain;
  application::CommandProcessor plain_processor{plain};
  const auto plain_bytes = application::ProjectCodec::encode(plain.snapshot());
  assert(plain_bytes.rfind("PULSAR_PROJECT 2\n", 0) == 0 && plain_bytes.find("TITLE") == std::string::npos);

  // Damaged title records are rejected without partial state.
  const auto line_start = bytes.find("TITLE 2 ");
  const auto line_end = bytes.find('\n', line_start);
  const auto mutate = [&](const std::string& replacement) {
    return bytes.substr(0, line_start) + replacement + bytes.substr(line_end);
  };
  for (const std::string& bad : {std::string{"TITLE 2 20 1 1 25 1 1 70 4294967296 1 2 1 \"x\""},       // colour out of range
                                 std::string{"TITLE 2 20 1 1 25 1 1 70 1 9 2 1 \"x\""},               // alignment
                                 std::string{"TITLE 2 20 1 1 25 1 1 5 1 1 2 1 \"x\""},                // size
                                 std::string{"TITLE 2 20 1 1 25 1 1 70 1 1 2 1 x"},                   // unquoted text
                                 std::string{"TITLE 2 20 1 1 25 1 1 70 1 1 2 1 \"x\" extra"},
                                 std::string{"TITLE 1 20 1 1 25 1 1 70 1 1 2 1 \"dup id\""},          // duplicate id
                                 std::string{"TITLE 2 1 1 1 4 1 1 70 1 1 2 1 \"overlap\""}}) {         // overlaps title 1
    bool rejected = false;
    try { (void)application::ProjectDocument::restore(application::ProjectCodec::decode(mutate(bad))); }
    catch (const std::exception&) { rejected = true; }
    assert(rejected);
  }
}

class FlatRasterizer final : public composition::TitleRasterizer {
public:
  int calls{0};
  bool fail{false};
  std::optional<composition::TitleRaster> rasterize(const Title&, int w, int h) override {
    ++calls;
    if (fail) throw std::runtime_error{"font missing"};
    composition::TitleRaster raster;
    raster.x = w / 4;
    raster.y = h / 4;
    raster.width = 4;
    raster.height = 4;
    raster.premultiplied_rgba.assign(4 * 4 * 4, 0);
    for (int i = 0; i < 16; ++i) {           // 50% white, premultiplied
      raster.premultiplied_rgba[i * 4 + 0] = 128;
      raster.premultiplied_rgba[i * 4 + 1] = 128;
      raster.premultiplied_rgba[i * 4 + 2] = 128;
      raster.premultiplied_rgba[i * 4 + 3] = 128;
    }
    return raster;
  }
};

void compositor_blend_cache_and_failure() {
  std::vector<std::uint8_t> pixels(16 * 16 * 4, 0);
  for (std::size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
  composition::RgbaCanvas canvas{16, 16, 64, pixels.data()};
  auto rasterizer = std::make_shared<FlatRasterizer>();
  composition::TitleCompositor compositor{rasterizer};
  const std::vector<Title> titles{Title{TitleId{1}, "A", TimeRange{secs(1), secs(2)}}, Title{TitleId{2}, "B", TimeRange{secs(2), secs(3)}}};

  assert(!compositor.composite(canvas, titles, ms(500)) && rasterizer->calls == 0);     // nothing active: untouched
  assert(pixels[(4 * 16 + 4) * 4] == 0);
  assert(compositor.composite(canvas, titles, secs(1)));
  assert(pixels[(4 * 16 + 4) * 4] == 128 && pixels[(4 * 16 + 4) * 4 + 3] == 255);       // 50% white over black
  assert(pixels[(3 * 16 + 3) * 4] == 0);                                                // outside the raster
  // Another frame of the same title: cached, no new raster; a different title: new raster.
  assert(compositor.composite(canvas, titles, ms(1500)) && rasterizer->calls == 1);
  assert(compositor.composite(canvas, titles, secs(2)) && rasterizer->calls == 2);
  assert(!compositor.composite(canvas, titles, secs(3)));                              // end is exclusive
  // Rasterising again for another canvas size.
  std::vector<std::uint8_t> big(32 * 32 * 4, 0);
  assert(compositor.composite(composition::RgbaCanvas{32, 32, 128, big.data()}, titles, secs(1)) && rasterizer->calls == 3);

  // A raster hanging off the canvas is clipped, never out of bounds.
  composition::TitleRaster edge;
  edge.x = 14; edge.y = -2; edge.width = 4; edge.height = 4;
  edge.premultiplied_rgba.assign(64, 255);
  std::vector<std::uint8_t> small(16 * 16 * 4, 10);
  composition::blend_premultiplied_over(composition::RgbaCanvas{16, 16, 64, small.data()}, edge);
  assert(small[(0 * 16 + 14) * 4] == 255 && small[(0 * 16 + 13) * 4] == 10);

  // A rasteriser that throws never breaks the frame (and is not retried every frame).
  auto failing = std::make_shared<FlatRasterizer>();
  failing->fail = true;
  composition::TitleCompositor safe{failing};
  std::vector<std::uint8_t> frame(16 * 16 * 4, 7);
  assert(!safe.composite(composition::RgbaCanvas{16, 16, 64, frame.data()}, titles, secs(1)));
  assert(!safe.composite(composition::RgbaCanvas{16, 16, 64, frame.data()}, titles, ms(1200)) && failing->calls == 1);
  assert(frame[0] == 7);
}

void export_preset_sizing() {
  using application::exporting::ExportPlan;
  using application::exporting::ExportProfile;
  const auto dims = [&](int w, int h, const ExportProfile& profile) {
    application::ProjectDocument document;
    application::CommandProcessor processor{document};
    const auto media = processor.import_asset("v.mp4", Probe{w, h, 10});
    (void)processor.add_clip(media, TimeRange{MediaTime::zero(), secs(10)}, MediaTime::zero());
    const auto plan = ExportPlan::build(document.snapshot(), profile);
    return std::pair<std::int64_t, std::int64_t>{plan.output_width(), plan.output_height()};
  };
  using Dims = std::pair<std::int64_t, std::int64_t>;
  const auto original = ExportProfile::beta_v01();
  const auto p1080 = original.with_max_output(1920, 1080);
  const auto p720 = original.with_max_output(1280, 720);
  assert((dims(3840, 2160, original) == Dims{3840, 2160}));                       // Original: untouched
  assert((dims(3840, 2160, p1080) == Dims{1920, 1080}));
  assert((dims(3840, 2160, p720) == Dims{1280, 720}));
  assert((dims(1280, 720, p1080) == Dims{1280, 720}));                            // never enlarged
  assert((dims(1920, 1080, p1080) == Dims{1920, 1080}));
  assert((dims(2160, 3840, p1080) == Dims{608, 1080}));                           // vertical: aspect kept
  assert((dims(4000, 3000, p1080) == Dims{1440, 1080}));                          // 4:3
  const auto odd = dims(2561, 1441, p1080);                                       // odd source, even result
  assert(odd.first % 2 == 0 && odd.second % 2 == 0 && odd.first <= 1920 && odd.second <= 1080);
}

void fit_geometry_cases() {
  const auto fit = composition::fit_geometry(1920, 1080, 1280, 720);
  assert(fit.scaled_width == 1280 && fit.scaled_height == 720 && fit.offset_x == 0 && fit.offset_y == 0);
  const auto pillar = composition::fit_geometry(1080, 1920, 1920, 1080);
  assert(pillar.scaled_height == 1080 && pillar.scaled_width == 608 && pillar.offset_x == (1920 - 608) / 2 && pillar.offset_y == 0);
  const auto letter = composition::fit_geometry(1000, 1000, 1920, 1080);
  assert(letter.scaled_width == 1080 && letter.scaled_height == 1080);
}

void output_stage_properties() {
  using application::soft_output_stage;
  using application::kOutputKnee;
  for (const float x : {0.0F, 0.1F, -0.5F, 0.8999F, -0.8999F, kOutputKnee, -kOutputKnee}) assert(soft_output_stage(x) == x);   // untouched
  float previous = kOutputKnee;
  for (float x = kOutputKnee; x < 50.0F; x += 0.05F) {                       // monotonic, bounded, never reaches full scale
    const float y = soft_output_stage(x);
    assert(y >= previous && y < 1.0F);
    assert(soft_output_stage(-x) == -y);                                   // odd symmetry
    previous = y;
  }
  assert(soft_output_stage(1.0F) > kOutputKnee && soft_output_stage(1.0F) < 1.0F);
  assert(soft_output_stage(1e9F) <= 1.0F && soft_output_stage(-1e9F) >= -1.0F);
  assert(soft_output_stage(std::nanf("")) == 0.0F && soft_output_stage(INFINITY) == 0.0F);
  // Continuous at the knee with slope ~1.
  const float just_above = soft_output_stage(kOutputKnee + 1e-4F);
  assert(std::fabs((just_above - kOutputKnee) - 1e-4F) < 2e-6F);
}

} // namespace

int main() {
  output_stage_properties();
  title_domain_rules();
  title_commands_undo_redo_and_overlap();
  title_project_file_round_trip();
  compositor_blend_cache_and_failure();
  export_preset_sizing();
  fit_geometry_cases();
  return 0;
}
