#include <pulsar/application/export/offline_video_renderer.hpp>

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>

namespace pulsar::application::exporting {
namespace {

#if defined(__SIZEOF_INT128__)
using WideInt = __int128_t;
#else
#error "PULSAR requires signed __int128 support for exact video geometry arithmetic."
#endif

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

[[nodiscard]] std::int64_t scaled_dimension(std::int64_t numerator, std::int64_t denominator) {
  if (numerator <= 0 || denominator <= 0) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::UnsupportedFrame,
                                  "source aspect ratio is invalid"};
  }
  const WideInt value = static_cast<WideInt>(numerator) / static_cast<WideInt>(denominator);
  if (value <= 0 || value > std::numeric_limits<std::int64_t>::max()) {
    throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::ConversionFailed,
                                  "scaled video dimension is invalid"};
  }
  return static_cast<std::int64_t>(value);
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
                                           OfflineVideoSourceFactory& source_factory)
    : plan_{plan}, source_factory_{source_factory} {}

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
      throw OfflineVideoRenderError{OfflineVideoRenderErrorCode::SourceEndedEarly,
                                    "offline video source ended before the requested interval"};
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
  const bool width_limited = static_cast<WideInt>(output_width) * source.height <=
                             static_cast<WideInt>(output_height) * source.width;
  const auto scaled_width = width_limited
                                ? output_width
                                : scaled_dimension(output_height * source.width, source.height);
  const auto scaled_height = width_limited
                                 ? scaled_dimension(output_width * source.height, source.width)
                                 : output_height;
  const auto offset_x = (output_width - scaled_width) / 2;
  const auto offset_y = (output_height - scaled_height) / 2;
  const int output_stride = checked_rgba_stride(output_width);
  const auto output_size = checked_buffer_size(static_cast<int>(output_width),
                                               static_cast<int>(output_height), output_stride);
  std::vector<std::uint8_t> pixels(output_size, 0);
  for (std::size_t offset = 3; offset < pixels.size(); offset += 4) pixels[offset] = 255;

  for (std::int64_t y = 0; y < scaled_height; ++y) {
    const auto source_y = static_cast<std::int64_t>(
        (static_cast<WideInt>(y) * source.height) / scaled_height);
    for (std::int64_t x = 0; x < scaled_width; ++x) {
      const auto source_x = static_cast<std::int64_t>(
          (static_cast<WideInt>(x) * source.width) / scaled_width);
      const auto source_offset = static_cast<std::size_t>(source_y) * source.stride_bytes +
                                 static_cast<std::size_t>(source_x) * 4;
      const auto destination_offset = static_cast<std::size_t>(offset_y + y) * output_stride +
                                      static_cast<std::size_t>(offset_x + x) * 4;
      std::copy_n(source.pixels.data() + source_offset, 4, pixels.data() + destination_offset);
    }
  }

  return OfflineVideoFrame{frame_index,
                           timeline_time,
                           static_cast<int>(output_width),
                           static_cast<int>(output_height),
                           output_stride,
                           OfflinePixelFormat::Rgba8888,
                           std::move(pixels)};
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
  ++next_frame_index_;
  ++metrics_.output_frames;
  return output;
}

} // namespace pulsar::application::exporting
