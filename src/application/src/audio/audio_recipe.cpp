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

    domain::UnderwaterSettings submerged;
    submerged.enabled = true;
    submerged.mix = 0.78;
    submerged.depth = 0.82;

    domain::DistantMusicSettings deep_dive_music = suspense;
    deep_dive_music.initial_gain_db = -42.0;
    deep_dive_music.duration_seconds = 6.0;
    deep_dive_music.reverb_amount = 0.5;
    domain::UnderwaterSettings deep_dive_water = submerged;
    deep_dive_water.mix = 0.86;
    deep_dive_water.depth = 0.9;

    domain::DistantMusicSettings lost_signal_music = memory;
    lost_signal_music.initial_gain_db = -34.0;
    lost_signal_music.duration_seconds = 4.5;
    domain::TelephoneSettings lost_signal_radio = radio;
    lost_signal_radio.drive = 0.2;

    return std::vector<AudioRecipe>{
        {"reveal", "Revelación", reveal, {}, {}},
        {"suspense", "Suspenso", suspense, {}, {}},
        {"memory", "Memoria", memory, {}, {}},
        {"radio", "Radio", {}, radio, {}},
        {"submerge", "Sumergido", {}, {}, submerged},
        {"deep_dive", "Inmersión Profunda", deep_dive_music, {}, deep_dive_water},
        {"lost_signal", "Señal Perdida", lost_signal_music, lost_signal_radio, {}},
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
