#pragma once

#include <pulsar/application/media/media_probe_result.hpp>

#include <filesystem>

namespace pulsar::application {

class MediaProbe {
public:
  virtual ~MediaProbe() = default;

  [[nodiscard]] virtual MediaProbeResult probe(const std::filesystem::path& path) const = 0;
};

} // namespace pulsar::application
