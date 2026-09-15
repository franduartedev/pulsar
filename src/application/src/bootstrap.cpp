#include <pulsar/application/bootstrap.hpp>

#include <pulsar/domain/build_info.hpp>

#include <string>

namespace pulsar::application {

std::string bootstrap_message() {
  return std::string{domain::product_name()} + " Foundation build OK";
}

} // namespace pulsar::application
