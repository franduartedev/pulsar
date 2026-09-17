#pragma once

#include <pulsar/application/project/project_restore_state.hpp>
#include <pulsar/application/project/project_snapshot.hpp>

#include <filesystem>
#include <stdexcept>
#include <utility>
#include <string>

namespace pulsar::application {

enum class ProjectSaveStatus {
  Durable,
  CommittedDurabilityUncertain,
};

struct ProjectSaveResult {
  ProjectSaveStatus status;
};

class ProjectRepositoryError : public std::runtime_error {
public:
  explicit ProjectRepositoryError(std::string message) : std::runtime_error{std::move(message)} {}
};

class ProjectRepository {
public:
  virtual ~ProjectRepository() = default;
  [[nodiscard]] virtual ProjectSaveResult save(const std::filesystem::path& destination,
                                               const ProjectSnapshot& snapshot) const = 0;
  [[nodiscard]] virtual ProjectRestoreState load(const std::filesystem::path& source) const = 0;
};

} // namespace pulsar::application
