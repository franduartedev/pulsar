#include <pulsar/application/audio/scene_cards.hpp>

#include <cassert>

int main() {
  pulsar::application::PulseMap map;
  map.duration_seconds = 6.0;
  map.points = {
      {0.0, 0.02F, 0.04F, 0.00F, false},
      {1.0, 0.04F, 0.08F, 0.02F, false},
      {2.0, 0.35F, 0.80F, 0.31F, false},
      {3.0, 0.82F, 1.00F, 0.47F, true},
      {4.0, 0.76F, 0.95F, 0.00F, false},
      {5.0, 0.03F, 0.06F, 0.00F, false},
  };
  const auto cards = pulsar::application::build_scene_cards(map, 1.0);
  assert(!cards.empty());
  assert(cards.front().start_seconds == 0.0);
  assert(cards.back().end_seconds == 6.0);
  bool found_impact = false;
  for (const auto& card : cards) {
    if (card.label == pulsar::application::SceneCardLabel::Impact) {
      found_impact = true;
      assert(card.pulse_count >= 1);
    }
  }
  assert(found_impact);
  assert(pulsar::application::build_scene_cards(map, 0.0).empty());
}
