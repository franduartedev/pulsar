#include <pulsar/application/audio/pulse_map.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace pulsar::application {

PulseMap analyze_pulse_map(const AudioBlock& block, std::size_t window_frames,
                           float sensitivity) {
  PulseMap result{block.sample_rate(), block.channel_count(), window_frames, 0.0, {}};
  if (block.sample_rate() <= 0 || block.channel_count() <= 0 || block.frame_count() == 0 ||
      window_frames == 0 || !std::isfinite(sensitivity) || sensitivity <= 0.0F) {
    return result;
  }
  const auto channels = static_cast<std::size_t>(block.channel_count());
  const auto available_frames = std::min(block.frame_count(), block.samples().size() / channels);
  if (available_frames == 0) return result;
  window_frames = std::clamp<std::size_t>(window_frames, 32, 16'384);
  result.window_frames = window_frames;
  const auto window_count = (available_frames + window_frames - 1) / window_frames;
  constexpr std::size_t maximum_points = 100'000;
  if (window_count > maximum_points) {
    window_frames = (available_frames + maximum_points - 1) / maximum_points;
    result.window_frames = window_frames;
  }
  result.points.reserve(std::min(maximum_points, (available_frames + window_frames - 1) / window_frames));

  float previous_energy = 0.0F;
  double last_pulse = -std::numeric_limits<double>::infinity();
  for (std::size_t start = 0; start < available_frames; start += window_frames) {
    const auto frames = std::min(window_frames, available_frames - start);
    long double energy_sum = 0.0L;
    float peak = 0.0F;
    for (std::size_t frame = 0; frame < frames; ++frame) {
      for (std::size_t channel = 0; channel < channels; ++channel) {
        const auto sample = block.samples()[(start + frame) * channels + channel];
        if (!std::isfinite(sample)) continue;
        const auto magnitude = std::fabs(sample);
        peak = std::max(peak, magnitude);
        energy_sum += static_cast<long double>(sample) * static_cast<long double>(sample);
      }
    }
    const float energy = static_cast<float>(std::sqrt(
        energy_sum / static_cast<long double>(frames * channels)));
    const float transient = std::max(0.0F, energy - previous_energy);
    const double time = static_cast<double>(start) / static_cast<double>(block.sample_rate());
    const std::size_t history_start = result.points.size() > 8 ? result.points.size() - 8 : 0;
    float history_flux = 0.0F;
    for (std::size_t index = history_start; index < result.points.size(); ++index) {
      history_flux += result.points[index].transient;
    }
    const auto history_count = result.points.size() - history_start;
    const float average_flux = history_count == 0 ? 0.0F : history_flux /
                                                          static_cast<float>(history_count);
    const float threshold = std::max(0.015F, average_flux * sensitivity);
    const bool pulse = transient >= threshold && transient > 0.015F && time - last_pulse >= 0.18;
    if (pulse) last_pulse = time;
    result.points.push_back({time, energy, peak, transient, pulse});
    previous_energy = energy;
  }
  result.duration_seconds = static_cast<double>(available_frames) /
                            static_cast<double>(block.sample_rate());
  return result;
}

} // namespace pulsar::application
