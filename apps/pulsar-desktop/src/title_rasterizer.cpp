#include "title_rasterizer.hpp"

#include <pulsar/project_file/runtime_resources.hpp>

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
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
  using pulsar::domain::TitleHAlign;
  using pulsar::domain::TitleVAlign;
  if (canvas_width <= 0 || canvas_height <= 0) return std::nullopt;
  const int font_px = std::max(8, static_cast<int>(std::lround(title.size_permille() / 1000.0 * canvas_height)));
  QFont font{fontFamily()};
  font.setPixelSize(font_px);
  font.setWeight(QFont::Bold);
  font.setHintingPreference(QFont::PreferNoHinting);   // same outlines at every resolution
  font.setStyleStrategy(QFont::PreferAntialias);

  const QString text = QString::fromUtf8(title.text().data(), static_cast<qsizetype>(title.text().size()));
  const int side_margin = static_cast<int>(std::lround(canvas_width * 0.05));
  const int vertical_margin = static_cast<int>(std::lround(canvas_height * 0.06));
  const double max_width = std::max(16, canvas_width - 2 * side_margin);
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
  const int image_w = text_w + 2 * pad;
  const int image_h = text_h + 2 * pad;
  if (image_w <= 0 || image_h <= 0 || static_cast<long long>(image_w) * image_h > 64LL * 1024 * 1024) return std::nullopt;

  QImage image{image_w, image_h, QImage::Format_RGBA8888_Premultiplied};
  image.fill(Qt::transparent);
  QPainter painter{&image};
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setFont(font);
  const QRectF draw_rect{static_cast<double>(pad), static_cast<double>(pad), static_cast<double>(text_w), static_cast<double>(text_h)};
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
  const int box_x = title.horizontal() == TitleHAlign::Left ? side_margin
                   : title.horizontal() == TitleHAlign::Right ? canvas_width - side_margin - text_w
                                                              : (canvas_width - text_w) / 2;
  const int box_y = title.vertical() == TitleVAlign::Top ? vertical_margin
                   : title.vertical() == TitleVAlign::Middle ? (canvas_height - text_h) / 2
                                                            : canvas_height - vertical_margin - text_h;
  raster.x = box_x - pad;
  raster.y = box_y - pad;
  raster.premultiplied_rgba.resize(static_cast<std::size_t>(image_w) * image_h * 4);
  for (int y = 0; y < image_h; ++y) {
    std::copy_n(image.constScanLine(y), static_cast<std::size_t>(image_w) * 4,
                raster.premultiplied_rgba.data() + static_cast<std::size_t>(y) * image_w * 4);
  }
  return raster;
}
