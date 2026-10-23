#pragma once

#include <pulsar/application/media/audio_block.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pulsar::application {

struct PulseMapPoint {
  double time_seconds{0.0};
  float energy{0.0F};
  float peak{0.0F};
  float transient{0.0F};
  bool pulse{false};
};

struct PulseMap {
  int sample_rate{0};
  int channel_count{0};
  std::size_t window_frames{0};
  double duration_seconds{0.0};
  std::vector<PulseMapPoint> points;
};

// Deterministic, bounded audio analysis for Pulse Map and future Scene Cards.
// It uses energy rise (not a guessed BPM) so it remains useful for speech,
// impacts and cinematic sound design as well as musical material.
[[nodiscard]] PulseMap analyze_pulse_map(const AudioBlock& block,
                                         std::size_t window_frames = 1024,
                                         float sensitivity = 1.35F);

} // namespace pulsar::application
