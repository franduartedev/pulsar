#include <pulsar/application/bootstrap.hpp>
#include <pulsar/domain/build_info.hpp>

#include <iostream>
#include <string>
#include <string_view>

namespace {

int fail(std::string_view message) {
  std::cerr << "smoke test failed: " << message << '\n';
  return 1;
}

} // namespace

int main() {
  static_assert(__cplusplus >= 202002L, "The smoke test must compile as C++20.");
  static_assert(pulsar::domain::cpp20_or_newer(), "Domain should report C++20 support.");

  if (pulsar::domain::product_name() != "PULSAR") {
    return fail("unexpected product name");
  }

  const std::string message = pulsar::application::bootstrap_message();
  if (message != "PULSAR Foundation build OK") {
    return fail("unexpected bootstrap message");
  }

  return 0;
}
