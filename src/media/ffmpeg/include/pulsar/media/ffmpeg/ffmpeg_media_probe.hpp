#pragma once

#include <pulsar/application/media/media_probe.hpp>

namespace pulsar::media::ffmpeg {

class FFmpegMediaProbe final : public application::MediaProbe {
public:
  [[nodiscard]] application::MediaProbeResult
  probe(const std::filesystem::path& path) const override;
};

} // namespace pulsar::media::ffmpeg
