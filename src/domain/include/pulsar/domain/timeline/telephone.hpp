#pragma once

#include <cmath>
#include <stdexcept>

namespace pulsar::domain {

struct TelephoneSettings {
  bool enabled{false};
  double mix{1.0};
  double drive{0.12};

  void validate() const {
    if (!std::isfinite(mix) || mix < 0.0 || mix > 1.0 || !std::isfinite(drive) ||
        drive < 0.0 || drive > 1.0) {
      throw std::invalid_argument{"invalid Telephone settings"};
    }
  }

  friend bool operator==(const TelephoneSettings&, const TelephoneSettings&) = default;
};

} // namespace pulsar::domain
