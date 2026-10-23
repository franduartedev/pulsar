#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace pulsar::application {

// The one master output stage of the mix, used by BOTH the live preview mixer and the offline
// (export) renderer, so what you hear is what is exported.
//
// A stateless, allocation-free soft knee — NOT a look-ahead limiter:
//   |x| <= knee : untouched (bit-exact for any normal mix)
//   |x| >  knee : smoothly compressed towards, and never reaching, full scale (C1 continuous)
// Where the old policy chopped overshoots flat at 0 dBFS (and the exporter did not guard them
// at all), loud sums of music + effects now bend instead of clicking. Real peak limiting
// (attack/release, look-ahead) needs per-block state and is post-Beta.
inline constexpr float kOutputKnee = 0.9F;   // about -0.9 dBFS

struct AudioLevel {
  float peak{0.0F};
  float rms{0.0F};
  std::size_t clipped_samples{0};
};

[[nodiscard]] inline float soft_output_stage(float x) noexcept {
  if (!std::isfinite(x)) return 0.0F;
  const float magnitude = std::fabs(x);
  if (magnitude <= kOutputKnee) return x;
  const float room = 1.0F - kOutputKnee;
  const float u = (magnitude - kOutputKnee) / room;          // 0 at the knee
  const float shaped = kOutputKnee + room * (u / (1.0F + u));  // slope 1 at the knee, asymptote 1
  return x < 0.0F ? -shaped : shaped;
}

[[nodiscard]] inline AudioLevel measure_audio_level(const float* samples,
                                                    std::size_t sample_count) noexcept {
  if (samples == nullptr || sample_count == 0) return {};
  long double energy = 0.0L;
  float peak = 0.0F;
  std::size_t clipped = 0;
  for (std::size_t index = 0; index < sample_count; ++index) {
    const float sample = samples[index];
    if (!std::isfinite(sample)) continue;
    const float magnitude = std::fabs(sample);
    peak = std::max(peak, magnitude);
    energy += static_cast<long double>(sample) * static_cast<long double>(sample);
    if (magnitude >= 0.999F) ++clipped;
  }
  return {peak, static_cast<float>(std::sqrt(energy / static_cast<long double>(sample_count))),
          clipped};
}

} // namespace pulsar::application
