#pragma once

#include <filesystem>
#include <string_view>
#include <system_error>

namespace pulsar::desktop {

inline bool is_builtin_sound_locator(const std::filesystem::path& path) {
  return path.generic_string().starts_with("pulsar-sound:");
}

inline std::filesystem::path builtin_sound_runtime_path(const std::filesystem::path& path) {
  const auto value = path.generic_string();
  constexpr std::string_view prefix{"pulsar-sound:"};
  if (!value.starts_with(prefix)) return path;
#ifndef PULSAR_SOURCE_DIR
#error "PULSAR_SOURCE_DIR is required for the original core sounds pack"
#endif
  const auto filename = std::string{value.substr(prefix.size())} + ".wav";
  const auto relative = std::filesystem::path{"assets"} / "sounds" / filename;
  std::error_code error;
  auto candidate = std::filesystem::current_path(error) / relative;
  if (!error && std::filesystem::is_regular_file(candidate, error)) return candidate;
  auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
  if (!error) {
    for (auto directory = executable.parent_path(); !directory.empty(); directory = directory.parent_path()) {
      candidate = directory / relative;
      if (std::filesystem::is_regular_file(candidate, error)) return candidate;
      if (directory == directory.parent_path()) break;
    }
  }
#ifndef PULSAR_SOURCE_DIR
#error "PULSAR_SOURCE_DIR is required for the original core sounds pack"
#endif
  candidate = std::filesystem::path{PULSAR_SOURCE_DIR} / relative;
  if (std::filesystem::is_regular_file(candidate, error)) return candidate;
  return candidate;
}

} // namespace pulsar::desktop
