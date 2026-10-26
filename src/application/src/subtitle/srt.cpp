#include <pulsar/application/subtitle/srt.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <utility>

namespace pulsar::application {
namespace {

constexpr std::size_t maximum_cues = 10'000;
constexpr std::size_t maximum_text = 16'384;

[[noreturn]] void fail(const char* message) { throw SrtError{message}; }

std::vector<std::string_view> lines_of(std::string_view bytes) {
  std::vector<std::string_view> lines;
  std::size_t start = 0;
  while (start <= bytes.size()) {
    const auto end = bytes.find('\n', start);
    auto line = bytes.substr(start, end == std::string_view::npos ? bytes.size() - start
                                                                   : end - start);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    lines.push_back(line);
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return lines;
}

std::uint32_t sequence(std::string_view line) {
  std::uint32_t value = 0;
  const auto result = std::from_chars(line.data(), line.data() + line.size(), value);
  if (line.empty() || result.ec != std::errc{} || result.ptr != line.data() + line.size() || value == 0) {
    fail("invalid SRT sequence number");
  }
  return value;
}

int two_digits(std::string_view value, std::size_t offset) {
  if (offset + 2 > value.size() || value[offset] < '0' || value[offset] > '9' ||
      value[offset + 1] < '0' || value[offset + 1] > '9') {
    fail("invalid SRT timestamp");
  }
  return (value[offset] - '0') * 10 + value[offset + 1] - '0';
}

int three_digits(std::string_view value, std::size_t offset) {
  if (offset + 3 > value.size()) fail("invalid SRT timestamp");
  for (std::size_t index = offset; index < offset + 3; ++index) {
    if (value[index] < '0' || value[index] > '9') fail("invalid SRT timestamp");
  }
  return (value[offset] - '0') * 100 + (value[offset + 1] - '0') * 10 +
         value[offset + 2] - '0';
}

domain::MediaTime parse_timestamp(std::string_view value) {
  if (value.size() != 12 || value[2] != ':' || value[5] != ':' || value[8] != ',' ||
      value[9] < '0' || value[9] > '9' || value[10] < '0' || value[10] > '9' ||
      value[11] < '0' || value[11] > '9') {
    fail("invalid SRT timestamp");
  }
  const int hours = two_digits(value, 0);
  const int minutes = two_digits(value, 3);
  const int seconds = two_digits(value, 6);
  const int millis = three_digits(value, 9);
  if (minutes > 59 || seconds > 59) fail("invalid SRT timestamp range");
  const auto total = (static_cast<std::int64_t>(hours) * 3600 + minutes * 60 + seconds) * 1000 + millis;
  return domain::MediaTime{total, domain::Rational{1, 1000}};
}

std::string format_timestamp(domain::MediaTime time) {
  const auto milliseconds = time.seconds();
  const auto total = static_cast<std::int64_t>(milliseconds.numerator() * 1000 /
                                                milliseconds.denominator());
  const auto hours = total / 3'600'000;
  const auto minutes = (total / 60'000) % 60;
  const auto seconds = (total / 1000) % 60;
  const auto millis = total % 1000;
  std::ostringstream output;
  output << std::setfill('0') << std::setw(2) << hours << ':' << std::setw(2) << minutes << ':'
         << std::setw(2) << seconds << ',' << std::setw(3) << millis;
  return output.str();
}

} // namespace

std::vector<SubtitleCue> parse_srt(std::string_view bytes) {
  const auto lines = lines_of(bytes);
  std::vector<SubtitleCue> cues;
  std::size_t index = 0;
  while (index < lines.size()) {
    while (index < lines.size() && lines[index].empty()) ++index;
    if (index == lines.size()) break;
    if (cues.size() >= maximum_cues) fail("SRT cue limit exceeded");
    SubtitleCue cue;
    cue.sequence = sequence(lines[index++]);
    if (index >= lines.size()) fail("SRT cue is missing timestamp");
    const auto timing = lines[index++];
    const auto separator = timing.find(" --> ");
    if (separator == std::string_view::npos) fail("invalid SRT cue timing");
    cue.start = parse_timestamp(timing.substr(0, separator));
    cue.end = parse_timestamp(timing.substr(separator + 5));
    if (cue.end <= cue.start) fail("SRT cue end must be after start");
    std::ostringstream text;
    bool has_text = false;
    while (index < lines.size() && !lines[index].empty()) {
      if (has_text) text << '\n';
      text << lines[index++];
      has_text = true;
      if (text.tellp() > static_cast<std::streamoff>(maximum_text)) fail("SRT cue text is too long");
    }
    if (!has_text) fail("SRT cue text cannot be empty");
    cue.text = text.str();
    if (!cues.empty() && cue.start < cues.back().end) fail("SRT cues must be ordered and non-overlapping");
    cues.push_back(std::move(cue));
  }
  return cues;
}

std::string encode_srt(const std::vector<SubtitleCue>& cues) {
  if (cues.size() > maximum_cues) fail("SRT cue limit exceeded");
  std::ostringstream output;
  domain::MediaTime previous_end = domain::MediaTime::zero();
  for (std::size_t index = 0; index < cues.size(); ++index) {
    const auto& cue = cues[index];
    if (cue.sequence == 0 || cue.end <= cue.start || cue.start < previous_end ||
        cue.text.empty() || cue.text.size() > maximum_text || cue.text.find('\r') != std::string::npos) {
      fail("invalid subtitle cue");
    }
    output << cue.sequence << '\n' << format_timestamp(cue.start) << " --> "
           << format_timestamp(cue.end) << '\n' << cue.text << "\n\n";
    previous_end = cue.end;
  }
  return output.str();
}

} // namespace pulsar::application
