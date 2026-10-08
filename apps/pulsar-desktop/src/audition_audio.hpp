#pragma once

// Pure helpers for the library's sound audition (no Qt): read a PCM WAV and turn it into the
// interleaved float samples of the output device, with a short lead-in of silence so a device that
// is waking from idle does not swallow a 100 ms click. Kept free of Qt so it is unit-tested.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace pulsar::desktop {

struct PcmWav {
  int sample_rate = 0;
  int channels = 0;
  std::vector<float> samples;   // interleaved, -1..1
  [[nodiscard]] std::size_t frames() const noexcept { return channels > 0 ? samples.size() / static_cast<std::size_t>(channels) : 0; }
};

namespace detail {
inline std::uint32_t le32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }
inline std::uint16_t le16(const unsigned char* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
} // namespace detail

// Parses a RIFF/WAVE file held in memory: integer PCM 8/16/24/32-bit or 32-bit float, mono or
// stereo up to 8 channels, any rate. Anything else (compressed, truncated, absurd) -> nullopt.
[[nodiscard]] inline std::optional<PcmWav> parse_pcm_wav(const std::vector<unsigned char>& bytes) {
  using detail::le16; using detail::le32;
  if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) return std::nullopt;
  int format_tag = 0, channels = 0, rate = 0, bits = 0;
  const unsigned char* data = nullptr;
  std::size_t data_size = 0;
  std::size_t pos = 12;
  while (pos + 8 <= bytes.size()) {
    const auto* chunk = bytes.data() + pos;
    const std::size_t size = le32(chunk + 4);
    const std::size_t body = pos + 8;
    if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16 && body + 16 <= bytes.size()) {
      format_tag = le16(bytes.data() + body);
      channels = le16(bytes.data() + body + 2);
      rate = static_cast<int>(le32(bytes.data() + body + 4));
      bits = le16(bytes.data() + body + 14);
      if (format_tag == 0xFFFE && size >= 26 && body + 26 <= bytes.size()) format_tag = le16(bytes.data() + body + 24);   // extensible: sub-format
    } else if (std::memcmp(chunk, "data", 4) == 0) {
      data = bytes.data() + body;
      data_size = std::min(size, bytes.size() - body);   // tolerate a header that overstates the length
      break;
    }
    pos = body + size + (size & 1U);
  }
  if (data == nullptr || channels < 1 || channels > 8 || rate < 8000 || rate > 384000) return std::nullopt;
  const bool is_float = format_tag == 3 && bits == 32;
  const bool is_int = format_tag == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
  if (!is_float && !is_int) return std::nullopt;
  const std::size_t bytes_per_sample = static_cast<std::size_t>(bits / 8);
  const std::size_t count = data_size / (bytes_per_sample * static_cast<std::size_t>(channels)) * static_cast<std::size_t>(channels);
  if (count == 0 || count > 50'000'000) return std::nullopt;
  PcmWav wav;
  wav.sample_rate = rate;
  wav.channels = channels;
  wav.samples.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    const unsigned char* p = data + i * bytes_per_sample;
    float value = 0.0F;
    if (is_float) { std::uint32_t raw = le32(p); float f; std::memcpy(&f, &raw, 4); value = std::isfinite(f) ? std::clamp(f, -1.0F, 1.0F) : 0.0F; }
    else if (bits == 8) value = (static_cast<int>(p[0]) - 128) / 128.0F;
    else if (bits == 16) value = static_cast<std::int16_t>(le16(p)) / 32768.0F;
    else if (bits == 24) { int v = p[0] | (p[1] << 8) | (p[2] << 16); if (v & 0x800000) v |= ~0xFFFFFF; value = static_cast<float>(v) / 8388608.0F; }
    else value = static_cast<float>(static_cast<std::int32_t>(le32(p)) / 2147483648.0);
    wav.samples[i] = value;
  }
  return wav;
}

[[nodiscard]] inline std::optional<PcmWav> read_pcm_wav(const std::filesystem::path& path) {
  std::ifstream file{path, std::ios::binary};
  if (!file) return std::nullopt;
  std::vector<unsigned char> bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
  return parse_pcm_wav(bytes);
}

// Interleaved float samples for a device of `device_rate` Hz and `device_channels`: mono is
// spread to every channel, extra source channels are dropped, the rate is converted linearly,
// and `lead_ms` / `tail_ms` of silence frame the sound.
[[nodiscard]] inline std::vector<float> prepare_audition_samples(const PcmWav& wav, int device_rate, int device_channels,
                                                                 int lead_ms, int tail_ms) {
  const std::size_t lead = static_cast<std::size_t>(device_rate) * static_cast<std::size_t>(lead_ms) / 1000U;
  const std::size_t tail = static_cast<std::size_t>(device_rate) * static_cast<std::size_t>(tail_ms) / 1000U;
  const std::size_t source_frames = wav.frames();
  const std::size_t body = source_frames == 0 ? 0 : std::max<std::size_t>(1, static_cast<std::size_t>(
      std::llround(static_cast<double>(source_frames) * device_rate / wav.sample_rate)));
  const std::size_t total = lead + body + tail;
  std::vector<float> out(total * static_cast<std::size_t>(device_channels), 0.0F);
  for (std::size_t n = 0; n < body; ++n) {
    const double position = static_cast<double>(n) * wav.sample_rate / device_rate;
    const std::size_t i0 = std::min(static_cast<std::size_t>(position), source_frames - 1);
    const std::size_t i1 = std::min(i0 + 1, source_frames - 1);
    const float t = static_cast<float>(position - static_cast<double>(i0));
    for (int c = 0; c < device_channels; ++c) {
      const int source_channel = wav.channels == 1 ? 0 : std::min(c, wav.channels - 1);
      const float a = wav.samples[i0 * static_cast<std::size_t>(wav.channels) + static_cast<std::size_t>(source_channel)];
      const float b = wav.samples[i1 * static_cast<std::size_t>(wav.channels) + static_cast<std::size_t>(source_channel)];
      out[(lead + n) * static_cast<std::size_t>(device_channels) + static_cast<std::size_t>(c)] = a + (b - a) * t;
    }
  }
  return out;
}

} // namespace pulsar::desktop
