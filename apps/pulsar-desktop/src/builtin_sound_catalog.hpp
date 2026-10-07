#pragma once

#include <pulsar/project_file/runtime_resources.hpp>

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
  const auto root = pulsar::project_file::assets_directory();
  const auto filename = std::string{value.substr(prefix.size())} + ".wav";
  // Missing pack: return a path that does not exist so callers report the
  // pack as unavailable instead of guessing another location.
  return (root.empty() ? std::filesystem::path{"assets"} : root) / "sounds" / filename;
}

} // namespace pulsar::desktop
