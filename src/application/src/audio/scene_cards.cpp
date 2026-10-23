#include <pulsar/application/audio/scene_cards.hpp>

#include <algorithm>
#include <cmath>

namespace pulsar::application {
namespace {

SceneCardLabel label_for(const PulseMapPoint& point, float maximum_energy,
                         float average_transient, float maximum_transient) noexcept {
  if (point.pulse && point.transient >= maximum_transient * 0.65F) {
    return SceneCardLabel::Impact;
  }
  if (point.transient >= std::max(0.015F, average_transient * 1.5F)) {
    return SceneCardLabel::Build;
  }
  if (point.energy <= maximum_energy * 0.22F) return SceneCardLabel::Quiet;
  return SceneCardLabel::Sustained;
}

void absorb(SceneCard& card, const PulseMapPoint& point, double end, SceneCardLabel label) {
  card.end_seconds = end;
  card.average_energy = (card.average_energy + point.energy) * 0.5F;
  card.maximum_peak = std::max(card.maximum_peak, point.peak);
  if (point.pulse) ++card.pulse_count;
  card.label = label;
}

} // namespace

std::vector<SceneCard> build_scene_cards(const PulseMap& pulse_map,
                                         double minimum_duration_seconds) {
  std::vector<SceneCard> cards;
  if (pulse_map.points.empty() || !std::isfinite(minimum_duration_seconds) ||
      minimum_duration_seconds <= 0.0) {
    return cards;
  }
  float maximum_energy = 0.0F;
  float maximum_transient = 0.0F;
  float transient_sum = 0.0F;
  for (const auto& point : pulse_map.points) {
    maximum_energy = std::max(maximum_energy, point.energy);
    maximum_transient = std::max(maximum_transient, point.transient);
    transient_sum += point.transient;
  }
  const float average_transient = transient_sum /
                                 static_cast<float>(pulse_map.points.size());
  const auto point_end = [&](std::size_t index) {
    if (index + 1 < pulse_map.points.size()) return pulse_map.points[index + 1].time_seconds;
    return pulse_map.duration_seconds;
  };
  for (std::size_t index = 0; index < pulse_map.points.size(); ++index) {
    const auto& point = pulse_map.points[index];
    const auto label = label_for(point, maximum_energy, average_transient, maximum_transient);
    const auto end = std::max(point_end(index), point.time_seconds);
    if (cards.empty() || cards.back().label != label) {
      cards.push_back(SceneCard{point.time_seconds, end, point.energy, point.peak,
                                point.pulse ? 1U : 0U, label});
    } else {
      absorb(cards.back(), point, end, label);
    }
  }
  // Avoid noisy one-window cards: merge a short region into its stronger
  // neighbour while keeping the result ordered and lossless in time.
  for (std::size_t index = 0; index < cards.size();) {
    if (cards.size() <= 1 || cards[index].end_seconds - cards[index].start_seconds >=
                                minimum_duration_seconds) {
      ++index;
      continue;
    }
    if (index == 0) {
      cards[1].start_seconds = cards[0].start_seconds;
      cards[1].average_energy = std::max(cards[1].average_energy, cards[0].average_energy);
      cards[1].maximum_peak = std::max(cards[1].maximum_peak, cards[0].maximum_peak);
      cards[1].pulse_count += cards[0].pulse_count;
      cards.erase(cards.begin());
    } else {
      cards[index - 1].end_seconds = cards[index].end_seconds;
      cards[index - 1].average_energy = std::max(cards[index - 1].average_energy,
                                                 cards[index].average_energy);
      cards[index - 1].maximum_peak = std::max(cards[index - 1].maximum_peak,
                                               cards[index].maximum_peak);
      cards[index - 1].pulse_count += cards[index].pulse_count;
      cards.erase(cards.begin() + static_cast<std::ptrdiff_t>(index));
      --index;
    }
  }
  return cards;
}

} // namespace pulsar::application
