#pragma once

#include <pulsar/application/export/offline_audio_renderer.hpp>

#include <functional>
#include <filesystem>

namespace pulsar::media::ffmpeg {

class FFmpegOfflineAudioSource final : public application::exporting::OfflineAudioSource {
public:
  FFmpegOfflineAudioSource(const std::filesystem::path& path, int audio_stream_index);
  ~FFmpegOfflineAudioSource() override;

  [[nodiscard]] int sample_rate() const noexcept override;
  [[nodiscard]] int channel_count() const noexcept override;
  [[nodiscard]] std::optional<domain::MediaTime> stream_origin() const override;
  void seek_to(domain::MediaTime stream_timestamp) override;
  [[nodiscard]] std::optional<application::exporting::OfflineAudioSourceBlock>
  decode_next() override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// Resolution of pulsar-sound:<id> and other logical locators belongs outside
// both the editorial renderer and this FFmpeg adapter. Production code can
// inject the platform/package resolver; tests can inject a deterministic one.
class FFmpegOfflineAudioSourceFactory final
    : public application::exporting::OfflineAudioSourceFactory {
public:
  using LocatorResolver = std::function<std::filesystem::path(const std::filesystem::path&)>;

  explicit FFmpegOfflineAudioSourceFactory(LocatorResolver resolver = {});

  [[nodiscard]] std::unique_ptr<application::exporting::OfflineAudioSource> open(
      const std::filesystem::path& logical_locator, int audio_stream_index) override;

private:
  LocatorResolver resolver_;
};

} // namespace pulsar::media::ffmpeg
