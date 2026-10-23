#include <pulsar/application/audio/distant_music_processor.hpp>
#include <pulsar/application/audio/scalar_keyframe_evaluator.hpp>

#include <algorithm>
#include <cmath>

namespace pulsar::application {
namespace {

float curve_value(domain::DistantMusicCurve curve, float progress) noexcept {
  progress = std::clamp(progress, 0.0F, 1.0F);
  switch (curve) {
  case domain::DistantMusicCurve::Smooth:
    return progress * progress * (3.0F - 2.0F * progress);
  case domain::DistantMusicCurve::Explosive:
    return std::sqrt(progress);
  case domain::DistantMusicCurve::Progressive:
    return progress * progress;
  }
  return progress;
}

} // namespace

DistantMusicProcessor::DistantMusicProcessor(domain::DistantMusicSettings settings, int sample_rate,
                                             int channel_count)
    : settings_{settings}, sample_rate_{sample_rate}, channel_count_{channel_count} {
  settings_.validate();
}

void DistantMusicProcessor::reset() noexcept {
  filter_state_[0] = filter_state_[1] = 0.0F;
  reverb_state_[0] = reverb_state_[1] = 0.0F;
}

void DistantMusicProcessor::process(AudioBlock& block, std::int64_t first_frame) noexcept {
  process_interleaved(block.samples_mutable().data(), block.frame_count(), first_frame);
}

void DistantMusicProcessor::process_interleaved(float* samples, std::size_t frame_count,
                                                std::int64_t first_frame) noexcept {
  if (samples == nullptr) return;
  if (!settings_.enabled || sample_rate_ <= 0 || (channel_count_ != 1 && channel_count_ != 2)) return;

  const auto duration_frames = std::max<std::int64_t>(1, static_cast<std::int64_t>(
      std::llround(settings_.duration_seconds * static_cast<double>(sample_rate_))));
  const float initial_gain = std::pow(10.0F, static_cast<float>(settings_.initial_gain_db / 20.0));
  const float tau = 1.0F / static_cast<float>(sample_rate_);
  const float two_pi = 6.2831853071795864769F;
  std::size_t keyframe_cursor = 0;

  for (std::size_t frame = 0; frame < frame_count; ++frame) {
    const auto absolute = first_frame + static_cast<std::int64_t>(frame);
    const float progress = static_cast<float>(std::clamp<std::int64_t>(absolute, 0, duration_frames)) /
                           static_cast<float>(duration_frames);
    const float shaped = curve_value(settings_.curve, progress);
    const float gain = settings_.gain_keyframes.empty()
                           ? initial_gain + (1.0F - initial_gain) * shaped
                           : std::pow(10.0F, evaluate_scalar_keyframes(
                                                   settings_.gain_keyframes,
                                                   static_cast<float>(absolute) /
                                                       static_cast<float>(sample_rate_),
                                                   keyframe_cursor) /
                                                       20.0F);
    const float cutoff = static_cast<float>(settings_.start_cutoff_hz +
        (settings_.end_cutoff_hz - settings_.start_cutoff_hz) * shaped);
    const float alpha = std::clamp(two_pi * cutoff * tau / (1.0F + two_pi * cutoff * tau), 0.001F, 1.0F);

    for (int channel = 0; channel < channel_count_; ++channel) {
      const auto index = frame * static_cast<std::size_t>(channel_count_) + static_cast<std::size_t>(channel);
      const float filtered = filter_state_[channel] + alpha * (samples[index] - filter_state_[channel]);
      filter_state_[channel] = filtered;
      const float reverberated = reverb_state_[channel] * 0.82F + filtered * 0.18F;
      reverb_state_[channel] = reverberated;
      samples[index] = gain * (filtered * (1.0F - static_cast<float>(settings_.reverb_amount)) +
                               reverberated * static_cast<float>(settings_.reverb_amount));
    }
  }
}

} // namespace pulsar::application
