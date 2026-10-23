#pragma once

#include <cmath>
#include <stdexcept>

namespace pulsar::domain {

// Underwater intentionally stays compact: depth controls the muffling cutoff
// and mix controls how much of the submerged signal replaces the dry source.
struct UnderwaterSettings {
  bool enabled{false};
  double mix{0.72};
  double depth{0.68};

  void validate() const {
    if (!std::isfinite(mix) || !std::isfinite(depth) || mix < 0.0 || mix > 1.0 ||
        depth < 0.0 || depth > 1.0) {
      throw std::invalid_argument{"invalid Underwater settings"};
    }
  }

  friend bool operator==(const UnderwaterSettings&, const UnderwaterSettings&) = default;
};

} // namespace pulsar::domain
