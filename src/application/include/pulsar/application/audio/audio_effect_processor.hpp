#pragma once

#include <pulsar/application/media/audio_block.hpp>

#include <cstddef>
#include <cstdint>

namespace pulsar::application {

// Stateless contract at the call site: processors own only bounded filter state,
// never allocate while processing, and reset deterministically after a seek.
class AudioEffectProcessor {
public:
  virtual ~AudioEffectProcessor() = default;
  virtual void reset() noexcept = 0;
  virtual void process(AudioBlock& block, std::int64_t first_frame) noexcept = 0;
  virtual void process_interleaved(float* samples, std::size_t frame_count,
                                   std::int64_t first_frame) noexcept = 0;
};

} // namespace pulsar::application
