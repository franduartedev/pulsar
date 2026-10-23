#include <pulsar/application/audio/audio_recipe.hpp>

#include <cassert>

int main() {
  const auto& recipes = pulsar::application::builtin_audio_recipes();
  assert(recipes.size() >= 7);
  const auto* reveal = pulsar::application::find_audio_recipe("reveal");
  assert(reveal != nullptr && reveal->distant_music.enabled && !reveal->telephone.enabled);
  const auto* radio = pulsar::application::find_audio_recipe("radio");
  assert(radio != nullptr && radio->telephone.enabled && !radio->distant_music.enabled);
  const auto* submerge = pulsar::application::find_audio_recipe("submerge");
  assert(submerge != nullptr && submerge->underwater.enabled);
  const auto* deep_dive = pulsar::application::find_audio_recipe("deep_dive");
  assert(deep_dive != nullptr && deep_dive->distant_music.enabled &&
         deep_dive->underwater.enabled);
  const auto* lost_signal = pulsar::application::find_audio_recipe("lost_signal");
  assert(lost_signal != nullptr && lost_signal->distant_music.enabled &&
         lost_signal->telephone.enabled);
  assert(pulsar::application::find_audio_recipe("missing") == nullptr);
  return 0;
}
