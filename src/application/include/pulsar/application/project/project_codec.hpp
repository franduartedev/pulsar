#pragma once

#include <pulsar/application/project/project_restore_state.hpp>
#include <pulsar/application/project/project_snapshot.hpp>

#include <stdexcept>
#include <string>
#include <utility>
#include <string_view>

namespace pulsar::application {

class ProjectCodecError : public std::runtime_error {
public:
  explicit ProjectCodecError(std::string message) : std::runtime_error{std::move(message)} {}
};

class ProjectCodec {
public:
  [[nodiscard]] static std::string encode(const ProjectSnapshot& snapshot);
  [[nodiscard]] static ProjectRestoreState decode(std::string_view bytes);
};

} // namespace pulsar::application
