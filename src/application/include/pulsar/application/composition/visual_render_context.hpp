#pragma once

#include <pulsar/application/project/project_snapshot.hpp>

namespace pulsar::application {

// Render-only contract: no media availability or audio/export eligibility checks.
struct VisualRenderContext {
  int width;
  int height;
  domain::FrameRate frame_rate;
  domain::MediaTime editorial_duration;

  [[nodiscard]] static VisualRenderContext build(const ProjectSnapshot& snapshot,
      int max_width = 0, int max_height = 0);
};

} // namespace pulsar::application
