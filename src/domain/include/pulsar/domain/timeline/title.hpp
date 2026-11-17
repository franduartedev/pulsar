#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/time_range.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace pulsar::domain {

struct TitleIdTag;
using TitleId = detail::StrongId<TitleIdTag>;

enum class TitleHAlign : std::uint8_t { Left, Center, Right };
enum class TitleVAlign : std::uint8_t { Top, Middle, Bottom };
// A box behind the text (None = just text). Colours are fixed so the look stays consistent.
enum class TitleBackground : std::uint8_t { None, Dark, Light };
enum class TitlePreset : std::uint8_t { Classic, Hero, LowerThird, StatCard, ChapterLocation };
enum class TitleAnimation : std::uint8_t { None, Fade, SlideFromLeft, SlideFromRight, SlideFromBottom, ScaleReveal };

// Motion is evaluated against timeline time, never a UI timer. Durations are
// normalised by the compositor when a short title cannot fit both phases.
struct TitleMotion {
  TitleAnimation in{TitleAnimation::None};
  TitleAnimation out{TitleAnimation::None};
  MediaTime in_duration{0, {1, 1}};
  MediaTime out_duration{0, {1, 1}};
  friend bool operator==(const TitleMotion&, const TitleMotion&) = default;
};

// Free placement: the CENTRE of the title block (text plus its box), in permille of the output
// frame width / height. Resolution independent like the size. Absent = the alignment presets.
struct TitlePosition {
  int x_permille = 500;
  int y_permille = 500;
  friend bool operator==(const TitlePosition&, const TitlePosition&) = default;
};

// A static text overlay on the video. Pure data: no fonts, pixels or UI types. The size is
// stored in permille of the output frame HEIGHT, so the same title looks the same at 720p,
// 1080p or in the preview (layout is resolution independent).
class Title {
public:
  static constexpr int minimum_size_permille = 20;
  static constexpr int maximum_size_permille = 200;
  static constexpr std::size_t maximum_text_bytes = 400;
  static constexpr int minimum_opacity_percent = 10;
  static constexpr int maximum_opacity_percent = 100;

  Title(TitleId id, std::string text, TimeRange range, int size_permille = 70,
        std::uint32_t color_rgba = 0xFFFFFFFFu, TitleHAlign horizontal = TitleHAlign::Center,
         TitleVAlign vertical = TitleVAlign::Bottom, bool shadow = true,
         TitleBackground background = TitleBackground::None, int opacity_percent = 100,
         std::optional<TitlePosition> position = std::nullopt,
         TitlePreset preset = TitlePreset::Classic, std::string secondary_text = {},
         std::uint32_t accent_rgba = 0x59D9FFFFu, TitleMotion motion = {});

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
  [[nodiscard]] TitleBackground background() const noexcept { return background_; }
  // Overall opacity of text, shadow and box, in percent.
  [[nodiscard]] int opacity_percent() const noexcept { return opacity_percent_; }
  [[nodiscard]] const std::optional<TitlePosition>& position() const noexcept { return position_; }
  [[nodiscard]] TitlePreset preset() const noexcept { return preset_; }
  [[nodiscard]] const std::string& secondary_text() const noexcept { return secondary_text_; }
  [[nodiscard]] std::uint32_t accent_rgba() const noexcept { return accent_rgba_; }
  [[nodiscard]] const TitleMotion& motion() const noexcept { return motion_; }

  // Value-style editing: every "with" returns a validated copy.
  [[nodiscard]] Title with_text(std::string text) const;
  [[nodiscard]] Title with_range(TimeRange range) const;
  [[nodiscard]] Title with_style(int size_permille, std::uint32_t color_rgba, TitleHAlign horizontal,
                                 TitleVAlign vertical, bool shadow) const;
  [[nodiscard]] Title with_look(TitleBackground background, int opacity_percent) const;
  [[nodiscard]] Title with_position(std::optional<TitlePosition> position) const;
  [[nodiscard]] Title with_graphics(TitlePreset preset, std::string secondary_text,
                                    std::uint32_t accent_rgba) const;
  [[nodiscard]] Title with_motion(TitleMotion motion) const;

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
  TitleBackground background_;
  int opacity_percent_;
  std::optional<TitlePosition> position_;
  TitlePreset preset_;
  std::string secondary_text_;
  std::uint32_t accent_rgba_;
  TitleMotion motion_;
};

} // namespace pulsar::domain
