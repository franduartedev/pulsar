#pragma once

#include <pulsar/domain/timeline/distant_music.hpp>
#include <pulsar/domain/timeline/telephone.hpp>
#include <pulsar/domain/timeline/underwater.hpp>

#include <string_view>
#include <vector>

namespace pulsar::application {

struct AudioRecipe {
  std::string_view id;
  std::string_view name;
  domain::DistantMusicSettings distant_music;
  domain::TelephoneSettings telephone;
  domain::UnderwaterSettings underwater;
};

// Immutable built-ins. Recipes are application data so UI clients can list
// and apply them without duplicating parameter values or effect semantics.
[[nodiscard]] const std::vector<AudioRecipe>& builtin_audio_recipes();
[[nodiscard]] const AudioRecipe* find_audio_recipe(std::string_view id) noexcept;

} // namespace pulsar::application
