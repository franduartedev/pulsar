#pragma once

#include <pulsar/application/export/offline_audio_renderer.hpp>
#include <pulsar/application/export/offline_video_renderer.hpp>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace pulsar::application::exporting {

enum class ExportMediaWriterErrorCode {
  EncoderUnavailable,
  MuxerUnavailable,
  OutputOpenFailed,
  InvalidInputFrame,
  VideoConversionFailed,
  AudioConversionFailed,
  VideoEncodeFailed,
  AudioEncodeFailed,
  MuxWriteFailed,
  FinalizeFailed,
  TimestampInvariant,
  InternalInvariant,
};

class ExportMediaWriterError final : public std::runtime_error {
public:
  ExportMediaWriterError(ExportMediaWriterErrorCode code, std::string message)
      : std::runtime_error{std::move(message)}, code_{code} {}

  [[nodiscard]] ExportMediaWriterErrorCode code() const noexcept { return code_; }

private:
  ExportMediaWriterErrorCode code_;
};

struct ExportMediaWriterMetrics {
  std::uint64_t video_frames_submitted{0};
  std::uint64_t video_packets_written{0};
  std::uint64_t audio_samples_submitted{0};
  std::uint64_t audio_encoder_frames{0};
  std::uint64_t audio_packets_written{0};
  std::uint64_t peak_audio_fifo_frames{0};
  std::uint64_t total_mux_packets{0};
};

class ExportMediaWriter {
public:
  virtual ~ExportMediaWriter() = default;

  ExportMediaWriter(const ExportMediaWriter&) = delete;
  ExportMediaWriter& operator=(const ExportMediaWriter&) = delete;

  virtual void write_video(const OfflineVideoFrame& frame) = 0;
  virtual void write_audio(const OfflineAudioBlock& block) = 0;
  virtual void finish() = 0;
  virtual void abort() noexcept = 0;
  [[nodiscard]] virtual const ExportMediaWriterMetrics& metrics() const noexcept = 0;

protected:
  ExportMediaWriter() = default;
};

} // namespace pulsar::application::exporting
