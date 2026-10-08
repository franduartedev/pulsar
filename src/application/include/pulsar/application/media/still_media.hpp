#pragma once

#include <pulsar/domain/time/frame_rate.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Still media: photographs/graphics (PNG, JPEG, WebP) and generated colour cards. They are real
// video-stream assets with one frame that is held for the whole clip, so every consumer
// (preview, export, thumbnails) keeps using the same decode path. No temporary movie is made.
//
// A still has no intrinsic length. Its asset reports a generous "playable" span so that the
// clip can be trimmed or extended freely; the length a clip gets when it is placed is
// `still_default_duration()`.
namespace pulsar::application {

inline constexpr std::string_view color_card_locator_prefix = "pulsar-color:";
inline constexpr std::string_view color_card_container = "pulsar-color";

[[nodiscard]] inline domain::MediaTime still_default_duration() {
  return domain::MediaTime{5, domain::Rational{1, 1}};
}
[[nodiscard]] inline domain::MediaTime still_playable_duration() {
  return domain::MediaTime{3600, domain::Rational{1, 1}};
}
[[nodiscard]] inline domain::FrameRate still_frame_rate() { return domain::FrameRate{30, 1}; }

// Largest picture a still-only project is built around (a 12 MP photo must not make the whole
// export 4000x3000).
inline constexpr int still_canvas_max_width = 1920;
inline constexpr int still_canvas_max_height = 1080;

// FFmpeg demuxer names that hold exactly one picture.
[[nodiscard]] inline bool is_still_image_container(std::string_view name) {
  return name == "image2" || name == "png_pipe" || name == "jpeg_pipe" || name == "webp_pipe";
}

[[nodiscard]] inline bool is_color_card_locator(std::string_view locator) {
  return locator.starts_with(color_card_locator_prefix);
}

[[nodiscard]] inline std::string color_card_locator(std::uint32_t rgb) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string result{color_card_locator_prefix};
  for (int shift = 20; shift >= 0; shift -= 4) result.push_back(digits[(rgb >> shift) & 0xF]);
  return result;
}

// "pulsar-color:rrggbb" -> 0xRRGGBB.
[[nodiscard]] inline std::optional<std::uint32_t> color_card_rgb(std::string_view locator) {
  if (!is_color_card_locator(locator)) return std::nullopt;
  const auto hex = locator.substr(color_card_locator_prefix.size());
  if (hex.size() != 6) return std::nullopt;
  std::uint32_t value = 0;
  for (const char c : hex) {
    value <<= 4;
    if (c >= '0' && c <= '9') value |= static_cast<std::uint32_t>(c - '0');
    else if (c >= 'a' && c <= 'f') value |= static_cast<std::uint32_t>(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') value |= static_cast<std::uint32_t>(c - 'A' + 10);
    else return std::nullopt;
  }
  return value;
}

} // namespace pulsar::application
