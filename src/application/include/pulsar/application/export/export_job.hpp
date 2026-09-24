#pragma once

#include <pulsar/application/export/export_media_writer.hpp>
#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/application/export/offline_audio_renderer.hpp>
#include <pulsar/application/export/offline_video_renderer.hpp>
#include <pulsar/application/project/project_snapshot.hpp>

#include <cstdint>
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace pulsar::application::exporting {

struct ExportRequest {
  ProjectSnapshot snapshot;
  std::filesystem::path destination;
  ExportProfile profile{ExportProfile::beta_v01()};
};

struct ExportProgress {
  double fraction{0.0};
  std::int64_t video_frames_done{0};
  std::int64_t video_frames_total{0};
  std::int64_t audio_samples_done{0};
  std::int64_t audio_samples_total{0};
};

struct ExportResult {
  std::filesystem::path destination;
  std::uint64_t document_generation{0};
  ExportProgress progress;
};

enum class ExportErrorCode {
  InvalidProject,
  MissingMedia,
  SourceOpenFailed,
  DecodeFailed,
  RenderFailed,
  EncoderUnavailable,
  OutputOpenFailed,
  MuxFailed,
  FinalizeFailed,
  PublishFailed,
  Cancelled,
  InternalInvariant,
};

class ExportError final : public std::runtime_error {
public:
  ExportError(ExportErrorCode code, std::string message)
      : std::runtime_error{std::move(message)}, code_{code} {}

  [[nodiscard]] ExportErrorCode code() const noexcept { return code_; }

private:
  ExportErrorCode code_;
};

class ExportMediaWriterFactory {
public:
  virtual ~ExportMediaWriterFactory() = default;
  [[nodiscard]] virtual std::unique_ptr<ExportMediaWriter> open(
      const ExportPlan& plan, const std::filesystem::path& temporary_output) = 0;
};

// This is the publication boundary. The implementation owns platform
// semantics; application never performs rename/remove or assumes replacement
// behavior from std::filesystem directly.
class ExportOutputPublisher {
public:
  virtual ~ExportOutputPublisher() = default;
  [[nodiscard]] virtual std::filesystem::path create_temporary_sibling(
      const std::filesystem::path& destination) = 0;
  [[nodiscard]] virtual bool temporary_exists(
      const std::filesystem::path& temporary_output) const = 0;
  virtual void publish_replace_existing_safely(const std::filesystem::path& temporary_output,
                                               const std::filesystem::path& destination) = 0;
  virtual void cleanup_temporary(const std::filesystem::path& temporary_output) noexcept = 0;
};

class ExportJob final {
public:
  using CancelPredicate = std::function<bool()>;
  using ProgressSink = std::function<void(const ExportProgress&)>;

  ExportJob(OfflineVideoSourceFactory& video_sources,
            OfflineAudioSourceFactory& audio_sources,
            ExportMediaWriterFactory& writers,
            ExportOutputPublisher& publisher);

  ExportJob(const ExportJob&) = delete;
  ExportJob& operator=(const ExportJob&) = delete;

  [[nodiscard]] ExportResult run(const ExportRequest& request,
                                 CancelPredicate is_cancelled = {},
                                 ProgressSink progress = {});

private:
  OfflineVideoSourceFactory& video_sources_;
  OfflineAudioSourceFactory& audio_sources_;
  ExportMediaWriterFactory& writers_;
  ExportOutputPublisher& publisher_;
  std::atomic_flag running_ = ATOMIC_FLAG_INIT;
};

} // namespace pulsar::application::exporting
