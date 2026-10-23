#include <pulsar/application/audio/distant_music_processor.hpp>
#include <pulsar/application/audio/audio_effect_chain.hpp>
#include <pulsar/application/audio/output_stage.hpp>
#include <pulsar/application/audio/telephone_processor.hpp>

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace pulsar;

int main() {
  domain::DistantMusicSettings settings;
  settings.enabled = true;
  settings.initial_gain_db = -30.0;
  settings.duration_seconds = 1.0;
  settings.start_cutoff_hz = 400.0;
  settings.end_cutoff_hz = 18'000.0;
  settings.reverb_amount = 0.2;

  constexpr int rate = 48'000;
  std::vector<float> samples(4'800 * 2, 1.0F);
  application::AudioBlock block{rate, 2, 4'800, std::move(samples), {1, rate}, std::nullopt};
  application::DistantMusicProcessor processor{settings, rate, 2};
  processor.process(block, 0);

  assert(block.samples().front() < 0.1F);
  assert(block.samples().back() > block.samples().front());

  processor.reset();
  const auto before = block.samples().front();
  processor.process(block, 48'000);
  assert(block.samples().front() > before);

  processor.reset();
  float raw[4] = {1.0F, 1.0F, 1.0F, 1.0F};
  processor.process_interleaved(raw, 2, 0);
  assert(raw[0] < 0.1F && raw[2] > raw[0]);

  auto chain = application::AudioEffectChain::from_distant_music(settings, rate, 2);
  assert(!chain.empty());
  assert(chain.size() == 1);
  chain.process_interleaved(raw, 2, 0);
  chain.reset();
  assert(!chain.empty());

  settings.enabled = false;
  auto empty_chain = application::AudioEffectChain::from_distant_music(settings, rate, 2);
  assert(empty_chain.empty());

  const float measured_samples[] = {0.5F, -1.0F, 0.25F, 0.0F};
  const auto level = application::measure_audio_level(measured_samples, 4);
  assert(level.peak == 1.0F && level.clipped_samples == 1);

  settings.enabled = true;
  settings.gain_keyframes = {{0.0, -40.0, domain::KeyframeInterpolation::Hold},
                             {1.0, 0.0, domain::KeyframeInterpolation::Linear}};
  application::DistantMusicProcessor automated{settings, rate, 2};
  float automated_samples[2] = {1.0F, 1.0F};
  automated.process_interleaved(automated_samples, 1, 0);
  assert(automated_samples[0] < 0.02F);
  automated.reset();
  automated.process_interleaved(automated_samples, 1, rate);
  assert(automated_samples[0] > 0.5F);

  settings.reverb_amount = 2.0;
  bool rejected = false;
  try {
    application::DistantMusicProcessor invalid{settings, rate, 2};
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  domain::TelephoneSettings telephone_settings;
  telephone_settings.enabled = true;
  telephone_settings.mix = 1.0;
  telephone_settings.drive = 0.2;
  application::TelephoneProcessor telephone{telephone_settings, rate, 2};
  float telephone_samples[8] = {1.0F, -1.0F, 0.5F, -0.5F, 0.25F, -0.25F, 0.1F, -0.1F};
  telephone.process_interleaved(telephone_samples, 4, 0);
  for (const float sample : telephone_samples) assert(std::isfinite(sample));
  telephone.reset();
  auto telephone_chain = application::AudioEffectChain::from_telephone(telephone_settings, rate, 2);
  assert(telephone_chain.size() == 1);
  settings.enabled = true;
  telephone_settings.enabled = true;
  auto clip_chain = application::AudioEffectChain::from_clip_effects(
      settings, telephone_settings, rate, 2);
  assert(clip_chain.size() == 2);
  settings.enabled = false;
  telephone_settings.enabled = false;
  assert(application::AudioEffectChain::from_clip_effects(
             settings, telephone_settings, rate, 2).empty());
  telephone_settings.mix = 2.0;
  bool telephone_rejected = false;
  try { application::TelephoneProcessor invalid_telephone{telephone_settings, rate, 2}; }
  catch (const std::invalid_argument&) { telephone_rejected = true; }
  assert(telephone_rejected);
  return 0;
}
