#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/time_range.hpp>

#include <cstdint>
#include <string>

namespace pulsar::domain {

struct TitleIdTag;
using TitleId = detail::StrongId<TitleIdTag>;

enum class TitleHAlign : std::uint8_t { Left, Center, Right };
enum class TitleVAlign : std::uint8_t { Top, Middle, Bottom };

// A static text overlay on the video. Pure data: no fonts, pixels or UI types. The size is
// stored in permille of the output frame HEIGHT, so the same title looks the same at 720p,
// 1080p or in the preview (layout is resolution independent).
class Title {
public:
  static constexpr int minimum_size_permille = 20;
  static constexpr int maximum_size_permille = 200;
  static constexpr std::size_t maximum_text_bytes = 400;

  Title(TitleId id, std::string text, TimeRange range, int size_permille = 70,
        std::uint32_t color_rgba = 0xFFFFFFFFu, TitleHAlign horizontal = TitleHAlign::Center,
        TitleVAlign vertical = TitleVAlign::Bottom, bool shadow = true);

  [[nodiscard]] TitleId id() const noexcept { return id_; }
  [[nodiscard]] const std::string& text() const noexcept { return text_; }
  [[nodiscard]] TimeRange range() const noexcept { return range_; }
  [[nodiscard]] MediaTime start() const noexcept { return range_.start(); }
  [[nodiscard]] MediaTime end() const noexcept { return range_.end(); }
  [[nodiscard]] int size_permille() const noexcept { return size_permille_; }
  [[nodiscard]] std::uint32_t color_rgba() const noexcept { return color_rgba_; }
  [[nodiscard]] TitleHAlign horizontal() const noexcept { return horizontal_; }
  [[nodiscard]] TitleVAlign vertical() const noexcept { return vertical_; }
  [[nodiscard]] bool shadow() const noexcept { return shadow_; }

  // Value-style editing: every "with" returns a validated copy.
  [[nodiscard]] Title with_text(std::string text) const;
  [[nodiscard]] Title with_range(TimeRange range) const;
  [[nodiscard]] Title with_style(int size_permille, std::uint32_t color_rgba, TitleHAlign horizontal,
                                 TitleVAlign vertical, bool shadow) const;

  // The title is on screen in [start, end).
  [[nodiscard]] bool active_at(MediaTime time) const;
  // Everything that changes how it LOOKS (not when): used as a raster cache identity.
  [[nodiscard]] std::string look_key() const;

private:
  TitleId id_;
  std::string text_;
  TimeRange range_;
  int size_permille_;
  std::uint32_t color_rgba_;
  TitleHAlign horizontal_;
  TitleVAlign vertical_;
  bool shadow_;
};

} // namespace pulsar::domain
