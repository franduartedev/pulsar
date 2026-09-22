#pragma once

#include <pulsar/application/audio/audio_mix_session.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace pulsar::application {

struct AudioMixPlaybackMetrics {
  std::uint64_t render_count{0};
  std::uint64_t render_p50_us{0};
  std::uint64_t render_p95_us{0};
  std::uint64_t render_max_us{0};
  std::uint64_t clock_transitions{0};
  AudioMixMetrics mixer;
  AudioOutputMetrics output;
};

// Drives one AudioMixSession into one AudioOutput.  It deliberately owns no
// media or device implementation: source creation and device lifetime remain
// at their existing adapter boundaries.
class AudioMixPlaybackSession final {
public:
  AudioMixPlaybackSession(AudioOutput* output, AudioPlaybackSourceFactory source_factory);
  ~AudioMixPlaybackSession();

  AudioMixPlaybackSession(const AudioMixPlaybackSession&) = delete;
  AudioMixPlaybackSession& operator=(const AudioMixPlaybackSession&) = delete;

  void configure(ProjectSnapshot snapshot);
  void seek(domain::MediaTime timeline_position);
  void play();
  void pause();
  void update(domain::MediaTime timeline_position);
  void reset() noexcept;

  [[nodiscard]] AudioPlaybackState state() const noexcept { return state_; }
  [[nodiscard]] AudioPlaybackCondition condition() const noexcept { return condition_; }
  [[nodiscard]] const std::optional<std::string>& error_message() const noexcept {
    return error_message_;
  }
  [[nodiscard]] bool has_device_clock() const noexcept;
  [[nodiscard]] std::optional<domain::MediaTime> clock_position() const;
  [[nodiscard]] const AudioMixPlaybackMetrics& metrics() const noexcept { return metrics_; }

private:
  static constexpr std::uint64_t start_buffer_frames = 9'600;
  // The desktop update cadence and video presentation can consume more than
  // a 40/100 ms reserve. Keep refill below the bounded 250 ms cap while
  // leaving enough margin for a compressed-source decode burst.
  static constexpr std::uint64_t refill_buffer_frames = 9'600;
  static constexpr std::uint64_t maximum_buffer_frames = 12'000;

  [[nodiscard]] std::uint64_t buffered_device_frames() const;
  [[nodiscard]] bool fill_until(std::uint64_t target_frames);
  void close_output() noexcept;
  void record_render(std::uint64_t micros) noexcept;
  void refresh_metrics() noexcept;
  void set_condition(AudioPlaybackCondition condition) noexcept;

  AudioOutput* output_{nullptr};
  AudioMixSession mixer_;
  std::optional<ProjectSnapshot> snapshot_;
  std::optional<domain::MediaTime> render_position_;
  std::optional<domain::MediaTime> output_buffer_start_;
  domain::MediaTime audio_clock_anchor_{domain::MediaTime::zero()};
  std::uint64_t processed_anchor_{0};
  AudioPlaybackState state_{AudioPlaybackState::Inactive};
  AudioPlaybackCondition condition_{AudioPlaybackCondition::Inactive};
  std::optional<std::string> error_message_;
  bool requested_playing_{false};
  bool audio_ahead_{false};
  std::uint64_t last_usable_contributors_{0};
  std::uint64_t observed_underruns_{0};
  std::array<std::uint64_t, 128> render_samples_{};
  std::size_t render_sample_count_{0};
  std::size_t render_sample_cursor_{0};
  std::uint64_t generation_{0};
  AudioMixPlaybackMetrics metrics_;
};

} // namespace pulsar::application
