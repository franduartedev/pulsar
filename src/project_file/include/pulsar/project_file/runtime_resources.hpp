#pragma once

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace pulsar::project_file {

// Location of the shipped read-only assets (the Core Sounds pack lives in
// <assets>/sounds). Nothing here is baked in at build time: a release must run
// from any directory. Search order:
//   1. $PULSAR_ASSETS_DIR                      (explicit override, e.g. for testers)
//   2. <exe dir>/assets, <exe dir>/../assets   (build tree and portable bundle)
//   3. <exe dir>/../share/pulsar/assets        (installed under a prefix)
//   4. <cwd>/assets
// Returns the first directory that contains sounds/index.json, or an empty path.
// Linux-only today: the executable is located through /proc/self/exe (see the
// Windows readiness notes in docs/release/0.1.0-beta.md).
[[nodiscard]] inline std::filesystem::path assets_directory() {
  namespace fs = std::filesystem;
  std::error_code error;
  const auto usable = [&error](const fs::path& directory) {
    return !directory.empty() && fs::is_regular_file(directory / "sounds" / "index.json", error);
  };
  if (const char* override_dir = std::getenv("PULSAR_ASSETS_DIR");
      override_dir != nullptr && usable(fs::path{override_dir})) {
    return fs::path{override_dir};
  }
  const auto executable = fs::read_symlink("/proc/self/exe", error);
  if (!error) {
    const auto dir = executable.parent_path();
    for (const auto& candidate : {dir / "assets", dir.parent_path() / "assets",
                                  dir.parent_path() / "share" / "pulsar" / "assets"}) {
      if (usable(candidate)) return candidate;
    }
  }
  const auto cwd = fs::current_path(error);
  if (!error && usable(cwd / "assets")) return cwd / "assets";
  return {};
}

} // namespace pulsar::project_file
