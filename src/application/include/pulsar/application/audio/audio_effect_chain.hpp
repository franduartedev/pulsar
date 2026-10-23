#pragma once

#include <pulsar/application/audio/distant_music_processor.hpp>
#include <pulsar/application/audio/telephone_processor.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace pulsar::application {

// Stateful effect chain shared by realtime preview and offline export. Effects
// are deliberately owned by the chain so both paths keep identical ordering
// and reset semantics as new processors are added.
class AudioEffectChain final {
public:
  AudioEffectChain() = default;
  explicit AudioEffectChain(std::unique_ptr<AudioEffectProcessor> first_effect);

  AudioEffectChain(AudioEffectChain&&) noexcept = default;
  AudioEffectChain& operator=(AudioEffectChain&&) noexcept = default;
  AudioEffectChain(const AudioEffectChain&) = delete;
  AudioEffectChain& operator=(const AudioEffectChain&) = delete;
  ~AudioEffectChain() = default;

  static AudioEffectChain from_distant_music(const domain::DistantMusicSettings& settings,
                                             int sample_rate, int channel_count);
  static AudioEffectChain from_telephone(const domain::TelephoneSettings& settings,
                                         int sample_rate, int channel_count);

  [[nodiscard]] bool empty() const noexcept { return effects_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return effects_.size(); }
  void add(std::unique_ptr<AudioEffectProcessor> effect);
  void reset() noexcept;
  void process(AudioBlock& block, std::int64_t first_frame) noexcept;
  void process_interleaved(float* samples, std::size_t frame_count,
                           std::int64_t first_frame) noexcept;

private:
  std::vector<std::unique_ptr<AudioEffectProcessor>> effects_;
};

} // namespace pulsar::application
