#include <pulsar/application/audio/audio_recipe.hpp>

#include <cassert>

int main() {
  const auto& recipes = pulsar::application::builtin_audio_recipes();
  assert(recipes.size() >= 4);
  const auto* reveal = pulsar::application::find_audio_recipe("reveal");
  assert(reveal != nullptr && reveal->distant_music.enabled && !reveal->telephone.enabled);
  const auto* radio = pulsar::application::find_audio_recipe("radio");
  assert(radio != nullptr && radio->telephone.enabled && !radio->distant_music.enabled);
  assert(pulsar::application::find_audio_recipe("missing") == nullptr);
  return 0;
}
