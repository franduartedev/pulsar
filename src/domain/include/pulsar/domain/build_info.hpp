#pragma once

#include <string_view>

namespace pulsar::domain {

[[nodiscard]] constexpr std::string_view product_name() noexcept {
  return "PULSAR";
}

[[nodiscard]] constexpr bool cpp20_or_newer() noexcept {
  return __cplusplus >= 202002L;
}

} // namespace pulsar::domain
