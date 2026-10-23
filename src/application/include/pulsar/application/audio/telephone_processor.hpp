#pragma once

#include <pulsar/application/audio/audio_effect_processor.hpp>
#include <pulsar/domain/timeline/telephone.hpp>

namespace pulsar::application {

// Compact telephone-band processor: a one-pole high-pass followed by a
// one-pole low-pass and bounded soft saturation. It is intentionally pure
// application code so realtime preview and offline export share the exact path.
class TelephoneProcessor final : public AudioEffectProcessor {
public:
  TelephoneProcessor(domain::TelephoneSettings settings, int sample_rate, int channel_count);

  void reset() noexcept override;
  void process(AudioBlock& block, std::int64_t first_frame) noexcept override;
  void process_interleaved(float* samples, std::size_t frame_count,
                           std::int64_t first_frame) noexcept override;

private:
  domain::TelephoneSettings settings_;
  int sample_rate_;
  int channel_count_;
  float low_state_[2]{0.0F, 0.0F};
  float band_state_[2]{0.0F, 0.0F};
};

} // namespace pulsar::application
