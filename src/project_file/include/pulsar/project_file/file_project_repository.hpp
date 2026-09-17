#pragma once

#include <pulsar/application/project/project_repository.hpp>

namespace pulsar::project_file {

class FileProjectRepository final : public application::ProjectRepository {
public:
  [[nodiscard]] application::ProjectSaveResult save(
      const std::filesystem::path& destination,
      const application::ProjectSnapshot& snapshot) const override;
  [[nodiscard]] application::ProjectRestoreState load(
      const std::filesystem::path& source) const override;
};

} // namespace pulsar::project_file
