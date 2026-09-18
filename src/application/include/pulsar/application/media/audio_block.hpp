#pragma once

#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace pulsar::application {

// A1 canonical decoded-audio representation: owned 32-bit float PCM,
// interleaved (L R L R ... for stereo), at the source stream's native sample
// rate and channel count. No project mix rate, no device format, no timeline
// mapping. Timing, when present, is the block's own source presentation
// timestamp expressed in the audio stream's time base.
class AudioBlock {
public:
  AudioBlock(int sample_rate, int channel_count, std::size_t frame_count,
             std::vector<float> samples, domain::Rational time_base,
             std::optional<domain::MediaTime> presentation_timestamp);

  [[nodiscard]] int sample_rate() const noexcept { return sample_rate_; }
  [[nodiscard]] int channel_count() const noexcept { return channel_count_; }
  [[nodiscard]] std::size_t frame_count() const noexcept { return frame_count_; }
  [[nodiscard]] const std::vector<float>& samples() const noexcept { return samples_; }
  [[nodiscard]] const domain::Rational& time_base() const noexcept { return time_base_; }
  [[nodiscard]] const std::optional<domain::MediaTime>& presentation_timestamp() const noexcept {
    return presentation_timestamp_;
  }

private:
  int sample_rate_;
  int channel_count_;
  std::size_t frame_count_;
  std::vector<float> samples_;
  domain::Rational time_base_;
  std::optional<domain::MediaTime> presentation_timestamp_;
};

} // namespace pulsar::application
