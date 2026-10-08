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

ClipFade ClipFade::from_times(domain::MediaTime fade_in, domain::MediaTime fade_out,
                              domain::MediaTime clip_duration, int sample_rate) {
  const domain::FrameRate grid{sample_rate, 1};
  ClipFade fade;
  fade.length_frames = domain::frame_at(clip_duration, grid, domain::RoundingMode::Ceil).value();
  fade.in_frames = std::clamp<std::int64_t>(domain::frame_at(fade_in, grid, domain::RoundingMode::Floor).value(), 0,
                                            fade.length_frames);
  fade.out_frames = std::clamp<std::int64_t>(domain::frame_at(fade_out, grid, domain::RoundingMode::Floor).value(), 0,
                                             fade.length_frames - fade.in_frames);
  return fade;
}

float ClipFade::gain_at(std::int64_t frame) const noexcept {
  float gain = 1.0F;
  if (in_frames > 0 && frame < in_frames) {
    gain = frame <= 0 ? 0.0F : static_cast<float>(frame) / static_cast<float>(in_frames);
  }
  if (out_frames > 0 && frame >= length_frames - out_frames) {
    const auto remaining = length_frames - frame;
    const float out = remaining <= 0 ? 0.0F : static_cast<float>(remaining) / static_cast<float>(out_frames);
    gain = std::min(gain, out);
  }
  return gain;
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
