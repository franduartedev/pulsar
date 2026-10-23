#include <pulsar/application/audio/underwater_processor.hpp>
#include <pulsar/application/audio/scalar_keyframe_evaluator.hpp>

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

UnderwaterProcessor::UnderwaterProcessor(domain::UnderwaterSettings settings, int sample_rate,
                                         int channel_count)
    : settings_{settings}, sample_rate_{sample_rate}, channel_count_{channel_count} {
  settings_.validate();
}

void UnderwaterProcessor::reset() noexcept {
  low_state_[0] = low_state_[1] = 0.0F;
  depth_keyframe_cursor_ = 0;
}

void UnderwaterProcessor::process(AudioBlock& block, std::int64_t first_frame) noexcept {
  process_interleaved(block.samples_mutable().data(), block.frame_count(), first_frame);
}

void UnderwaterProcessor::process_interleaved(float* samples, std::size_t frame_count,
                                              std::int64_t first_frame) noexcept {
  if (samples == nullptr || frame_count == 0 || sample_rate_ <= 0 ||
      (channel_count_ != 1 && channel_count_ != 2)) return;
  const float mix = static_cast<float>(settings_.mix);
  depth_keyframe_cursor_ = 0;
  for (std::size_t frame = 0; frame < frame_count; ++frame) {
    const auto absolute_frame = first_frame + static_cast<std::int64_t>(frame);
    const float depth = settings_.depth_keyframes.empty()
                            ? static_cast<float>(settings_.depth)
                            : evaluate_scalar_keyframes(
                                  settings_.depth_keyframes,
                                  static_cast<float>(absolute_frame) /
                                      static_cast<float>(sample_rate_),
                                  depth_keyframe_cursor_);
    const float motion = 1.0F + 0.08F * std::sin(
        static_cast<float>(absolute_frame) * 0.00019F);
    const float cutoff = std::clamp((2'400.0F - depth * 1'950.0F) * motion,
                                    250.0F, 2'600.0F);
    const float alpha = coefficient(cutoff, sample_rate_);
    for (int channel = 0; channel < channel_count_; ++channel) {
      const auto index = frame * static_cast<std::size_t>(channel_count_) + channel;
      const float input = std::isfinite(samples[index]) ? samples[index] : 0.0F;
      low_state_[channel] += alpha * (input - low_state_[channel]);
      samples[index] = input + (low_state_[channel] - input) * mix;
    }
  }
}

} // namespace pulsar::application
