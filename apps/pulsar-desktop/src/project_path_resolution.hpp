#pragma once

#include <filesystem>

namespace pulsar::desktop {

// Resolves the FFmpeg runtime path for an authored media locator without
// touching the persisted/document value. Absolute locators pass through;
// relative locators resolve against the directory containing the project
// file (never against the process working directory). Header-only and Qt-free
// so the contract stays unit-testable.
[[nodiscard]] inline std::filesystem::path resolve_runtime_media_path(
    const std::filesystem::path& project_file, const std::filesystem::path& authored_locator) {
  if (authored_locator.is_absolute()) {
    return authored_locator;
  }
  const auto parent = project_file.parent_path();
  if (parent.empty()) {
    return authored_locator;
  }
  return parent / authored_locator;
}

} // namespace pulsar::desktop
