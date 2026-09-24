#include <pulsar/project_file/filesystem_export_output_publisher.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace pulsar::project_file {
namespace {

[[nodiscard]] std::filesystem::path::string_type unique_suffix() {
  static std::atomic<std::uint64_t> sequence{0};
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto ordinal = sequence.fetch_add(1, std::memory_order_relaxed);
  return std::filesystem::path{".pulsar-" + std::to_string(now) + "-" +
                               std::to_string(ordinal) + ".part"}
      .native();
}

} // namespace

std::filesystem::path FilesystemExportOutputPublisher::create_temporary_sibling(
    const std::filesystem::path& destination) {
  const auto parent = destination.parent_path().empty() ? std::filesystem::path{"."}
                                                        : destination.parent_path();
  std::error_code error;
  if (!std::filesystem::is_directory(parent, error) || error) {
    throw std::runtime_error{"export destination directory is unavailable"};
  }
  if (destination.filename().empty()) {
    throw std::runtime_error{"export destination filename is empty"};
  }

  for (int attempt = 0; attempt < 100; ++attempt) {
    auto name = destination.filename();
    name += unique_suffix();
    const auto candidate = parent / name;
    if (!std::filesystem::exists(candidate, error)) return candidate;
    if (error) throw std::runtime_error{"could not inspect temporary export path"};
  }
  throw std::runtime_error{"could not allocate a unique temporary export path"};
}

bool FilesystemExportOutputPublisher::temporary_exists(
    const std::filesystem::path& temporary_output) const {
  std::error_code error;
  const auto exists = std::filesystem::exists(temporary_output, error);
  if (error) throw std::runtime_error{"could not inspect temporary export output"};
  return exists;
}

void FilesystemExportOutputPublisher::publish_replace_existing_safely(
    const std::filesystem::path& temporary_output, const std::filesystem::path& destination) {
  std::error_code error;
  std::filesystem::rename(temporary_output, destination, error);
  if (error) {
    throw std::runtime_error{"could not publish export output: " + error.message()};
  }
}

void FilesystemExportOutputPublisher::cleanup_temporary(
    const std::filesystem::path& temporary_output) noexcept {
  std::error_code ignored;
  std::filesystem::remove(temporary_output, ignored);
}

} // namespace pulsar::project_file
