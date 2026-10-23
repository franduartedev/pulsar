#include <pulsar/project_file/runtime_resources.hpp>

#include <cstdlib>
#include <string>
#include <system_error>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pulsar::project_file {
namespace {

[[nodiscard]] std::filesystem::path executable_path() {
#if defined(_WIN32)
  // Wide API: the executable may live under a non-ASCII user directory.
  std::wstring buffer(32'768, L'\0');
  const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) {
    return {};
  }
  buffer.resize(length);
  return std::filesystem::path{buffer};
#elif defined(__linux__)
  std::error_code error;
  const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
  return error ? std::filesystem::path{} : path;
#else
  return {};
#endif
}

} // namespace

std::filesystem::path assets_directory() {
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
