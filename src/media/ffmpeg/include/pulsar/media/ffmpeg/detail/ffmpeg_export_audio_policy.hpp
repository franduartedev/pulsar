#pragma once

#include <algorithm>

namespace pulsar::media::ffmpeg::detail {

// E4 policy: clip before AAC, with no limiter, normalization, or AGC.
[[nodiscard]] constexpr float hard_clip_audio_sample(float sample) noexcept {
  return std::clamp(sample, -1.0F, 1.0F);
}

} // namespace pulsar::media::ffmpeg::detail
