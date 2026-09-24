#pragma once

#include <pulsar/application/export/export_job.hpp>

namespace pulsar::project_file {

// Linux-first adapter for ExportOutputPublisher. It uses same-directory
// std::filesystem::rename without deleting the destination first; on Linux
// this atomically replaces an existing destination. Other platforms may
// provide a different adapter if their rename primitive cannot guarantee the
// same ReplaceExistingSafely contract.
class FilesystemExportOutputPublisher final
    : public application::exporting::ExportOutputPublisher {
public:
  [[nodiscard]] std::filesystem::path create_temporary_sibling(
      const std::filesystem::path& destination) override;
  [[nodiscard]] bool temporary_exists(
      const std::filesystem::path& temporary_output) const override;
  void publish_replace_existing_safely(const std::filesystem::path& temporary_output,
                                       const std::filesystem::path& destination) override;
  void cleanup_temporary(const std::filesystem::path& temporary_output) noexcept override;
};

} // namespace pulsar::project_file
