#pragma once

#include <stdexcept>
#include <string>

namespace pulsar::application {

enum class MediaProbeErrorCode {
  FileNotFound,
  NotRegularFile,
  RemoteResourceNotAllowed,
  UnsupportedMedia,
  ProbeFailed,
};

class MediaProbeError : public std::runtime_error {
public:
  MediaProbeError(MediaProbeErrorCode code, std::string message);

  [[nodiscard]] constexpr MediaProbeErrorCode code() const noexcept {
    return code_;
  }

private:
  MediaProbeErrorCode code_;
};

} // namespace pulsar::application
