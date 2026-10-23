#include <pulsar/application/audio/telephone_processor.hpp>

#include <algorithm>
#include <cmath>

namespace pulsar::application {
namespace {

float coefficient(float cutoff, int sample_rate) noexcept {
  if (sample_rate <= 0) return 1.0F;
  return std::clamp(1.0F - std::exp(-2.0F * 3.14159265358979323846F * cutoff /
                    static_cast<float>(sample_rate)), 0.001F, 1.0F);
}

} // namespace

TelephoneProcessor::TelephoneProcessor(domain::TelephoneSettings settings, int sample_rate,
                                       int channel_count)
    : settings_{settings}, sample_rate_{sample_rate}, channel_count_{channel_count} {
  settings_.validate();
}

void TelephoneProcessor::reset() noexcept {
  low_state_[0] = low_state_[1] = 0.0F;
  band_state_[0] = band_state_[1] = 0.0F;
}

void TelephoneProcessor::process(AudioBlock& block, std::int64_t first_frame) noexcept {
  process_interleaved(block.samples_mutable().data(), block.frame_count(), first_frame);
}

void TelephoneProcessor::process_interleaved(float* samples, std::size_t frame_count,
                                             std::int64_t) noexcept {
  if (samples == nullptr || frame_count == 0 || sample_rate_ <= 0 ||
      (channel_count_ != 1 && channel_count_ != 2)) return;
  const auto channels = channel_count_;
  const float low_alpha = coefficient(3400.0F, sample_rate_);
  const float high_alpha = coefficient(300.0F, sample_rate_);
  const float mix = static_cast<float>(settings_.mix);
  const float drive = 1.0F + static_cast<float>(settings_.drive) * 7.0F;
  for (std::size_t frame = 0; frame < frame_count; ++frame) {
    for (int channel = 0; channel < channels; ++channel) {
      const auto index = frame * static_cast<std::size_t>(channel_count_) + channel;
      const float input = std::isfinite(samples[index]) ? samples[index] : 0.0F;
      low_state_[channel] += high_alpha * (input - low_state_[channel]);
      const float high_pass = input - low_state_[channel];
      band_state_[channel] += low_alpha * (high_pass - band_state_[channel]);
      const float shaped = std::tanh(band_state_[channel] * drive) / std::tanh(drive);
      samples[index] = input + (shaped - input) * mix;
    }
  }
}

} // namespace pulsar::application
