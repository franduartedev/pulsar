#include <pulsar/application/audio/audio_effect_chain.hpp>

namespace pulsar::application {

AudioEffectChain::AudioEffectChain(std::unique_ptr<AudioEffectProcessor> first_effect)
    : effects_{} {
  add(std::move(first_effect));
}

AudioEffectChain AudioEffectChain::from_distant_music(
    const domain::DistantMusicSettings& settings, int sample_rate, int channel_count) {
  if (!settings.enabled) return {};
  return AudioEffectChain{std::make_unique<DistantMusicProcessor>(settings, sample_rate,
                                                                   channel_count)};
}

AudioEffectChain AudioEffectChain::from_telephone(const domain::TelephoneSettings& settings,
                                                  int sample_rate, int channel_count) {
  if (!settings.enabled) return {};
  return AudioEffectChain{std::make_unique<TelephoneProcessor>(settings, sample_rate,
                                                                channel_count)};
}

AudioEffectChain AudioEffectChain::from_clip_effects(
    const domain::DistantMusicSettings& distant_music,
    const domain::TelephoneSettings& telephone, const domain::UnderwaterSettings& underwater,
    const std::vector<domain::AudioEffectType>& order, int sample_rate, int channel_count) {
  const auto canonical = domain::canonical_audio_effect_order();
  const auto& effective_order = order.empty() ? canonical : order;
  AudioEffectChain chain;
  for (const auto type : effective_order) {
    switch (type) {
    case domain::AudioEffectType::DistantMusic:
      if (distant_music.enabled) {
        chain.add(std::make_unique<DistantMusicProcessor>(distant_music, sample_rate, channel_count));
      }
      break;
    case domain::AudioEffectType::Telephone:
      if (telephone.enabled) {
        chain.add(std::make_unique<TelephoneProcessor>(telephone, sample_rate, channel_count));
      }
      break;
    case domain::AudioEffectType::Underwater:
      if (underwater.enabled) {
        chain.add(std::make_unique<UnderwaterProcessor>(underwater, sample_rate, channel_count));
      }
      break;
    }
  }
  return chain;
}

void AudioEffectChain::reset() noexcept {
  for (auto& effect : effects_) effect->reset();
}

void AudioEffectChain::process(AudioBlock& block, std::int64_t first_frame) noexcept {
  for (auto& effect : effects_) effect->process(block, first_frame);
}

void AudioEffectChain::process_interleaved(float* samples, std::size_t frame_count,
                                           std::int64_t first_frame) noexcept {
  for (auto& effect : effects_) {
    effect->process_interleaved(samples, frame_count, first_frame);
  }
}

void AudioEffectChain::add(std::unique_ptr<AudioEffectProcessor> effect) {
  if (effect) effects_.push_back(std::move(effect));
}

} // namespace pulsar::application
