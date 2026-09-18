#include <pulsar/application/audio/audio_converter.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace pulsar::application {
namespace {

[[nodiscard]] bool channels_supported(int source_channels, int device_channels) {
  if (source_channels == device_channels) {
    return true;
  }
  return (source_channels == 1 || source_channels == 2) &&
         (device_channels == 1 || device_channels == 2);
}

[[nodiscard]] std::vector<float> map_channels(const std::vector<float>& samples,
                                              std::size_t frame_count, int source_channels,
                                              int device_channels) {
  if (source_channels == device_channels) {
    return samples;
  }
  std::vector<float> mapped;
  mapped.reserve(frame_count * static_cast<std::size_t>(device_channels));
  if (source_channels == 1 && device_channels == 2) {
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
      mapped.push_back(samples[frame]);
      mapped.push_back(samples[frame]);
    }
    return mapped;
  }
  if (source_channels == 2 && device_channels == 1) {
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
      mapped.push_back(0.5F * (samples[frame * 2] + samples[frame * 2 + 1]));
    }
    return mapped;
  }
  throw AudioConversionError{"unsupported audio channel mapping"};
}

void pack_float32(const std::vector<float>& samples, std::vector<std::uint8_t>& out) {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(samples.data());
  out.assign(bytes, bytes + samples.size() * sizeof(float));
}

[[nodiscard]] std::int16_t quantize_int16(float sample) {
  const float clamped = sample < -1.0F ? -1.0F : (sample > 1.0F ? 1.0F : sample);
  const long rounded = std::lround(clamped * 32768.0F);
  if (rounded < -32768) {
    return -32768;
  }
  if (rounded > 32767) {
    return 32767;
  }
  return static_cast<std::int16_t>(rounded);
}

void pack_int16(const std::vector<float>& samples, std::vector<std::uint8_t>& out) {
  out.reserve(samples.size() * 2);
  for (float sample : samples) {
    const std::int16_t value = quantize_int16(sample);
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  }
}

void pack_uint8(const std::vector<float>& samples, std::vector<std::uint8_t>& out) {
  out.reserve(samples.size());
  for (float sample : samples) {
    const float clamped = sample < -1.0F ? -1.0F : (sample > 1.0F ? 1.0F : sample);
    const long rounded = std::lround((clamped * 0.5F + 0.5F) * 255.0F);
    const long limited = rounded < 0 ? 0 : (rounded > 255 ? 255 : rounded);
    out.push_back(static_cast<std::uint8_t>(limited));
  }
}

void pack_samples(const std::vector<float>& samples, AudioSampleFormat format,
                  std::vector<std::uint8_t>& out) {
  switch (format) {
  case AudioSampleFormat::Float32:
    pack_float32(samples, out);
    break;
  case AudioSampleFormat::Int16:
    pack_int16(samples, out);
    break;
  case AudioSampleFormat::UInt8:
    pack_uint8(samples, out);
    break;
  }
}

} // namespace

StreamingAudioConverter::StreamingAudioConverter(int source_rate, int source_channels,
                                                 AudioDeviceFormat device)
    : source_rate_{source_rate}, source_channels_{source_channels}, device_{device} {
  if (source_rate_ <= 0 || device_.sample_rate <= 0) {
    throw AudioConversionError{"audio conversion requires positive sample rates"};
  }
  if (source_channels_ <= 0 || device_.channel_count <= 0) {
    throw AudioConversionError{"audio conversion requires positive channel counts"};
  }
  if (!channels_supported(source_channels_, device_.channel_count)) {
    throw AudioConversionError{"unsupported audio channel mapping"};
  }
}

ConvertedAudioBlock StreamingAudioConverter::convert(const AudioBlock& block) {
  if (block.sample_rate() != source_rate_ || block.channel_count() != source_channels_) {
    throw AudioConversionError{"audio block does not match converter source format"};
  }
  const std::size_t channels = static_cast<std::size_t>(device_.channel_count);
  const std::size_t source_frames = block.frame_count();
  const std::int64_t block_start = absolute_frames_;
  if (!reservoir_.empty() &&
      reservoir_base_ + static_cast<std::int64_t>(reservoir_.size() / channels) != block_start) {
    throw AudioConversionError{"audio blocks are not contiguous"};
  }
  const std::vector<float> mapped =
      map_channels(block.samples(), source_frames, source_channels_, device_.channel_count);
  if (reservoir_.empty()) {
    reservoir_base_ = block_start;
  }
  reservoir_.insert(reservoir_.end(), mapped.begin(), mapped.end());
  absolute_frames_ += static_cast<std::int64_t>(source_frames);

  // Streaming lattice in absolute source coordinates: identical grids for
  // split and contiguous input. A point is emitted once its successor
  // sample exists (interpolation history), so trailing points wait for the
  // next block or the drain flush.
  const double step = static_cast<double>(source_rate_) / device_.sample_rate;
  const double block_end = static_cast<double>(block_start + static_cast<std::int64_t>(source_frames));
  std::vector<float> resampled;
  while (next_position_ < block_end - 1.0) {
    const std::int64_t base = static_cast<std::int64_t>(std::floor(next_position_));
    const float fraction =
        static_cast<float>(next_position_ - static_cast<double>(base));
    const std::size_t base_offset =
        static_cast<std::size_t>(base - reservoir_base_) * channels;
    for (std::size_t channel = 0; channel < channels; ++channel) {
      resampled.push_back(reservoir_[base_offset + channel] * (1.0F - fraction) +
                          reservoir_[base_offset + channels + channel] * fraction);
    }
    next_position_ += step;
  }
  // Keep only samples a future point may still reference.
  const std::int64_t keep_from = static_cast<std::int64_t>(std::floor(next_position_)) - 1;
  if (keep_from > reservoir_base_) {
    const std::size_t drop =
        static_cast<std::size_t>(keep_from - reservoir_base_) * channels;
    if (drop >= reservoir_.size()) {
      reservoir_.clear();
    } else {
      reservoir_.erase(reservoir_.begin(), reservoir_.begin() + static_cast<std::ptrdiff_t>(drop));
    }
    reservoir_base_ = keep_from;
  }

  ConvertedAudioBlock converted;
  converted.format = device_;
  converted.frame_count = resampled.size() / channels;
  pack_samples(resampled, device_.sample_format, converted.frames);
  return converted;
}

ConvertedAudioBlock StreamingAudioConverter::flush_tail() const {
  ConvertedAudioBlock tail;
  tail.format = device_;
  if (reservoir_.empty()) {
    return tail;
  }
  const std::size_t channels = static_cast<std::size_t>(device_.channel_count);
  const double step = static_cast<double>(source_rate_) / device_.sample_rate;
  const double stream_end = static_cast<double>(absolute_frames_);
  std::vector<float> resampled;
  double position = next_position_;
  const std::size_t reservoir_frames = reservoir_.size() / channels;
  while (position < stream_end) {
    std::int64_t base = static_cast<std::int64_t>(std::floor(position));
    const float fraction = static_cast<float>(position - static_cast<double>(base));
    std::size_t base_index = base - reservoir_base_ >= 0
                                 ? static_cast<std::size_t>(base - reservoir_base_)
                                 : 0;
    std::size_t after_index = base_index + 1;
    if (after_index >= reservoir_frames) {
      after_index = reservoir_frames - 1;
    }
    if (base_index >= reservoir_frames) {
      break;
    }
    for (std::size_t channel = 0; channel < channels; ++channel) {
      resampled.push_back(reservoir_[base_index * channels + channel] * (1.0F - fraction) +
                          reservoir_[after_index * channels + channel] * fraction);
    }
    position += step;
  }
  tail.frame_count = resampled.size() / channels;
  pack_samples(resampled, device_.sample_format, tail.frames);
  return tail;
}

void StreamingAudioConverter::reset() noexcept {
  next_position_ = 0.0;
  reservoir_.clear();
  reservoir_base_ = 0;
  absolute_frames_ = 0;
}

bool StreamingAudioConverter::can_convert(int source_channels) const noexcept {
  return channels_supported(source_channels, device_.channel_count);
}

} // namespace pulsar::application
