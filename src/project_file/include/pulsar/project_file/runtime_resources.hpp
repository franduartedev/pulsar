#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace pulsar::project_file {

// Location of the shipped read-only assets (the Core Sounds pack lives in
// <assets>/sounds). Nothing here is baked in at build time: a release must run
// from any directory. Search order:
//   1. $PULSAR_ASSETS_DIR                      (explicit override, e.g. for testers)
//   2. <exe dir>/assets, <exe dir>/../assets   (build tree and portable bundle)
//   3. <exe dir>/../share/pulsar/assets        (installed under a prefix)
//   4. <cwd>/assets
// Returns the first directory that contains sounds/index.json, or an empty path.
// The executable is located through /proc/self/exe on Linux and
// GetModuleFileNameW on Windows.
[[nodiscard]] inline std::filesystem::path executable_path() {
#if defined(_WIN32)
  std::wstring buffer(512, L'\0');
  for (;;) {
    const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) return {};
    if (length < buffer.size()) {
      buffer.resize(length);
      return std::filesystem::path{buffer};
    }
    buffer.resize(buffer.size() * 2);
  }
#else
  std::error_code error;
  auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
  return error ? std::filesystem::path{} : executable;
#endif
}

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
  const auto executable = executable_path();
  if (!executable.empty()) {
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
