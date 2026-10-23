#include <pulsar/application/audio/audio_recipe.hpp>

#include <algorithm>

namespace pulsar::application {

const std::vector<AudioRecipe>& builtin_audio_recipes() {
  static const std::vector<AudioRecipe> recipes = [] {
    domain::DistantMusicSettings reveal;
    reveal.enabled = true;
    reveal.initial_gain_db = -36.0;
    reveal.duration_seconds = 5.0;
    reveal.start_cutoff_hz = 420.0;
    reveal.end_cutoff_hz = 18'000.0;
    reveal.reverb_amount = 0.24;
    reveal.curve = domain::DistantMusicCurve::Smooth;

    domain::DistantMusicSettings suspense;
    suspense.enabled = true;
    suspense.initial_gain_db = -48.0;
    suspense.duration_seconds = 7.0;
    suspense.start_cutoff_hz = 180.0;
    suspense.end_cutoff_hz = 12'000.0;
    suspense.reverb_amount = 0.42;
    suspense.curve = domain::DistantMusicCurve::Progressive;

    domain::DistantMusicSettings memory;
    memory.enabled = true;
    memory.initial_gain_db = -28.0;
    memory.duration_seconds = 3.0;
    memory.start_cutoff_hz = 650.0;
    memory.end_cutoff_hz = 9'500.0;
    memory.reverb_amount = 0.58;
    memory.curve = domain::DistantMusicCurve::Explosive;

    domain::TelephoneSettings radio;
    radio.enabled = true;
    radio.mix = 1.0;
    radio.drive = 0.12;

    return std::vector<AudioRecipe>{
        {"reveal", "Revelación", reveal, {}},
        {"suspense", "Suspenso", suspense, {}},
        {"memory", "Memoria", memory, {}},
        {"radio", "Radio", {}, radio},
    };
  }();
  return recipes;
}

const AudioRecipe* find_audio_recipe(std::string_view id) noexcept {
  const auto& recipes = builtin_audio_recipes();
  const auto found = std::find_if(recipes.begin(), recipes.end(),
                                  [id](const AudioRecipe& recipe) { return recipe.id == id; });
  return found == recipes.end() ? nullptr : &*found;
}

} // namespace pulsar::application
