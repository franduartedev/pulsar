#include <pulsar/application/composition/visual_compositor.hpp>

#include <array>
#include <cassert>
#include <stdexcept>

using namespace pulsar::application::composition;

int main() {
  const auto black = compose_opaque_layers(4, 4, {});
  for (std::size_t i = 0; i < black.pixels.size(); i += 4) {
    assert(black.pixels[i] == 0 && black.pixels[i + 1] == 0 && black.pixels[i + 2] == 0);
    assert(black.pixels[i + 3] == 255);
  }
  const RgbaSurface red{1, 1, 4, {255, 0, 0, 255}};
  const RgbaSurface blue{2, 1, 12, {0, 0, 255, 255, 0, 0, 255, 255, 99, 99, 99, 99}};
  const std::array<const RgbaSurface*, 3> layers{&red, nullptr, &blue};
  const auto result = compose_opaque_layers(4, 4, layers);
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      const auto i = static_cast<std::size_t>(y * result.stride_bytes + x * 4);
      assert(result.pixels[i] == 0 && result.pixels[i + 1] == 0);
      assert(result.pixels[i + 2] == (y == 1 || y == 2 ? 255 : 0));
      assert(result.pixels[i + 3] == 255);
    }
  }
  const std::array<const RgbaSurface*, 2> reversed{&blue, &red};
  const auto red_top = compose_opaque_layers(4, 4, reversed);
  assert(red_top.pixels[0] == 255 && red_top.pixels[2] == 0);
  const std::array<const RgbaSurface*, 2> gap{&red, nullptr};
  assert(compose_opaque_layers(4, 4, gap).pixels == red_top.pixels);

  // Invalid sources, scaler stride, placement and aliasing leave the canvas intact.
  for (const auto& bad : std::array<RgbaSurface, 4>{
           RgbaSurface{0, 1, 4, {}}, RgbaSurface{1, 1, -4, {}},
           RgbaSurface{2, 1, 4, {0, 0, 0, 255}}, RgbaSurface{1, 1, 4, {0}}}) {
    auto canvas = red_top;
    bool rejected = false;
    try { composite_opaque_layer(canvas, bad, FitGeometry{4, 4, 0, 0}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && canvas.pixels == red_top.pixels);
  }
  auto canvas = red_top;
  bool rejected = false;
  try { composite_opaque_layer(canvas, red, FitGeometry{4, 4, 1, 0}); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected && canvas.pixels == red_top.pixels);
  rejected = false;
  try { composite_opaque_layer(canvas, canvas, FitGeometry{4, 4, 0, 0}); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected && canvas.pixels == red_top.pixels);

  // Existing title blending happens after all visuals.
  blend_premultiplied_over(RgbaCanvas{canvas.width, canvas.height, canvas.stride_bytes, canvas.pixels.data()},
                           TitleRaster{0, 0, 1, 1, {0, 255, 0, 255}});
  assert(canvas.pixels[0] == 0 && canvas.pixels[1] == 255 && canvas.pixels[2] == 0);
  assert(canvas.pixels[4] == 255);
}
