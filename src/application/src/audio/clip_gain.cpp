#include <pulsar/application/audio/clip_gain.hpp>
#include <pulsar/domain/timeline/clip.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace pulsar::application {

ClipGain ClipGain::from_decibels(double gain_db, bool muted) {
  if (!std::isfinite(gain_db) ||
      gain_db < domain::Clip::minimum_audio_gain_db ||
      gain_db > domain::Clip::maximum_audio_gain_db) {
    throw std::invalid_argument{"clip audio gain must be finite and within supported range"};
  }
  return ClipGain{static_cast<float>(std::pow(10.0, gain_db / 20.0)), muted};
}

void apply_clip_gain(AudioBlock& block, ClipGain gain) noexcept {
  auto& samples = block.samples_mutable();
  if (gain.muted) {
    std::fill(samples.begin(), samples.end(), 0.0F);
    return;
  }
  if (gain.linear == 1.0F) return;
  for (float& sample : samples) sample *= gain.linear;
}

} // namespace pulsar::application
