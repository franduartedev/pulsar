#include <pulsar/domain/timeline/title.hpp>

#include <stdexcept>
#include <utility>

namespace pulsar::domain {

Title::Title(TitleId id, std::string text, TimeRange range, int size_permille,
             std::uint32_t color_rgba, TitleHAlign horizontal, TitleVAlign vertical, bool shadow,
             TitleBackground background, int opacity_percent)
    : id_{id}, text_{std::move(text)}, range_{range}, size_permille_{size_permille},
      color_rgba_{color_rgba}, horizontal_{horizontal}, vertical_{vertical}, shadow_{shadow},
      background_{background}, opacity_percent_{opacity_percent} {
  if (text_.empty()) throw std::invalid_argument{"title text cannot be empty"};
  if (text_.size() > maximum_text_bytes) throw std::invalid_argument{"title text is too long"};
  for (const unsigned char c : text_) {
    if (c < 0x20 && c != '\n') throw std::invalid_argument{"title text contains a control character"};
  }
  if (range_.start() < MediaTime::zero()) throw std::invalid_argument{"title start cannot be negative"};
  if (range_.duration() <= MediaTime::zero()) throw std::invalid_argument{"title duration must be positive"};
  if (size_permille_ < minimum_size_permille || size_permille_ > maximum_size_permille) {
    throw std::invalid_argument{"title size is out of range"};
  }
  if (opacity_percent_ < minimum_opacity_percent || opacity_percent_ > maximum_opacity_percent ||
      static_cast<int>(background_) > static_cast<int>(TitleBackground::Light)) {
    throw std::invalid_argument{"title look is out of range"};
  }
}

Title Title::with_text(std::string text) const {
  return Title{id_, std::move(text), range_, size_permille_, color_rgba_, horizontal_, vertical_, shadow_, background_, opacity_percent_};
}

Title Title::with_range(TimeRange range) const {
  return Title{id_, text_, range, size_permille_, color_rgba_, horizontal_, vertical_, shadow_, background_, opacity_percent_};
}

Title Title::with_style(int size_permille, std::uint32_t color_rgba, TitleHAlign horizontal,
                        TitleVAlign vertical, bool shadow) const {
  return Title{id_, text_, range_, size_permille, color_rgba, horizontal, vertical, shadow, background_, opacity_percent_};
}

Title Title::with_look(TitleBackground background, int opacity_percent) const {
  return Title{id_, text_, range_, size_permille_, color_rgba_, horizontal_, vertical_, shadow_, background, opacity_percent};
}

bool Title::active_at(MediaTime time) const {
  return time >= range_.start() && time < range_.end();
}

std::string Title::look_key() const {
  return std::to_string(size_permille_) + "|" + std::to_string(color_rgba_) + "|" +
         std::to_string(static_cast<int>(horizontal_)) + "|" +
         std::to_string(static_cast<int>(vertical_)) + "|" + (shadow_ ? "s" : "n") + "|" +
         std::to_string(static_cast<int>(background_)) + "|" + std::to_string(opacity_percent_) + "|" + text_;
}

} // namespace pulsar::domain
