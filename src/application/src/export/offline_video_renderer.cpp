#include <pulsar/application/export/offline_video_renderer.hpp>
#include <pulsar/application/composition/composition_plan.hpp>
#include <pulsar/application/composition/visual_composition_resolver.hpp>
#include <pulsar/application/playback/frame_coverage.hpp>

#include <algorithm>
#include <chrono>
#include <limits>
#include <sstream>
#include <type_traits>
#include <utility>

namespace pulsar::application::exporting {
namespace {

[[nodiscard]] std::size_t checked_buffer_size(int width, int height, int stride) {
  if (width <= 0 || height <= 0 || stride < static_cast<std::int64_t>(width) * 4) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::UnsupportedFrame,
                                  "offline video frame has invalid geometry"};
  }
  const auto size = static_cast<std::uint64_t>(stride) * static_cast<std::uint64_t>(height);
  if (size > std::numeric_limits<std::size_t>::max()) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::ConversionFailed,
                                  "offline video frame buffer size overflows"};
  }
  return static_cast<std::size_t>(size);
}

} // namespace

struct OfflineVideoRenderer::ActiveSource {
  domain::ClipId clip_id{domain::ClipId{1}};
  domain::MediaId media_id{domain::MediaId{1}};
  int video_stream_index{-1};
  domain::MediaTime last_source_time{domain::MediaTime::zero()};
  std::unique_ptr<OfflineVideoSource> source;
  std::optional<OfflineVideoSourceFrame> candidate;
  std::optional<OfflineVideoSourceFrame> lookahead;
  struct ScaledCache {
    std::shared_ptr<const OfflineVideoPixelBuffer> source;
    std::int64_t width{0};
    std::int64_t height{0};
    OfflineVideoPixelBuffer scaled;
  };
  std::optional<ScaledCache> scaled_cache;
};

OfflineVideoRenderer::OfflineVideoRenderer(const ExportPlan& plan,
                                           OfflineVideoSourceFactory& source_factory,
                                           OfflineVideoRenderOptions options,
                                           std::function<bool()> is_cancelled)
    : snapshot_{plan.snapshot()}, context_{plan.visual_context()}, export_frame_count_{plan.frame_count()},
      source_factory_{source_factory}, options_{options},
      is_cancelled_{std::move(is_cancelled)} {}

OfflineVideoRenderer::OfflineVideoRenderer(const ProjectSnapshot& snapshot, VisualRenderContext context,
                                           OfflineVideoSourceFactory& source_factory,
                                           OfflineVideoRenderOptions options,
                                           std::function<bool()> is_cancelled)
    : snapshot_{snapshot}, context_{std::move(context)}, source_factory_{source_factory},
      options_{options}, is_cancelled_{std::move(is_cancelled)} {}

OfflineVideoRenderer::~OfflineVideoRenderer() = default;

void OfflineVideoRenderer::update_live_frame_metric() noexcept {
  std::uint64_t live = 0;
  for (const auto& [id, active] : active_sources_) {
    (void)id;
    live += (active->candidate.has_value() ? 1 : 0) + (active->lookahead.has_value() ? 1 : 0);
    if (active->scaled_cache &&
        (!active->candidate || active->candidate->image != active->scaled_cache->source) &&
        (!active->lookahead || active->lookahead->image != active->scaled_cache->source)) ++live;
  }
  metrics_.peak_live_source_frames = std::max(metrics_.peak_live_source_frames, live);
  metrics_.peak_active_decoders = std::max(metrics_.peak_active_decoders,
      static_cast<std::uint64_t>(active_sources_.size()));
}

void OfflineVideoRenderer::check_cancelled() const {
  if (is_cancelled_ && is_cancelled_()) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::Cancelled, "export cancelled"};
  }
}

void OfflineVideoRenderer::open_source(domain::TrackId track_id, const VisualLayerPlan& layer,
                                       domain::MediaTime source_time) {
  check_cancelled();
  (void)track_id;
  active_sources_.erase(layer.clip_id.value());
  std::unique_ptr<OfflineVideoSource> source;
  try {
    source = source_factory_.open(layer.source_locator, layer.video_stream_index);
  } catch (const OfflineVideoRenderError&) {
    throw;
  } catch (const std::exception& error) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::SourceOpenFailed, error.what()};
  }
  if (!source) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::SourceOpenFailed,
                                  "offline video source factory returned no source"};
  }

  ++metrics_.decoder_opens;
  ++metrics_.seeks;
  OfflineVideoSeekResult seek_result;
  try {
    seek_result = source->seek_to_presentation_timestamp(source_time);
  } catch (const OfflineVideoRenderError&) {
    throw;
  } catch (...) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::SeekFailed,
                                  "offline video source seek failed"};
  }
  metrics_.decoded_source_frames += seek_result.frames_decoded_after_seek;
  metrics_.conversion_count += seek_result.frames_decoded_after_seek;
  active_sources_[layer.clip_id.value()] = std::make_unique<ActiveSource>(ActiveSource{
      layer.clip_id,
      layer.media_id,
      layer.video_stream_index,
      source_time,
      std::move(source),
      std::move(seek_result.candidate),
      std::move(seek_result.lookahead),
      std::nullopt,
  });
  update_live_frame_metric();
}

const OfflineVideoSourceFrame* OfflineVideoRenderer::select_source_frame(
    domain::TrackId track_id, const VisualLayerPlan& layer, domain::MediaTime source_time) {
  const auto found = active_sources_.find(layer.clip_id.value());
  const auto* active_source = found == active_sources_.end() ? nullptr : found->second.get();
  const bool same_segment =
      active_source && active_source->clip_id == layer.clip_id &&
      active_source->media_id == layer.media_id &&
      active_source->video_stream_index == layer.video_stream_index &&
      source_time >= active_source->last_source_time;
  if (!same_segment) {
    open_source(track_id, layer, source_time);
  }

  auto& active = *active_sources_.at(layer.clip_id.value());
  active.last_source_time = source_time;

  auto check_timestamp = [](const OfflineVideoSourceFrame& frame) {
    if (!frame.presentation_timestamp.has_value()) {
      throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::TimestampMissing,
                                    "offline video frame has no presentation timestamp"};
    }
    if (!frame.image) {
      throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::UnsupportedFrame,
                                    "offline video frame has no pixel buffer"};
    }
  };

  if (active.candidate.has_value()) check_timestamp(*active.candidate);
  if (active.lookahead.has_value()) check_timestamp(*active.lookahead);

  while (active.lookahead && (!active.candidate || frame_coverage(source_time,
      *active.candidate->presentation_timestamp, active.candidate->presentation_duration,
      active.lookahead->presentation_timestamp, false).kind == FrameCoverageKind::Advance)) {
    if (!active.candidate && source_time < *active.lookahead->presentation_timestamp) return nullptr;
    check_cancelled();
    active.candidate = std::move(active.lookahead);
    active.lookahead.reset();
    try {
      active.lookahead = active.source->decode_next();
    } catch (const OfflineVideoRenderError&) {
      throw;
    } catch (...) {
      throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::DecodeFailed,
                                    "offline video source decode failed"};
    }
    if (active.lookahead.has_value()) {
      ++metrics_.decoded_source_frames;
      ++metrics_.conversion_count;
    }
    update_live_frame_metric();
    if (active.lookahead.has_value()) check_timestamp(*active.lookahead);
  }

  if (!active.candidate.has_value()) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::SourceEndedEarly,
                                  "offline video source ended before the requested time"};
  }
  const auto coverage = frame_coverage(source_time, *active.candidate->presentation_timestamp,
      active.candidate->presentation_duration,
      active.lookahead ? active.lookahead->presentation_timestamp : std::nullopt, !active.lookahead);
  if (coverage.kind == FrameCoverageKind::Gap) return nullptr;
  if (coverage.kind != FrameCoverageKind::Covered) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::SourceEndedEarly,
                                  "offline video source ended before the requested interval"};
  }
  update_live_frame_metric();
  return &*active.candidate;
}

void OfflineVideoRenderer::compose_frame(composition::RgbaSurface& canvas, ActiveSource& active,
                                         const OfflineVideoSourceFrame& source_frame,
                                         const domain::VisualTransform& transform) {
  const auto& source = *source_frame.image;
  const auto source_size = checked_buffer_size(source.width, source.height, source.stride_bytes);
  if (source.pixels.size() < source_size) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::UnsupportedFrame,
                                  "offline video source pixel buffer is truncated"};
  }

  const auto output_width = context_.width;
  const auto output_height = context_.height;
  const auto geometry = composition::fit_geometry(source.width, source.height, static_cast<int>(output_width),
                                                  static_cast<int>(output_height));
  // Default transform keeps the exact E1 opaque path (bars included, top wins).
  if (transform.is_default()) {
    const composition::RgbaSurface* placed = &source;
    const auto scaled_width = static_cast<std::int64_t>(geometry.scaled_width);
    const auto scaled_height = static_cast<std::int64_t>(geometry.scaled_height);
    if (scaled_width != source.width || scaled_height != source.height) {
      if (options_.scaler != nullptr) {
        if (!active.scaled_cache || active.scaled_cache->source != source_frame.image || active.scaled_cache->width != scaled_width ||
            active.scaled_cache->height != scaled_height) {
          active.scaled_cache.reset();
          const auto start = std::chrono::steady_clock::now();
          active.scaled_cache = ActiveSource::ScaledCache{source_frame.image, scaled_width, scaled_height,
              options_.scaler->scale(source, static_cast<int>(scaled_width), static_cast<int>(scaled_height))};
          metrics_.scale_nanoseconds += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::steady_clock::now() - start).count());
        }
        placed = &active.scaled_cache->scaled;
      }
    }
    if (placed != &source) {
      if (placed->width != scaled_width || placed->height != scaled_height) {
        throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::ConversionFailed, "offline frame scaler returned an unexpected picture"};
      }
    }
    try {
      const auto start = std::chrono::steady_clock::now();
      composition::composite_opaque_layer(canvas, *placed, geometry);
      metrics_.composite_nanoseconds += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - start).count());
    } catch (const std::invalid_argument& error) {
      throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::ConversionFailed, error.what()};
    }
    return;
  }

  if (transform.opacity <= 0.0) return; // fully transparent: lower layers show through
  // Transformed reference path: fit without bars, then bilinear source-over.
  const composition::RgbaSurface* fitted = &source;
  composition::RgbaSurface fitted_owned;
  const auto scaled_width = static_cast<std::int64_t>(geometry.scaled_width);
  const auto scaled_height = static_cast<std::int64_t>(geometry.scaled_height);
  if (scaled_width != source.width || scaled_height != source.height) {
    if (options_.scaler != nullptr) {
      if (!active.scaled_cache || active.scaled_cache->source != source_frame.image || active.scaled_cache->width != scaled_width ||
          active.scaled_cache->height != scaled_height) {
        active.scaled_cache.reset();
        const auto start = std::chrono::steady_clock::now();
        active.scaled_cache = ActiveSource::ScaledCache{source_frame.image, scaled_width, scaled_height,
            options_.scaler->scale(source, static_cast<int>(scaled_width), static_cast<int>(scaled_height))};
        metrics_.scale_nanoseconds += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start).count());
      }
      fitted = &active.scaled_cache->scaled;
    } else {
      try {
        fitted_owned = composition::fitted_image_nearest(source, geometry);
      } catch (const std::invalid_argument& error) {
        throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::ConversionFailed, error.what()};
      }
      fitted = &fitted_owned;
    }
  }
  if (fitted->width != scaled_width || fitted->height != scaled_height) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::ConversionFailed,
                                  "fitted picture has unexpected size"};
  }
  try {
    const auto start = std::chrono::steady_clock::now();
    composition::composite_transformed_layer(canvas, *fitted, transform);
    metrics_.composite_nanoseconds += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count());
  } catch (const std::invalid_argument& error) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::ConversionFailed, error.what()};
  }
}

OfflineVideoFrame OfflineVideoRenderer::render_frame(domain::FrameNumber frame_index) {
  if (frame_index.value() != next_frame_index_) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::InternalInvariant,
                                  "offline video frames must be rendered sequentially"};
  }
  if (!export_frame_count_ || frame_index.value() < 0 || frame_index.value() >= *export_frame_count_) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::InternalInvariant,
                                  "offline video frame index is outside the export plan"};
  }

  const auto timeline_time = output_frame_time(frame_index, context_.frame_rate);
  auto output = render_time(frame_index, timeline_time);
  ++next_frame_index_;
  return output;
}

OfflineVideoFrame OfflineVideoRenderer::render_at(domain::MediaTime timeline_time) {
  if (timeline_time < domain::MediaTime::zero() || timeline_time >= context_.editorial_duration) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::InternalInvariant,
                                  "preview time is outside the editorial range"};
  }
  return render_time(domain::FrameNumber{output_frame_index_at(timeline_time, context_.frame_rate,
      domain::RoundingMode::Floor)}, timeline_time);
}

OfflineVideoFrame OfflineVideoRenderer::render_time(domain::FrameNumber frame_index,
                                                    domain::MediaTime timeline_time) {
  check_cancelled();
  CompositionPlan plan;
  try {
    plan = evaluate_composition_plan(snapshot_, context_, timeline_time);
  } catch (const CompositionPlanError& error) {
    if (error.code() == CompositionPlanErrorCode::MissingMedia) {
      throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::MissingMedia,
                                    "offline video clip references missing media"};
    }
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::InternalInvariant,
                                  "timeline ended before the export plan"};
  }
  std::ostringstream diagnostic;
  diagnostic << "frame_index=" << frame_index.value()
             << " timeline_time=" << static_cast<double>(timeline_time);
  // Evict inactive tracks before opening new ones, including their scaled caches.
  for (auto it = active_sources_.begin(); it != active_sources_.end();) {
    const bool still_active = std::any_of(plan.layers.begin(), plan.layers.end(),
        [&](const VisualLayerPlan& layer) { return layer.clip_id.value() == it->first; });
    if (!still_active) it = active_sources_.erase(it);
    else ++it;
  }
  metrics_.peak_active_layers =
      std::max(metrics_.peak_active_layers, static_cast<std::uint64_t>(plan.layers.size()));

  try {
    auto canvas = composition::black_surface(context_.width, context_.height);
    for (const auto& layer : plan.layers) {
      check_cancelled();
      diagnostic << " track_id=" << layer.track_id.value();
      diagnostic << " clip_id=" << layer.clip_id.value() << " media_id=" << layer.media_id.value()
                 << " source_time=" << static_cast<double>(layer.source_time);
      if (layer.transform.opacity <= 0.0) continue;
      const auto* frame = select_source_frame(layer.track_id, layer, layer.source_time);
      if (frame) {
        compose_frame(canvas, *active_sources_.at(layer.clip_id.value()), *frame, layer.transform);
      }
    }
    check_cancelled();
    OfflineVideoFrame output{frame_index, timeline_time, canvas.width, canvas.height,
        canvas.stride_bytes, OfflinePixelFormat::Rgba8888, std::move(canvas.pixels)};
    if (options_.titles != nullptr && !snapshot_.titles.empty()) {
      // Titles ride on the finished output frame: the same compositor the preview uses.
      options_.titles->composite(composition::RgbaCanvas{output.width, output.height, output.stride_bytes, output.pixels.data()},
                                  snapshot_.titles, timeline_time);
    }
    ++metrics_.output_frames;
    return output;
  } catch (OfflineVideoRenderError& error) {
    for (const auto& [id, active] : active_sources_) {
      diagnostic << " source_track_id=" << id;
      if (active->candidate.has_value() &&
          active->candidate->presentation_timestamp.has_value()) {
        diagnostic << " candidate_pts="
                   << static_cast<double>(*active->candidate->presentation_timestamp);
      }
      if (active->lookahead.has_value() &&
          active->lookahead->presentation_timestamp.has_value()) {
        diagnostic << " lookahead_pts="
                   << static_cast<double>(*active->lookahead->presentation_timestamp);
      }
    }
    error.set_diagnostic_context(diagnostic.str());
    throw;
  }
}

} // namespace pulsar::application::exporting
