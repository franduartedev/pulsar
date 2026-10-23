#pragma once

#include <pulsar/domain/time/media_time.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pulsar::application {

struct SubtitleCue {
  std::uint32_t sequence{0};
  domain::MediaTime start{domain::MediaTime::zero()};
  domain::MediaTime end{domain::MediaTime::zero()};
  std::string text;
};

class SrtError final : public std::invalid_argument {
public:
  explicit SrtError(const std::string& message) : std::invalid_argument{message} {}
};

[[nodiscard]] std::vector<SubtitleCue> parse_srt(std::string_view bytes);
[[nodiscard]] std::string encode_srt(const std::vector<SubtitleCue>& cues);

} // namespace pulsar::application
