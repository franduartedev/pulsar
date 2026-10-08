#pragma once

#include <pulsar/application/composition/title_compositor.hpp>

#include <QString>

// Draws a title with the bundled font (assets/fonts/Inter-Bold.otf) into premultiplied RGBA.
// Layout is expressed relative to the canvas HEIGHT, so preview and export share it exactly:
// both call this one class (through the same TitleCompositor) for the same canvas size.
// Safe to call from the UI thread and from the export thread (it only paints into QImage).
class QtTitleRasterizer final : public pulsar::application::composition::TitleRasterizer {
public:
  [[nodiscard]] std::optional<pulsar::application::composition::TitleRaster> rasterize(
      const pulsar::domain::Title& title, int canvas_width, int canvas_height) override;

  // Loads the bundled font once (idempotent). Returns the family used for titles.
  static QString fontFamily();
};
