#include <pulsar/application/subtitle/srt.hpp>

#include <cassert>

int main() {
  const std::string source = "1\n00:00:01,250 --> 00:00:02,500\nHola mundo\n\n"
                             "2\n00:00:03,000 --> 00:00:04,125\nLínea uno\nLínea dos\n\n";
  const auto cues = pulsar::application::parse_srt(source);
  assert(cues.size() == 2);
  const pulsar::domain::MediaTime expected_start{1250, {1, 1000}};
  assert(cues[0].start == expected_start);
  assert(cues[1].text == "Línea uno\nLínea dos");
  assert(pulsar::application::encode_srt(cues) == source);
  bool rejected = false;
  try {
    (void)pulsar::application::parse_srt("1\n00:00:02,000 --> 00:00:01,000\nNo\n");
  } catch (const pulsar::application::SrtError&) {
    rejected = true;
  }
  assert(rejected);
}
