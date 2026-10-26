#pragma once

#include <cstddef>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace pulsar::domain {

enum class AudioEffectType { DistantMusic = 0, Telephone = 1, Underwater = 2 };

inline std::vector<AudioEffectType> canonical_audio_effect_order() {
  return {AudioEffectType::DistantMusic, AudioEffectType::Telephone,
          AudioEffectType::Underwater};
}

inline void validate_audio_effect_order(const std::vector<AudioEffectType>& order) {
  if (order.size() != 3) throw std::invalid_argument{"audio effect order must contain every effect type"};
  for (std::size_t i = 0; i < order.size(); ++i) {
    switch (order[i]) {
    case AudioEffectType::DistantMusic:
    case AudioEffectType::Telephone:
    case AudioEffectType::Underwater:
      break;
    default:
      throw std::invalid_argument{"audio effect order contains an unknown effect type"};
    }
    if (std::find(order.begin() + static_cast<std::ptrdiff_t>(i + 1), order.end(), order[i]) !=
        order.end()) {
      throw std::invalid_argument{"audio effect order contains duplicates"};
    }
  }
}

} // namespace pulsar::domain
