#pragma once

#include <pulsar/application/audio/audio_effect_processor.hpp>
#include <pulsar/domain/timeline/underwater.hpp>

namespace pulsar::application {

// A low-pass submerged ambience with a very small moving cutoff. The
// processor is allocation-free and shared by realtime preview and export.
class UnderwaterProcessor final : public AudioEffectProcessor {
public:
  UnderwaterProcessor(domain::UnderwaterSettings settings, int sample_rate, int channel_count);

  void reset() noexcept override;
  void process(AudioBlock& block, std::int64_t first_frame) noexcept override;
  void process_interleaved(float* samples, std::size_t frame_count,
                           std::int64_t first_frame) noexcept override;

private:
  domain::UnderwaterSettings settings_;
  int sample_rate_;
  int channel_count_;
  float low_state_[2]{0.0F, 0.0F};
};

} // namespace pulsar::application
