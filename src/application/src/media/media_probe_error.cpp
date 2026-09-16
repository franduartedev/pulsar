#include <pulsar/application/media/media_probe_error.hpp>

#include <utility>

namespace pulsar::application {

MediaProbeError::MediaProbeError(MediaProbeErrorCode code, std::string message)
    : std::runtime_error{std::move(message)}, code_{code} {}

} // namespace pulsar::application
