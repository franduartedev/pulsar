#include <pulsar/application/waveform/waveform_cache.hpp>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

namespace {
class SyntheticSource final : public pulsar::application::WaveformSource {
public:
  explicit SyntheticSource(std::uint64_t frames) : frames_{frames} {}
  pulsar::application::WaveformSourceInfo info() const override {
    return {48000, 2, pulsar::domain::Rational{1, 48000}, pulsar::domain::MediaTime::zero()};
  }
  std::optional<pulsar::application::AudioBlock> next_block() override {
    if (position_ == frames_) return std::nullopt;
    const auto count = std::min<std::uint64_t>(4096, frames_ - position_);
    std::vector<float> samples(static_cast<std::size_t>(count * 2));
    for (std::uint64_t frame = 0; frame < count; ++frame) {
      const auto phase = static_cast<float>((position_ + frame) % 48000) / 48000.0f;
      samples[frame * 2] = phase * 2.0f - 1.0f;
      samples[frame * 2 + 1] = 0.25f - phase * 0.5f;
    }
    position_ += count;
    return pulsar::application::AudioBlock{48000, 2, static_cast<std::size_t>(count), std::move(samples), pulsar::domain::Rational{1, 48000}, std::nullopt};
  }
private:
  std::uint64_t frames_; std::uint64_t position_{0};
};
}

int main() {
  const auto directory = std::filesystem::temp_directory_path() / "pulsar-a7-benchmark";
  std::filesystem::create_directories(directory);
  for (const auto minutes : {1, 10}) {
    const auto frames = static_cast<std::uint64_t>(minutes) * 60U * 48000U;
    const auto path = directory / (std::to_string(minutes) + "m.pwfc");
    const auto key = pulsar::application::WaveformCacheKey{"benchmark-" + std::to_string(minutes), 0, 48000, 2, pulsar::domain::Rational{1, 48000}, pulsar::domain::MediaTime::zero()};
    SyntheticSource source{frames};
    const auto start = std::chrono::steady_clock::now();
    const auto report = pulsar::application::generate_waveform_cache(source, key, path);
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::cout << minutes << " min: " << elapsed << " s, " << (static_cast<double>(frames) / elapsed) << " samples/s, " << std::filesystem::file_size(path) << " bytes, peak input " << report.peak_input_frames << " frames\n";
    std::filesystem::remove(path);
  }
  std::filesystem::remove_all(directory);
}
