#include <pulsar/application/export/offline_video_renderer.hpp>

#include <algorithm>
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

[[nodiscard]] int checked_rgba_stride(std::int64_t width) {
  if (width <= 0 || width > std::numeric_limits<int>::max() / 4) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::ConversionFailed,
                                  "output RGBA stride is invalid"};
  }
  return static_cast<int>(width * 4);
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
};

OfflineVideoRenderer::OfflineVideoRenderer(const ExportPlan& plan,
                                           OfflineVideoSourceFactory& source_factory,
                                           OfflineVideoRenderOptions options)
    : plan_{plan}, source_factory_{source_factory}, options_{options} {}

OfflineVideoRenderer::~OfflineVideoRenderer() = default;

void OfflineVideoRenderer::update_live_frame_metric() noexcept {
  const auto live = static_cast<std::uint64_t>(
      (active_source_ && active_source_->candidate.has_value() ? 1 : 0) +
      (active_source_ && active_source_->lookahead.has_value() ? 1 : 0));
  metrics_.peak_live_source_frames = std::max(metrics_.peak_live_source_frames, live);
}

void OfflineVideoRenderer::open_source(const ResolvedClip& clip, domain::MediaTime source_time) {
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
  active_source_ = std::make_unique<ActiveSource>(ActiveSource{
      clip.clip_id,
      clip.media_id,
      clip.video_stream_index,
      source_time,
      std::move(source),
      std::move(seek_result.candidate),
      std::move(seek_result.lookahead),
  });
  update_live_frame_metric();
}

const OfflineVideoSourceFrame& OfflineVideoRenderer::select_source_frame(
    const ResolvedClip& clip, domain::MediaTime source_time) {
  const bool same_segment =
      active_source_ && active_source_->clip_id == clip.clip_id &&
      active_source_->media_id == clip.media_id &&
      active_source_->video_stream_index == clip.video_stream_index &&
      source_time >= active_source_->last_source_time;
  if (!same_segment) {
    open_source(clip, source_time);
  }

  auto& active = *active_source_;
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

  while (active.lookahead.has_value() &&
         *active.lookahead->presentation_timestamp <= source_time) {
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

  if (!active.candidate.has_value() && active.lookahead.has_value()) {
    // The target is before the first physical frame. The first frame is the
    // only deterministic representation available for that interval.
    active.candidate = std::move(active.lookahead);
    active.lookahead.reset();
  }
  if (!active.candidate.has_value()) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::SourceEndedEarly,
                                  "offline video source ended before the requested time"};
  }
  if (*active.candidate->presentation_timestamp > source_time) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::SourceEndedEarly,
                                  "offline video source has no frame at the requested time"};
  }
  if (!active.lookahead.has_value() &&
      *active.candidate->presentation_timestamp < source_time) {
    if (!active.candidate->presentation_duration.has_value() ||
        *active.candidate->presentation_timestamp +
                *active.candidate->presentation_duration <=
            source_time) {
      // A demuxer is allowed to end immediately after the final physical
      // frame without exposing a duration for that frame.  The project
      // already validated the half-open source range against the media
      // duration, so the final decoded frame covers the remainder of this
      // clip range.  Do not require a synthetic PTS at every output-grid
      // instant.  An explicit duration still wins when the source provides
      // one, because it can prove an actual gap before the clip boundary.
      const auto nominal_output_duration = domain::MediaTime{
          1, domain::Rational{plan_.frame_rate().rational().denominator(),
                              plan_.frame_rate().rational().numerator()}};
      if (active.candidate->presentation_duration.has_value() ||
          source_time >= clip.source_range.end() ||
          *active.candidate->presentation_timestamp + nominal_output_duration <= source_time) {
        throw OfflineVideoRenderError{
            OfflineVideoRenderErrorCode::SourceEndedEarly,
            "offline video source ended before the requested interval"};
      }
    }
  }
  update_live_frame_metric();
  return *active.candidate;
}

OfflineVideoFrame OfflineVideoRenderer::black_frame(domain::FrameNumber frame_index,
                                                    domain::MediaTime timeline_time) const {
  const int stride = checked_rgba_stride(plan_.output_width());
  const auto size = checked_buffer_size(static_cast<int>(plan_.output_width()),
                                        static_cast<int>(plan_.output_height()), stride);
  std::vector<std::uint8_t> pixels(size, 0);
  for (std::size_t offset = 3; offset < pixels.size(); offset += 4) pixels[offset] = 255;
  return OfflineVideoFrame{frame_index, timeline_time, static_cast<int>(plan_.output_width()),
                           static_cast<int>(plan_.output_height()), stride,
                           OfflinePixelFormat::Rgba8888, std::move(pixels)};
}

OfflineVideoFrame OfflineVideoRenderer::compose_frame(
    domain::FrameNumber frame_index, domain::MediaTime timeline_time,
    const OfflineVideoSourceFrame& source_frame) const {
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
  auto canvas = composition::black_surface(static_cast<int>(output_width), static_cast<int>(output_height));

  // Same size: straight row copy. Different size: the media adapter's scaler (bicubic/lanczos
  // through libswscale) when provided, otherwise the basic nearest-neighbour fallback.
  const OfflineVideoPixelBuffer* placed = &source;
  if (scaled_width != source.width || scaled_height != source.height) {
    if (options_.scaler != nullptr) {
      if (!scaled_cache_ || scaled_cache_->source != source_frame.image || scaled_cache_->width != scaled_width ||
          scaled_cache_->height != scaled_height) {
        scaled_cache_ = ScaledCache{source_frame.image, scaled_width, scaled_height,
                                    options_.scaler->scale(source, static_cast<int>(scaled_width), static_cast<int>(scaled_height))};
      }
      placed = &scaled_cache_->scaled;
    }
  }
  if (placed != &source) {
    if (placed->width != scaled_width || placed->height != scaled_height) {
      throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::ConversionFailed, "offline frame scaler returned an unexpected picture"};
    }
  }
  try {
    composition::composite_opaque_layer(canvas, *placed, geometry);
  } catch (const std::invalid_argument& error) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::ConversionFailed, error.what()};
  }

  return OfflineVideoFrame{frame_index,
                           timeline_time,
                           static_cast<int>(output_width),
                           static_cast<int>(output_height),
                           canvas.stride_bytes,
                           OfflinePixelFormat::Rgba8888,
                           std::move(canvas.pixels)};
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
  const auto outcome = resolve_clip_at(plan_.snapshot(), plan_.snapshot().video_track_id,
                                       timeline_time);
  std::ostringstream diagnostic;
  diagnostic << "frame_index=" << frame_index.value()
             << " timeline_time=" << static_cast<double>(timeline_time);
  if (const auto* clip = std::get_if<ResolvedClip>(&outcome)) {
    const auto source_time = clip_source_position(*clip, timeline_time);
    diagnostic << " clip_id=" << clip->clip_id.value() << " media_id=" << clip->media_id.value()
               << " source_time=" << static_cast<double>(source_time);
  }

  try {
    OfflineVideoFrame output = std::visit(
        [this, frame_index, timeline_time](const auto& resolved) -> OfflineVideoFrame {
        using Result = std::decay_t<decltype(resolved)>;
        if constexpr (std::is_same_v<Result, TimelineGap>) {
          active_source_.reset();
          return black_frame(frame_index, timeline_time);
        } else if constexpr (std::is_same_v<Result, ResolvedClip>) {
          const auto source_time = clip_source_position(resolved, timeline_time);
          return compose_frame(frame_index, timeline_time, select_source_frame(resolved, source_time));
        } else if constexpr (std::is_same_v<Result, MissingClipMedia>) {
          throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::MissingMedia,
                                        "offline video clip references missing media"};
        } else {
          throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::InternalInvariant,
                                        "timeline ended before the export plan"};
        }
        },
        outcome);
    if (options_.titles != nullptr && !plan_.snapshot().titles.empty()) {
      // Titles ride on the finished output frame: the same compositor the preview uses.
      options_.titles->composite(composition::RgbaCanvas{output.width, output.height, output.stride_bytes, output.pixels.data()},
                                 plan_.snapshot().titles, timeline_time);
    }
    ++next_frame_index_;
    ++metrics_.output_frames;
    return output;
  } catch (OfflineVideoRenderError& error) {
    if (active_source_ != nullptr) {
      if (active_source_->candidate.has_value() &&
          active_source_->candidate->presentation_timestamp.has_value()) {
        diagnostic << " candidate_pts="
                   << static_cast<double>(*active_source_->candidate->presentation_timestamp);
      }
      if (active_source_->lookahead.has_value() &&
          active_source_->lookahead->presentation_timestamp.has_value()) {
        diagnostic << " lookahead_pts="
                   << static_cast<double>(*active_source_->lookahead->presentation_timestamp);
      }
    }
    error.set_diagnostic_context(diagnostic.str());
    throw;
  }
}

} // namespace pulsar::application::exporting
