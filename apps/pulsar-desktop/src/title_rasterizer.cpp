#include "title_rasterizer.hpp"

#include <pulsar/project_file/runtime_resources.hpp>

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QRect>
#include <QRectF>
#include <QStringList>

#include <algorithm>
#include <cmath>

QString QtTitleRasterizer::fontFamily() {
  static QMutex mutex;
  static QString family;
  static bool loaded = false;
  QMutexLocker lock{&mutex};
  if (loaded) return family;
  loaded = true;
  const auto assets = pulsar::project_file::assets_directory();
  if (!assets.empty()) {
    const QString path = QString::fromStdString((assets / "fonts" / "Inter-Bold.otf").string());
    const int id = QFontDatabase::addApplicationFont(path);
    if (id >= 0) {
      const auto families = QFontDatabase::applicationFontFamilies(id);
      if (!families.isEmpty()) family = families.front();
    }
  }
  if (family.isEmpty()) family = QStringLiteral("Sans Serif");   // bundle incomplete: still draws something
  return family;
}

std::optional<pulsar::application::composition::TitleRaster> QtTitleRasterizer::rasterize(
    const pulsar::domain::Title& title, int canvas_width, int canvas_height) {
  return rasterizeWithBlock(title, canvas_width, canvas_height, nullptr);
}

std::optional<pulsar::application::composition::TitleRaster> QtTitleRasterizer::rasterizeWithBlock(
    const pulsar::domain::Title& title, int canvas_width, int canvas_height, QRect* block) {
  int block_x = 0, block_y = 0, block_w = 0, block_h = 0;
  using pulsar::domain::TitleHAlign;
  using pulsar::domain::TitleVAlign;
  if (canvas_width <= 0 || canvas_height <= 0) return std::nullopt;
  const int font_px = std::max(8, static_cast<int>(std::lround(title.size_permille() / 1000.0 * canvas_height)));
  QFont font{fontFamily()};
  font.setPixelSize(font_px);
  font.setWeight(QFont::Bold);
  font.setHintingPreference(QFont::PreferNoHinting);   // same outlines at every resolution
  font.setStyleStrategy(QFont::PreferAntialias);

  // Presets remain one raster and one title object. Their text and editorial
  // shapes are drawn here once; frame-time motion is applied later by the
  // shared compositor, preserving preview/export parity and cacheability.
  if (title.preset() != pulsar::domain::TitlePreset::Classic) {
    const QString primary = QString::fromUtf8(title.text().data(), static_cast<qsizetype>(title.text().size()));
    const QString secondary = QString::fromUtf8(title.secondary_text().data(), static_cast<qsizetype>(title.secondary_text().size()));
    const int safe_x = std::max(12, static_cast<int>(std::lround(canvas_width * 0.06)));
    const int safe_y = std::max(12, static_cast<int>(std::lround(canvas_height * 0.07)));
    const bool hero = title.preset() == pulsar::domain::TitlePreset::Hero;
    const bool card = title.preset() == pulsar::domain::TitlePreset::LowerThird ||
                      title.preset() == pulsar::domain::TitlePreset::StatCard;
    const int max_width = std::max(48, static_cast<int>(canvas_width * (hero ? 0.86 : card ? 0.72 : 0.82)));
    QFont secondary_font{font};
    secondary_font.setPixelSize(std::max(10, static_cast<int>(std::lround(font_px * (hero ? 0.30 : 0.42)))));
    secondary_font.setWeight(QFont::DemiBold);
    QImage measure_image{1, 1, QImage::Format_ARGB32_Premultiplied};
    QPainter measure{&measure_image};
    measure.setFont(font);
    const QRectF primary_bounds = measure.boundingRect(QRectF{0.0, 0.0, static_cast<qreal>(max_width), 100000.0}, Qt::TextWordWrap, primary);
    measure.setFont(secondary_font);
    const QRectF secondary_bounds = secondary.isEmpty() ? QRectF{} :
        measure.boundingRect(QRectF{0.0, 0.0, static_cast<qreal>(max_width), 100000.0}, Qt::TextWordWrap, secondary);
    measure.end();
    const int pad_x = std::max(10, static_cast<int>(std::lround(font_px * (card ? 0.48 : 0.16))));
    const int pad_y = std::max(8, static_cast<int>(std::lround(font_px * (card ? 0.34 : 0.14))));
    const int accent_h = std::max(3, static_cast<int>(std::lround(font_px * 0.075)));
    const int gap = secondary.isEmpty() ? 0 : std::max(4, static_cast<int>(std::lround(font_px * 0.14)));
    const int content_w = std::max(static_cast<int>(std::ceil(primary_bounds.width())), static_cast<int>(std::ceil(secondary_bounds.width())));
    const int content_h = static_cast<int>(std::ceil(primary_bounds.height() + secondary_bounds.height())) + gap;
    const int shadow_pad = title.shadow() ? std::max(2, static_cast<int>(std::lround(font_px * 0.06))) : 0;
    const int image_w = std::min(canvas_width, content_w + 2 * (pad_x + shadow_pad));
    const int image_h = content_h + 2 * (pad_y + shadow_pad) + accent_h + std::max(3, gap);
    if (image_w <= 0 || image_h <= 0 || static_cast<long long>(image_w) * image_h > 64LL * 1024 * 1024) return std::nullopt;
    QImage image{image_w, image_h, QImage::Format_RGBA8888_Premultiplied};
    image.fill(Qt::transparent);
    QPainter painter{&image};
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setOpacity(title.opacity_percent() / 100.0);
    const QRectF visible{static_cast<double>(shadow_pad), static_cast<double>(shadow_pad),
                         static_cast<double>(image_w - 2 * shadow_pad), static_cast<double>(image_h - 2 * shadow_pad)};
    if (card || title.background() != pulsar::domain::TitleBackground::None) {
      const bool light = title.background() == pulsar::domain::TitleBackground::Light;
      painter.setPen(Qt::NoPen);
      painter.setBrush(light ? QColor{255, 255, 255, 220} : QColor{8, 14, 25, card ? 205 : 165});
      painter.drawRoundedRect(visible, font_px * 0.18, font_px * 0.18);
    }
    const auto accent = title.accent_rgba();
    const QColor accent_color{static_cast<int>((accent >> 24) & 0xFF), static_cast<int>((accent >> 16) & 0xFF),
                              static_cast<int>((accent >> 8) & 0xFF), static_cast<int>(accent & 0xFF)};
    const double line_w = hero ? std::min<double>(content_w * 0.28, font_px * 3.0) : image_w - 2.0 * (pad_x + shadow_pad);
    const double line_x = hero ? (image_w - line_w) / 2.0 : pad_x + shadow_pad;
    painter.setPen(Qt::NoPen); painter.setBrush(accent_color);
    painter.drawRoundedRect(QRectF{line_x, static_cast<qreal>(pad_y + shadow_pad), line_w, static_cast<qreal>(accent_h)}, accent_h / 2.0, accent_h / 2.0);
    const int flags = (hero ? Qt::AlignHCenter : Qt::AlignLeft) | Qt::TextWordWrap;
    const QRectF primary_rect{static_cast<double>(pad_x + shadow_pad), static_cast<double>(pad_y + shadow_pad + accent_h + gap),
                              static_cast<double>(image_w - 2 * (pad_x + shadow_pad)), primary_bounds.height() + 3};
    if (title.shadow()) {
      painter.setFont(font); painter.setPen(QColor{0, 0, 0, 170});
      painter.drawText(primary_rect.translated(shadow_pad, shadow_pad), flags, primary);
    }
    const auto rgba = title.color_rgba();
    painter.setFont(font); painter.setPen(QColor{static_cast<int>((rgba >> 24) & 0xFF), static_cast<int>((rgba >> 16) & 0xFF),
                                                 static_cast<int>((rgba >> 8) & 0xFF), static_cast<int>(rgba & 0xFF)});
    painter.drawText(primary_rect, flags, primary);
    if (!secondary.isEmpty()) {
      const QRectF secondary_rect{primary_rect.left(), primary_rect.bottom() + gap,
                                  primary_rect.width(), secondary_bounds.height() + 3};
      painter.setFont(secondary_font); painter.setPen(card ? accent_color : QColor{220, 230, 240, 230});
      painter.drawText(secondary_rect, flags, secondary);
    }
    painter.end();
    const int full_w = image_w - 2 * shadow_pad, full_h = image_h - 2 * shadow_pad;
    int box_x = title.horizontal() == TitleHAlign::Left ? safe_x : title.horizontal() == TitleHAlign::Right ? canvas_width - safe_x - full_w : (canvas_width - full_w) / 2;
    int box_y = title.vertical() == TitleVAlign::Top ? safe_y : title.vertical() == TitleVAlign::Middle ? (canvas_height - full_h) / 2 : canvas_height - safe_y - full_h;
    if (const auto& free = title.position()) {
      box_x = std::clamp(static_cast<int>(std::lround(free->x_permille / 1000.0 * canvas_width - full_w / 2.0)), 0, std::max(0, canvas_width - full_w));
      box_y = std::clamp(static_cast<int>(std::lround(free->y_permille / 1000.0 * canvas_height - full_h / 2.0)), 0, std::max(0, canvas_height - full_h));
    }
    if (block != nullptr) *block = QRect{box_x, box_y, full_w, full_h};
    pulsar::application::composition::TitleRaster raster;
    raster.x = box_x - shadow_pad; raster.y = box_y - shadow_pad; raster.width = image_w; raster.height = image_h;
    raster.premultiplied_rgba.resize(static_cast<std::size_t>(image_w) * image_h * 4);
    for (int y = 0; y < image_h; ++y) std::copy_n(image.constScanLine(y), static_cast<std::size_t>(image_w) * 4,
        raster.premultiplied_rgba.data() + static_cast<std::size_t>(y) * image_w * 4);
    return raster;
  }

  const QString text = QString::fromUtf8(title.text().data(), static_cast<qsizetype>(title.text().size()));
  const int side_margin = static_cast<int>(std::lround(canvas_width * 0.05));
  const int vertical_margin = static_cast<int>(std::lround(canvas_height * 0.06));
  // A background box grows the title; keep the whole box inside the safe margins.
  const bool boxed = title.background() != pulsar::domain::TitleBackground::None;
  const int box_pad_x = boxed ? static_cast<int>(std::lround(font_px * 0.45)) : 0;
  const int box_pad_y = boxed ? static_cast<int>(std::lround(font_px * 0.25)) : 0;
  const double max_width = std::max(16, canvas_width - 2 * side_margin - 2 * box_pad_x);
  const int qt_alignment = title.horizontal() == TitleHAlign::Left ? Qt::AlignLeft
                          : title.horizontal() == TitleHAlign::Right ? Qt::AlignRight : Qt::AlignHCenter;
  const int flags = qt_alignment | Qt::TextWordWrap;

  // Measure on a scratch image so the final picture is exactly as large as the text.
  QImage scratch{1, 1, QImage::Format_ARGB32_Premultiplied};
  QPainter measure{&scratch};
  measure.setFont(font);
  const QRectF bounds = measure.boundingRect(QRectF{0, 0, max_width, 100000.0}, flags, text);
  measure.end();
  const int shadow_offset = title.shadow() ? std::max(1, static_cast<int>(std::lround(font_px * 0.05))) : 0;
  const int pad = static_cast<int>(std::ceil(font_px * 0.12)) + shadow_offset + 2;
  const int text_w = std::min(static_cast<int>(std::ceil(bounds.width())) + 2, canvas_width);
  const int text_h = static_cast<int>(std::ceil(bounds.height())) + 2;
  const int image_w = text_w + 2 * (pad + box_pad_x);
  const int image_h = text_h + 2 * (pad + box_pad_y);
  if (image_w <= 0 || image_h <= 0 || static_cast<long long>(image_w) * image_h > 64LL * 1024 * 1024) return std::nullopt;

  QImage image{image_w, image_h, QImage::Format_RGBA8888_Premultiplied};
  image.fill(Qt::transparent);
  QPainter painter{&image};
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setFont(font);
  painter.setOpacity(title.opacity_percent() / 100.0);   // text, shadow and box fade together
  if (boxed) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(title.background() == pulsar::domain::TitleBackground::Dark ? QColor{0, 0, 0, 165}
                                                                                  : QColor{255, 255, 255, 215});
    painter.drawRoundedRect(QRectF{static_cast<double>(pad), static_cast<double>(pad),
                                   static_cast<double>(text_w + 2 * box_pad_x), static_cast<double>(text_h + 2 * box_pad_y)},
                            font_px * 0.22, font_px * 0.22);
  }
  const QRectF draw_rect{static_cast<double>(pad + box_pad_x), static_cast<double>(pad + box_pad_y), static_cast<double>(text_w), static_cast<double>(text_h)};
  if (title.shadow()) {
    painter.setPen(QColor{0, 0, 0, 170});
    painter.drawText(draw_rect.translated(shadow_offset, shadow_offset), flags, text);
  }
  const std::uint32_t rgba = title.color_rgba();
  painter.setPen(QColor{static_cast<int>((rgba >> 24) & 0xFF), static_cast<int>((rgba >> 16) & 0xFF),
                        static_cast<int>((rgba >> 8) & 0xFF), static_cast<int>(rgba & 0xFF)});
  painter.drawText(draw_rect, flags, text);
  painter.end();

  pulsar::application::composition::TitleRaster raster;
  raster.width = image_w;
  raster.height = image_h;
  const int full_w = text_w + 2 * box_pad_x;     // the visible block: text plus its box, if any
  const int full_h = text_h + 2 * box_pad_y;
  int box_x = title.horizontal() == TitleHAlign::Left ? side_margin
             : title.horizontal() == TitleHAlign::Right ? canvas_width - side_margin - full_w
                                                        : (canvas_width - full_w) / 2;
  int box_y = title.vertical() == TitleVAlign::Top ? vertical_margin
             : title.vertical() == TitleVAlign::Middle ? (canvas_height - full_h) / 2
                                                      : canvas_height - vertical_margin - full_h;
  if (const auto& free = title.position()) {   // dragged placement: the block's centre, kept on screen
    box_x = std::clamp(static_cast<int>(std::lround(free->x_permille / 1000.0 * canvas_width - full_w / 2.0)), 0,
                       std::max(0, canvas_width - full_w));
    box_y = std::clamp(static_cast<int>(std::lround(free->y_permille / 1000.0 * canvas_height - full_h / 2.0)), 0,
                       std::max(0, canvas_height - full_h));
  }
  block_x = box_x; block_y = box_y; block_w = full_w; block_h = full_h;
  if (block != nullptr) *block = QRect{block_x, block_y, block_w, block_h};
  raster.x = box_x - pad;
  raster.y = box_y - pad;
  raster.premultiplied_rgba.resize(static_cast<std::size_t>(image_w) * image_h * 4);
  for (int y = 0; y < image_h; ++y) {
    std::copy_n(image.constScanLine(y), static_cast<std::size_t>(image_w) * 4,
                raster.premultiplied_rgba.data() + static_cast<std::size_t>(y) * image_w * 4);
  }
  return raster;
}
