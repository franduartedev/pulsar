#include <pulsar/application/audio/pulse_map.hpp>

#include <cassert>
#include <cmath>
#include <utility>
#include <vector>

int main() {
  constexpr int rate = 1'000;
  constexpr std::size_t frames = 1'000;
  std::vector<float> samples(frames * 2, 0.0F);
  for (std::size_t frame = 200; frame < 240; ++frame) {
    samples[frame * 2] = 1.0F;
    samples[frame * 2 + 1] = 1.0F;
  }
  for (std::size_t frame = 700; frame < 740; ++frame) {
    samples[frame * 2] = 0.8F;
    samples[frame * 2 + 1] = 0.8F;
  }
  pulsar::application::AudioBlock block{rate, 2, frames, std::move(samples), {1, rate}, std::nullopt};
  const auto map = pulsar::application::analyze_pulse_map(block, 100);
  assert(map.points.size() == 10);
  assert(map.duration_seconds == 1.0);
  assert(map.points[2].peak == 1.0F);
  assert(map.points[2].pulse);
  assert(map.points[7].pulse);
  const auto invalid = pulsar::application::analyze_pulse_map(block, 0);
  assert(invalid.points.empty());
  assert(std::isfinite(map.points.front().energy));
}
