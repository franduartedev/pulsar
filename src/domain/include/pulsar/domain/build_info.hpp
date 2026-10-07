#pragma once

#include <string_view>

namespace pulsar::domain {

[[nodiscard]] constexpr std::string_view product_name() noexcept {
  return "PULSAR";
}

// "0.1.0-beta": defined once in the root CMakeLists (PULSAR_VERSION_STRING).
[[nodiscard]] std::string_view product_version() noexcept;

[[nodiscard]] constexpr bool cpp20_or_newer() noexcept {
  return __cplusplus >= 202002L;
}

} // namespace pulsar::domain
