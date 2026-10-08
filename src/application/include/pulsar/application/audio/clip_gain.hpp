#pragma once

#include <pulsar/application/media/audio_block.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <cstdint>

namespace pulsar::application {

struct ClipGain {
  float linear{1.0F};
  bool muted{false};

  [[nodiscard]] static ClipGain from_decibels(double gain_db, bool muted);
};

// Linear fade-in / fade-out of one clip on the output sample grid. Shared by the preview mixer and
// the exporter so both ramp identically. Frames are counted from the clip's first output frame.
struct ClipFade {
  std::int64_t in_frames{0};
  std::int64_t out_frames{0};
  std::int64_t length_frames{0};

  [[nodiscard]] bool active() const noexcept { return in_frames > 0 || out_frames > 0; }
  // Fades longer than the clip are limited to it (fade-in first, fade-out takes the rest).
  [[nodiscard]] static ClipFade from_times(domain::MediaTime fade_in, domain::MediaTime fade_out,
                                           domain::MediaTime clip_duration, int sample_rate);
  // 0 at the first frame of a fade-in, 1 once it is done; 1 at the start of a fade-out, near 0 at the end.
  [[nodiscard]] float gain_at(std::int64_t frame) const noexcept;
};

// Applies the one scalar clip-amplitude stage to an exclusively owned block.
// Shape and timing metadata are unchanged. Finite headroom is retained here;
// the existing device conversion owns final format clipping.
void apply_clip_gain(AudioBlock& block, ClipGain gain) noexcept;

} // namespace pulsar::application
