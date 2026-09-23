#pragma once

#include <pulsar/application/export/export_media_writer.hpp>
#include <pulsar/application/export/export_plan.hpp>

#include <filesystem>
#include <memory>

namespace pulsar::media::ffmpeg {

class FFmpegExportMediaWriter final : public application::exporting::ExportMediaWriter {
public:
  FFmpegExportMediaWriter(const application::exporting::ExportPlan& plan,
                          const std::filesystem::path& output_path);
  ~FFmpegExportMediaWriter() override;

  void write_video(const application::exporting::OfflineVideoFrame& frame) override;
  void write_audio(const application::exporting::OfflineAudioBlock& block) override;
  void finish() override;
  void abort() noexcept override;
  [[nodiscard]] const application::exporting::ExportMediaWriterMetrics& metrics() const noexcept
      override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace pulsar::media::ffmpeg
