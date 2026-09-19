#pragma once

#include <pulsar/application/audio/audio_output.hpp>
#include <pulsar/application/media/audio_block.hpp>
#include <pulsar/application/timeline/timeline_clip_resolver.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <filesystem>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace pulsar::application {

// Application-owned source seam. Media adapters may recreate a source for a
// seek; no FFmpeg or device type crosses this boundary.
class AudioPlaybackSource {
public:
  virtual ~AudioPlaybackSource() = default;
  [[nodiscard]] virtual int sample_rate() const noexcept = 0;
  [[nodiscard]] virtual int channel_count() const noexcept = 0;
  // Raw stream timestamp corresponding to presentation-relative source zero,
  // when the media adapter can provide it. The fallback is zero.
  [[nodiscard]] virtual std::optional<domain::MediaTime> stream_origin() const {
    return std::nullopt;
  }
  [[nodiscard]] virtual bool seek_to(domain::MediaTime) { return false; }
  [[nodiscard]] virtual std::optional<AudioBlock> decode_next() = 0;
};

using AudioPlaybackSourceFactory = std::function<std::unique_ptr<AudioPlaybackSource>(
    const std::filesystem::path&, int)>;

enum class AudioPlaybackState {
  Inactive,
  Preparing,
  Ready,
  Running,
  Paused,
  IntentionalSilence,
  Starved,
  EndedEarly,
  DeviceUnavailable,
  Error,
};

// One selected clip's audio pipeline. It owns decoder state and queue policy,
// while the caller owns the AudioOutput and controls its lifetime.
class AudioPlaybackSession final {
public:
  AudioPlaybackSession(AudioOutput* output, AudioPlaybackSourceFactory source_factory);
  ~AudioPlaybackSession();

  AudioPlaybackSession(const AudioPlaybackSession&) = delete;
  AudioPlaybackSession& operator=(const AudioPlaybackSession&) = delete;

  void configure(const ResolvedClip& clip, domain::MediaTime timeline_position);
  void seek(domain::MediaTime timeline_position);
  void play();
  void pause();
  void update(domain::MediaTime timeline_position);
  void reset();

  [[nodiscard]] AudioPlaybackState state() const noexcept { return state_; }
  [[nodiscard]] bool has_device_clock() const noexcept;
  [[nodiscard]] std::optional<domain::MediaTime> clock_position() const;
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

private:
  [[nodiscard]] bool open_source_at(domain::MediaTime timeline_position);
  [[nodiscard]] bool prepare_audio_at(domain::MediaTime timeline_position);
  [[nodiscard]] bool enqueue_block(const AudioBlock& block);
  [[nodiscard]] bool enqueue_silence(std::size_t frames);
  [[nodiscard]] bool fill_until(std::uint64_t target_device_frames);
  [[nodiscard]] std::optional<AudioBlock> align_block(AudioBlock block);
  [[nodiscard]] domain::MediaTime audio_target_for(domain::MediaTime timeline_position) const;
  [[nodiscard]] std::size_t source_frames_for(domain::MediaTime duration) const;
  [[nodiscard]] std::uint64_t buffered_device_frames() const;
  void close_output();
  void set_error();

  AudioOutput* output_{nullptr};
  AudioPlaybackSourceFactory source_factory_;
  std::unique_ptr<AudioPlaybackSource> source_;
  std::optional<ResolvedClip> clip_;
  std::optional<AudioBlock> pending_block_;
  std::size_t pending_silence_frames_{0};
  domain::MediaTime configured_timeline_{domain::MediaTime::zero()};
  domain::MediaTime audio_clock_anchor_{domain::MediaTime::zero()};
  std::uint64_t processed_anchor_{0};
  std::uint64_t observed_underruns_{0};
  std::uint64_t generation_{0};
  bool source_eof_{false};
  std::optional<domain::MediaTime> stream_origin_;
  bool drain_requested_{false};
  AudioPlaybackState state_{AudioPlaybackState::Inactive};
};

} // namespace pulsar::application
