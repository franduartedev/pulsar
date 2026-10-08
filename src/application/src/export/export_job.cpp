#include <pulsar/application/export/export_job.hpp>

#include <algorithm>
#include <exception>
#include <utility>

namespace pulsar::application::exporting {
namespace {

[[noreturn]] void fail(ExportErrorCode code, std::string message) {
  throw ExportError{code, std::move(message)};
}

[[nodiscard]] ExportErrorCode map_plan_error(ExportPlanErrorCode code) noexcept {
  return code == ExportPlanErrorCode::MissingMedia ? ExportErrorCode::MissingMedia
                                                    : ExportErrorCode::InvalidProject;
}

[[nodiscard]] ExportErrorCode map_video_error(OfflineVideoRenderErrorCode code) noexcept {
  switch (code) {
  case OfflineVideoRenderErrorCode::Cancelled:
    return ExportErrorCode::Cancelled;
  case OfflineVideoRenderErrorCode::SourceOpenFailed:
    return ExportErrorCode::SourceOpenFailed;
  case OfflineVideoRenderErrorCode::DecodeFailed:
    return ExportErrorCode::DecodeFailed;
  case OfflineVideoRenderErrorCode::MissingMedia:
    return ExportErrorCode::MissingMedia;
  default:
    return ExportErrorCode::RenderFailed;
  }
}

[[nodiscard]] ExportErrorCode map_audio_error(OfflineAudioRenderErrorCode code) noexcept {
  switch (code) {
  case OfflineAudioRenderErrorCode::SourceOpenFailed:
    return ExportErrorCode::SourceOpenFailed;
  case OfflineAudioRenderErrorCode::DecodeFailed:
    return ExportErrorCode::DecodeFailed;
  case OfflineAudioRenderErrorCode::MissingMedia:
    return ExportErrorCode::MissingMedia;
  default:
    return ExportErrorCode::RenderFailed;
  }
}

[[nodiscard]] ExportErrorCode map_writer_error(ExportMediaWriterErrorCode code) noexcept {
  switch (code) {
  case ExportMediaWriterErrorCode::EncoderUnavailable:
    return ExportErrorCode::EncoderUnavailable;
  case ExportMediaWriterErrorCode::OutputOpenFailed:
    return ExportErrorCode::OutputOpenFailed;
  case ExportMediaWriterErrorCode::MuxWriteFailed:
    return ExportErrorCode::MuxFailed;
  case ExportMediaWriterErrorCode::FinalizeFailed:
    return ExportErrorCode::FinalizeFailed;
  default:
    return ExportErrorCode::InternalInvariant;
  }
}

void throw_cancelled() { fail(ExportErrorCode::Cancelled, "export cancelled"); }

void check_cancelled(const ExportJob::CancelPredicate& predicate) {
  if (predicate && predicate()) throw_cancelled();
}

void emit_progress(const ExportJob::ProgressSink& sink, ExportProgress progress,
                   bool completed = false) {
  if (!sink) return;
  // Reserve the final value for successful publication. This prevents a
  // failed/cancelled job from ever reporting 1.0.
  progress.fraction = completed ? 1.0 : std::min(0.99, std::max(0.0, progress.fraction));
  sink(progress);
}

} // namespace

ExportJob::ExportJob(OfflineVideoSourceFactory& video_sources,
                     OfflineAudioSourceFactory& audio_sources,
                     ExportMediaWriterFactory& writers,
                     ExportOutputPublisher& publisher)
    : video_sources_{video_sources}, audio_sources_{audio_sources}, writers_{writers},
      publisher_{publisher} {}

ExportResult ExportJob::run(const ExportRequest& request, CancelPredicate is_cancelled,
                            ProgressSink progress) {
  if (running_.test_and_set(std::memory_order_acquire)) {
    fail(ExportErrorCode::InternalInvariant, "ExportJob is already running");
  }
  struct RunGuard {
    std::atomic_flag& flag;
    ~RunGuard() { flag.clear(std::memory_order_release); }
  } guard{running_};

  check_cancelled(is_cancelled);

  std::optional<ExportPlan> plan;
  try {
    plan.emplace(ExportPlan::build(request.snapshot, request.profile));
  } catch (const ExportPlanError& error) {
    throw ExportError{map_plan_error(error.code()), error.what()};
  } catch (const std::exception& error) {
    throw ExportError{ExportErrorCode::InvalidProject, error.what()};
  }

  std::filesystem::path temporary_output;
  try {
    temporary_output = publisher_.create_temporary_sibling(request.destination);
  } catch (const std::exception& error) {
    fail(ExportErrorCode::PublishFailed, error.what());
  }
  std::unique_ptr<ExportMediaWriter> writer;
  bool published = false;
  try {
    const auto initial = ExportProgress{0.0, 0, plan->frame_count(), 0,
                                        plan->audio_sample_count()};
    emit_progress(progress, initial);

    try {
      writer = writers_.open(*plan, temporary_output);
    } catch (const ExportMediaWriterError& error) {
      fail(map_writer_error(error.code()), error.what());
    } catch (const std::exception& error) {
      fail(ExportErrorCode::OutputOpenFailed, error.what());
    }
    if (!writer) fail(ExportErrorCode::OutputOpenFailed, "writer factory returned no writer");

    OfflineVideoRenderer video_renderer{*plan, video_sources_, request.render_options, is_cancelled};
    OfflineAudioRenderer audio_renderer{*plan, audio_sources_};
    std::int64_t audio_done = 0;
    auto report = [&] {
      const auto video_done = video_renderer.metrics().output_frames;
      const auto audio_samples = audio_renderer.metrics().output_samples;
      const auto video_fraction = plan->frame_count() == 0
                                      ? 0.0
                                      : static_cast<double>(video_done) / plan->frame_count();
      const auto audio_fraction = plan->audio_sample_count() == 0
                                      ? 0.0
                                      : static_cast<double>(audio_samples) /
                                            plan->audio_sample_count();
      emit_progress(progress,
                    ExportProgress{0.5 * (video_fraction + audio_fraction),
                                   static_cast<std::int64_t>(video_done), plan->frame_count(),
                                   audio_done, plan->audio_sample_count()});
    };

    for (std::int64_t frame_index = 0; frame_index < plan->frame_count(); ++frame_index) {
      check_cancelled(is_cancelled);
      writer->write_video(video_renderer.render_frame(domain::FrameNumber{frame_index}));
      const auto horizon = std::min(
          plan->audio_sample_count(),
          audio_sample_index_at(output_frame_time(domain::FrameNumber{frame_index + 1},
                                                  plan->frame_rate()),
                                domain::RoundingMode::Ceil));
      while (audio_done < horizon) {
        check_cancelled(is_cancelled);
        auto block = audio_renderer.render_next_block();
        if (!block.has_value() || block->first_sample_index != audio_done) {
          fail(ExportErrorCode::InternalInvariant, "audio renderer stopped off the sample grid");
        }
        writer->write_audio(*block);
        audio_done += static_cast<std::int64_t>(block->frame_count);
        report();
      }
      report();
    }

    while (audio_done < plan->audio_sample_count()) {
      check_cancelled(is_cancelled);
      auto block = audio_renderer.render_next_block();
      if (!block.has_value() || block->first_sample_index != audio_done) {
        fail(ExportErrorCode::InternalInvariant, "audio renderer did not complete the sample grid");
      }
      writer->write_audio(*block);
      audio_done += static_cast<std::int64_t>(block->frame_count);
      report();
    }

    check_cancelled(is_cancelled);
    try {
      writer->finish();
    } catch (const ExportMediaWriterError& error) {
      fail(map_writer_error(error.code()), error.what());
    } catch (const std::exception& error) {
      fail(ExportErrorCode::FinalizeFailed, error.what());
    }
    writer.reset();

    check_cancelled(is_cancelled);
    if (!publisher_.temporary_exists(temporary_output)) {
      fail(ExportErrorCode::FinalizeFailed, "writer did not produce the temporary output");
    }
    try {
      publisher_.publish_replace_existing_safely(temporary_output, request.destination);
    } catch (const std::exception& error) {
      fail(ExportErrorCode::PublishFailed, error.what());
    }
    published = true;
    const ExportProgress final_progress{1.0, plan->frame_count(), plan->frame_count(),
                                        plan->audio_sample_count(), plan->audio_sample_count()};
    emit_progress(progress, final_progress, true);
    return ExportResult{request.destination, plan->document_generation(), final_progress};
  } catch (const ExportError&) {
    if (writer) writer->abort();
    if (!published) publisher_.cleanup_temporary(temporary_output);
    throw;
  } catch (const OfflineVideoRenderError& error) {
    if (writer) writer->abort();
    if (!published) publisher_.cleanup_temporary(temporary_output);
    std::string message = error.what();
    if (!error.diagnostic_context().empty()) {
      message += " [" + error.diagnostic_context() + "]";
    }
    throw ExportError{map_video_error(error.code()), std::move(message)};
  } catch (const OfflineAudioRenderError& error) {
    if (writer) writer->abort();
    if (!published) publisher_.cleanup_temporary(temporary_output);
    throw ExportError{map_audio_error(error.code()), error.what()};
  } catch (const ExportMediaWriterError& error) {
    if (writer) writer->abort();
    if (!published) publisher_.cleanup_temporary(temporary_output);
    throw ExportError{map_writer_error(error.code()), error.what()};
  } catch (const std::exception& error) {
    if (writer) writer->abort();
    if (!published) publisher_.cleanup_temporary(temporary_output);
    throw ExportError{ExportErrorCode::InternalInvariant, error.what()};
  }
}

} // namespace pulsar::application::exporting
