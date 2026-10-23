#pragma once

#include <pulsar/application/media/audio_block.hpp>
#include <pulsar/domain/timeline/distant_music.hpp>

#include <cstdint>
#include <cstddef>

namespace pulsar::application {

class AudioEffectProcessor {
public:
  virtual ~AudioEffectProcessor() = default;

  virtual void reset() noexcept = 0;
  virtual void process(AudioBlock& block, std::int64_t first_frame) noexcept = 0;
  virtual void process_interleaved(float* samples, std::size_t frame_count,
                                   std::int64_t first_frame) noexcept = 0;
};

// Stateful block processor shared by preview and offline rendering. The caller
// resets it whenever playback seeks or a new contributor becomes active.
class DistantMusicProcessor final : public AudioEffectProcessor {
public:
  explicit DistantMusicProcessor(domain::DistantMusicSettings settings, int sample_rate,
                                 int channel_count);

  void reset() noexcept override;
  void process(AudioBlock& block, std::int64_t first_frame) noexcept override;
  void process_interleaved(float* samples, std::size_t frame_count,
                           std::int64_t first_frame) noexcept override;

private:
  domain::DistantMusicSettings settings_;
  int sample_rate_;
  int channel_count_;
  float filter_state_[2]{0.0F, 0.0F};
  float reverb_state_[2]{0.0F, 0.0F};
};

} // namespace pulsar::application
