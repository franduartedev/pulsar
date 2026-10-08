#pragma once

#include <QObject>
#include <QString>

#include <filesystem>
#include <memory>

class QAudioSink;
class QBuffer;
class QTimer;

namespace pulsar::desktop {

// Plays one library sound at a time on its OWN audio stream (a dedicated QAudioSink, negotiated the
// same way as the project's audio engine: float at the device rate, else the device's preferred
// format). It shares nothing with the timeline audio engine and never touches the project, so
// auditioning cannot disturb playback or edit anything. A new request replaces the running one.
class SoundAuditioner final : public QObject {
  Q_OBJECT
public:
  explicit SoundAuditioner(QObject* parent = nullptr);
  ~SoundAuditioner() override;

  // Starts `wav` (stopping whatever played before). Returns an empty string on success, otherwise a
  // short reason in Spanish (no output device, unreadable file, device refused the format).
  [[nodiscard]] QString play(const std::filesystem::path& wav);
  void stop();
  [[nodiscard]] bool playing() const noexcept { return sink_ != nullptr; }

signals:
  void finished();   // the sound ended or was stopped

private:
  void teardown();
  std::unique_ptr<QAudioSink> sink_;
  std::unique_ptr<QBuffer> buffer_;
  std::unique_ptr<QTimer> guard_;
};

} // namespace pulsar::desktop
