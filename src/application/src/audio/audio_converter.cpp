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

[[nodiscard]] std::size_t resampled_frame_count(std::size_t source_frames, int source_rate,
                                                int device_rate) {
  const auto count = (static_cast<std::uint64_t>(source_frames) *
                      static_cast<std::uint64_t>(device_rate)) /
                     static_cast<std::uint64_t>(source_rate);
  if (count == 0 || count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    throw AudioConversionError{"audio resampling produced an invalid frame count"};
  }
  return static_cast<std::size_t>(count);
}

[[nodiscard]] std::vector<float> resample_linear(const std::vector<float>& samples,
                                                 std::size_t source_frames, int channels,
                                                 int source_rate, int device_rate) {
  if (source_rate == device_rate) {
    return samples;
  }
  const std::size_t device_frames = resampled_frame_count(source_frames, source_rate, device_rate);
  std::vector<float> resampled(device_frames * static_cast<std::size_t>(channels));
  if (source_frames == 1) {
    for (std::size_t frame = 0; frame < device_frames; ++frame) {
      for (int channel = 0; channel < channels; ++channel) {
        resampled[(frame * static_cast<std::size_t>(channels)) +
                  static_cast<std::size_t>(channel)] = samples[static_cast<std::size_t>(channel)];
      }
    }
    return resampled;
  }
  for (std::size_t frame = 0; frame < device_frames; ++frame) {
    const double position =
        (static_cast<double>(frame) * source_rate) / static_cast<double>(device_rate);
    std::size_t base = static_cast<std::size_t>(position);
    if (base + 1 >= source_frames) {
      base = source_frames - 2;
    }
    const float fraction = static_cast<float>(position - static_cast<double>(base));
    for (int channel = 0; channel < channels; ++channel) {
      const float before = samples[(base * static_cast<std::size_t>(channels)) +
                                   static_cast<std::size_t>(channel)];
      const float after = samples[((base + 1) * static_cast<std::size_t>(channels)) +
                                  static_cast<std::size_t>(channel)];
      resampled[(frame * static_cast<std::size_t>(channels)) +
                static_cast<std::size_t>(channel)] = before + fraction * (after - before);
    }
  }
  return resampled;
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

} // namespace

ConvertedAudioBlock convert_block_for_device(const AudioBlock& source,
                                             const AudioDeviceFormat& device) {
  if (source.sample_rate() <= 0 || device.sample_rate <= 0) {
    throw AudioConversionError{"audio conversion requires positive sample rates"};
  }
  if (source.channel_count() <= 0 || device.channel_count <= 0) {
    throw AudioConversionError{"audio conversion requires positive channel counts"};
  }
  if (!channels_supported(source.channel_count(), device.channel_count)) {
    throw AudioConversionError{"unsupported audio channel mapping"};
  }

  std::vector<float> mapped =
      map_channels(source.samples(), source.frame_count(), source.channel_count(),
                   device.channel_count);
  std::vector<float> timed =
      resample_linear(mapped, source.frame_count(), device.channel_count,
                      source.sample_rate(), device.sample_rate);
  const std::size_t device_frames = timed.size() / static_cast<std::size_t>(device.channel_count);

  ConvertedAudioBlock converted;
  converted.format = device;
  converted.frame_count = device_frames;
  switch (device.sample_format) {
  case AudioSampleFormat::Float32:
    pack_float32(timed, converted.frames);
    break;
  case AudioSampleFormat::Int16:
    pack_int16(timed, converted.frames);
    break;
  case AudioSampleFormat::UInt8:
    pack_uint8(timed, converted.frames);
    break;
  }
  return converted;
}

} // namespace pulsar::application
