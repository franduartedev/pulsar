#include <pulsar/application/media/audio_block.hpp>

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace pulsar::application {

AudioBlock::AudioBlock(int sample_rate, int channel_count, std::size_t frame_count,
                       std::vector<float> samples, domain::Rational time_base,
                       std::optional<domain::MediaTime> presentation_timestamp)
    : sample_rate_{sample_rate},
      channel_count_{channel_count},
      frame_count_{frame_count},
      samples_{std::move(samples)},
      time_base_{time_base},
      presentation_timestamp_{std::move(presentation_timestamp)} {
  if (sample_rate_ <= 0) {
    throw std::invalid_argument{"audio block sample rate must be positive"};
  }
  if (channel_count_ <= 0) {
    throw std::invalid_argument{"audio block channel count must be positive"};
  }
  if (frame_count_ == 0) {
    throw std::invalid_argument{"audio block frame count must be positive"};
  }
  const auto expected =
      static_cast<std::uint64_t>(frame_count_) * static_cast<std::uint64_t>(channel_count_);
  if (expected != static_cast<std::uint64_t>(samples_.size())) {
    throw std::invalid_argument{"audio block samples must hold frame_count * channel_count floats"};
  }
  if (presentation_timestamp_.has_value() &&
      presentation_timestamp_->time_base() != time_base_) {
    throw std::invalid_argument{"audio block timestamp must use the block time base"};
  }
}

} // namespace pulsar::application
