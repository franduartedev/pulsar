#pragma once

#include <cstdlib>
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
  // libavformat recognizes a fully-qualified DOS path (C:/...) on Windows.
  // Keep the same generic UTF-8 spelling used by the production adapter.
  const auto value = path.generic_u8string();
  std::string text{reinterpret_cast<const char*>(value.data()), value.size()};
  if (text.starts_with("//?/")) text.erase(0, 4);
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

// Runs a command line built from shell_quote() pieces. cmd.exe /c drops the
// first and last quote of a command that starts with a quote and holds more
// quotes (a quoted program plus a quoted path), so Windows needs one more pair.
[[nodiscard]] inline int run_shell(const std::string& command) {
#if defined(_WIN32)
  return std::system(("\"" + command + "\"").c_str());
#else
  return std::system(command.c_str());
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
