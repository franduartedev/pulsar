#include <pulsar/application/export/offline_video_renderer.hpp>
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
    : plan_{plan}, source_factory_{source_factory}, options_{options},
      is_cancelled_{std::move(is_cancelled)} {}

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

void OfflineVideoRenderer::open_source(domain::TrackId track_id, const ResolvedClip& clip,
                                      domain::MediaTime source_time) {
  check_cancelled();
  active_sources_.erase(track_id.value());
  std::unique_ptr<OfflineVideoSource> source;
  try {
    source = source_factory_.open(clip.source_locator, clip.video_stream_index);
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
  active_sources_[track_id.value()] = std::make_unique<ActiveSource>(ActiveSource{
      clip.clip_id,
      clip.media_id,
      clip.video_stream_index,
      source_time,
      std::move(source),
      std::move(seek_result.candidate),
      std::move(seek_result.lookahead),
      std::nullopt,
  });
  update_live_frame_metric();
}

const OfflineVideoSourceFrame* OfflineVideoRenderer::select_source_frame(
    domain::TrackId track_id, const ResolvedClip& clip, domain::MediaTime source_time) {
  const auto found = active_sources_.find(track_id.value());
  const auto* active_source = found == active_sources_.end() ? nullptr : found->second.get();
  const bool same_segment =
      active_source && active_source->clip_id == clip.clip_id &&
      active_source->media_id == clip.media_id &&
      active_source->video_stream_index == clip.video_stream_index &&
      source_time >= active_source->last_source_time;
  if (!same_segment) {
    open_source(track_id, clip, source_time);
  }

  auto& active = *active_sources_.at(track_id.value());
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
                                         const OfflineVideoSourceFrame& source_frame) {
  const auto& source = *source_frame.image;
  const auto source_size = checked_buffer_size(source.width, source.height, source.stride_bytes);
  if (source.pixels.size() < source_size) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::UnsupportedFrame,
                                  "offline video source pixel buffer is truncated"};
  }

  const auto output_width = plan_.output_width();
  const auto output_height = plan_.output_height();
  const auto geometry = composition::fit_geometry(source.width, source.height, static_cast<int>(output_width),
                                                  static_cast<int>(output_height));
  const auto scaled_width = static_cast<std::int64_t>(geometry.scaled_width);
  const auto scaled_height = static_cast<std::int64_t>(geometry.scaled_height);

  // Same size: straight row copy. Different size: the media adapter's scaler (bicubic/lanczos
  // through libswscale) when provided, otherwise the basic nearest-neighbour fallback.
  const OfflineVideoPixelBuffer* placed = &source;
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

}

OfflineVideoFrame OfflineVideoRenderer::render_frame(domain::FrameNumber frame_index) {
  if (frame_index.value() != next_frame_index_) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::InternalInvariant,
                                  "offline video frames must be rendered sequentially"};
  }
  if (frame_index.value() < 0 || frame_index.value() >= plan_.frame_count()) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::InternalInvariant,
                                  "offline video frame index is outside the export plan"};
  }

  const auto timeline_time = output_frame_time(frame_index, plan_.frame_rate());
  check_cancelled();
  const auto composition = resolve_visual_composition(plan_.snapshot(), timeline_time);
  std::ostringstream diagnostic;
  diagnostic << "frame_index=" << frame_index.value()
             << " timeline_time=" << static_cast<double>(timeline_time);
  std::uint64_t active_layers = 0;
  // Evict inactive tracks before opening new ones, including their scaled caches.
  for (const auto& layer : composition.layers) {
    if (std::holds_alternative<ResolvedClip>(layer.source)) ++active_layers;
    else active_sources_.erase(layer.track_id.value());
  }
  metrics_.peak_active_layers = std::max(metrics_.peak_active_layers, active_layers);

  try {
    auto canvas = composition::black_surface(static_cast<int>(plan_.output_width()),
                                              static_cast<int>(plan_.output_height()));
    for (const auto& layer : composition.layers) {
      check_cancelled();
      diagnostic << " track_id=" << layer.track_id.value();
      if (const auto* clip = std::get_if<ResolvedClip>(&layer.source)) {
        diagnostic << " clip_id=" << clip->clip_id.value() << " media_id=" << clip->media_id.value()
                   << " source_time=" << static_cast<double>(clip->source_time);
        const auto* frame = select_source_frame(layer.track_id, *clip, clip->source_time);
        if (frame) compose_frame(canvas, *active_sources_.at(layer.track_id.value()), *frame);
      } else if (std::holds_alternative<MissingClipMedia>(layer.source)) {
        throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::MissingMedia,
                                      "offline video clip references missing media"};
      } else if (std::holds_alternative<TimelineEnded>(layer.source)) {
        throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::InternalInvariant,
                                      "timeline ended before the export plan"};
      }
    }
    check_cancelled();
    OfflineVideoFrame output{frame_index, timeline_time, canvas.width, canvas.height,
        canvas.stride_bytes, OfflinePixelFormat::Rgba8888, std::move(canvas.pixels)};
    if (options_.titles != nullptr && !plan_.snapshot().titles.empty()) {
      // Titles ride on the finished output frame: the same compositor the preview uses.
      options_.titles->composite(composition::RgbaCanvas{output.width, output.height, output.stride_bytes, output.pixels.data()},
                                 plan_.snapshot().titles, timeline_time);
    }
    ++next_frame_index_;
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
