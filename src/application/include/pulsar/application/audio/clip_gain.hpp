#pragma once

#include <pulsar/application/media/audio_block.hpp>

namespace pulsar::application {

struct ClipGain {
  float linear{1.0F};
  bool muted{false};

  [[nodiscard]] static ClipGain from_decibels(double gain_db, bool muted);
};

// Applies the one scalar clip-amplitude stage to an exclusively owned block.
// Shape and timing metadata are unchanged. Finite headroom is retained here;
// the existing device conversion owns final format clipping.
void apply_clip_gain(AudioBlock& block, ClipGain gain) noexcept;

} // namespace pulsar::application
