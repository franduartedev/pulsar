#pragma once

#include <filesystem>
#include <string>

namespace pulsar::test_support {

[[nodiscard]] inline std::string utf8_path(const std::filesystem::path& path) {
#if defined(_WIN32)
  const auto value = path.generic_u8string();
  return {reinterpret_cast<const char*>(value.data()), value.size()};
#else
  return path.generic_string();
#endif
}

[[nodiscard]] inline std::string ffmpeg_path(const std::filesystem::path& path) {
#if defined(_WIN32)
  // FFmpeg's Windows file protocol accepts native DOS paths. Keep the
  // backslashes produced by path.u8string(); converting C:\\... to C:/...
  // makes some FFmpeg builds treat the drive letter as a protocol name.
  const auto value = path.u8string();
  std::string text{reinterpret_cast<const char*>(value.data()), value.size()};
  if (text.starts_with("\\\\?\\")) text.erase(0, 4);
  return text;
#else
  return path.generic_string();
#endif
}

// Quotes a path for the shell that std::system() uses on this platform:
// POSIX single quotes, or cmd.exe double quotes on Windows.
[[nodiscard]] inline std::string shell_quote(const std::filesystem::path& path) {
#if defined(_WIN32)
  std::string quoted{"\""};
  for (const char character : utf8_path(path)) {
    if (character == '"') {
      quoted += "\\\"";
    } else {
      quoted += character;
    }
  }
  quoted += '"';
  return quoted;
#else
  std::string quoted{"'"};
  for (const char character : utf8_path(path)) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }
  quoted += "'";
  return quoted;
#endif
}

// Output sink understood by the platform shell's redirection operator.
[[nodiscard]] inline std::string null_device() {
#if defined(_WIN32)
  return "NUL";
#else
  return "/dev/null";
#endif
}

} // namespace pulsar::test_support
