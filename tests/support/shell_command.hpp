#pragma once

#include <filesystem>
#include <string>

namespace pulsar::test_support {

// Quotes a path for the shell that std::system() uses on this platform:
// POSIX single quotes, or cmd.exe double quotes on Windows.
[[nodiscard]] inline std::string shell_quote(const std::filesystem::path& path) {
#if defined(_WIN32)
  std::string quoted{"\""};
  for (const char character : path.string()) {
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
  for (const char character : path.string()) {
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
