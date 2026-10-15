#include <pulsar/application/composition/visual_render_context.hpp>
#include <pulsar/application/composition/project_canvas.hpp>
#include <pulsar/application/composition/visual_compositor.hpp>

#include <stdexcept>

namespace pulsar::application {
VisualRenderContext VisualRenderContext::build(const ProjectSnapshot& snapshot,
                                               int max_width, int max_height) {
  validate_visual_track_ids(snapshot.timeline, snapshot.video_track_id, snapshot.visual_track_ids);
  const auto canvas = effective_project_canvas(snapshot);
  if (!canvas) throw std::invalid_argument{"project has no established visual canvas"};
  if (canvas->width <= 0 || canvas->height <= 0 || canvas->width > 32768 || canvas->height > 32768 ||
      static_cast<std::int64_t>(canvas->width) * canvas->height > 64'000'000)
    throw std::invalid_argument{"invalid project canvas dimensions"};
  int width = canvas->width + (canvas->width & 1);
  int height = canvas->height + (canvas->height & 1);
  if (max_width > 0 && max_height > 0 && (width > max_width || height > max_height)) {
    const auto fit = composition::fit_geometry(width, height, max_width, max_height);
    width = fit.scaled_width + (fit.scaled_width & 1);
    height = fit.scaled_height + (fit.scaled_height & 1);
  }
  return {width, height, canvas->frame_rate, snapshot.timeline.duration()};
}
} // namespace pulsar::application
