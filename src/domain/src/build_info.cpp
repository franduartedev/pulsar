#include <pulsar/domain/build_info.hpp>

static_assert(pulsar::domain::cpp20_or_newer(), "PULSAR requires C++20 or newer.");

#ifndef PULSAR_VERSION_STRING
#define PULSAR_VERSION_STRING "0.0.0-dev"
#endif

namespace pulsar::domain {

std::string_view product_version() noexcept { return PULSAR_VERSION_STRING; }

} // namespace pulsar::domain
