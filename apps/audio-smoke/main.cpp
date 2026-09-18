// Manual A2 smoke: fixture -> FFmpegAudioDecodeSession -> QtAudioOutput ->
// default device. Not part of CI (no add_test): it needs a real output
// device and a human ear. Reports negotiation, runs to Completed (or a
// deterministic error), prints metrics, exits.
#include <pulsar/application/audio/audio_output.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_audio_decode_session.hpp>
#include <pulsar/media/qt_audio/qt_audio_output.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace {

using pulsar::application::AudioBlock;
using pulsar::application::AudioEnqueueResult;
using pulsar::application::AudioOutputState;
using pulsar::media::ffmpeg::FFmpegAudioDecodeSession;
using pulsar::media::qt_audio::QtAudioOutput;

int fail(const std::string& message) {
  std::fprintf(stderr, "audio-smoke: %s\n", message.c_str());
  return 1;
}

bool wait_for_state(QtAudioOutput& output, AudioOutputState wanted, int timeout_ms) {
  QElapsedTimer timer;
  timer.start();
  while (output.state() != wanted) {
    if (output.state() == AudioOutputState::Error) {
      return false;
    }
    if (timer.elapsed() > timeout_ms) {
      return false;
    }
    QCoreApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app{argc, argv};
  if (argc != 2) {
    return fail("usage: pulsar-audio-smoke <media-file>");
  }
  const std::filesystem::path media{argv[1]};

  FFmpegAudioDecodeSession session{media, 2};
  std::fprintf(stderr, "source: stream=%d rate=%d channels=%d codec=%s\n", session.stream_index(),
               session.sample_rate(), session.channel_count(), session.codec_name().c_str());

  QtAudioOutput output{QtAudioOutput::Options{{48000 * 2}, {}}};
  if (!output.open({session.sample_rate(), session.channel_count()})) {
    const auto error = output.error();
    return fail("open failed: " + (error.has_value() ? error->message : std::string{"unknown"}));
  }
  const auto device = output.device_format();
  std::fprintf(stderr, "device: rate=%d channels=%d sample=%d\n",
               device->sample_rate, device->channel_count,
               static_cast<int>(device->sample_format));

  bool started = false;
  while (auto block = session.decode_next()) {
    while (output.enqueue(*block) == AudioEnqueueResult::Backpressure) {
      if (!started) {
        if (!output.start()) {
          return fail("start failed");
        }
        started = true;
      }
      QCoreApplication::processEvents();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!started) {
      if (!output.start()) {
        return fail("start failed");
      }
      started = true;
    }
    QCoreApplication::processEvents();
  }
  if (!output.request_drain()) {
    return fail("drain rejected");
  }
  if (!wait_for_state(output, AudioOutputState::Completed, 15000)) {
    const auto error = output.error();
    return fail("completion failed: " + (error.has_value() ? error->message : std::string{"timeout"}));
  }
  const auto metrics = output.metrics();
  std::fprintf(stderr,
               "completed: accepted=%llu submitted=%llu processed=%llu buffered=%llu underruns=%llu\n",
               (unsigned long long)metrics.source_frames_accepted,
               (unsigned long long)metrics.device_frames_submitted,
               (unsigned long long)metrics.device_frames_processed,
               (unsigned long long)metrics.buffered_device_frames,
               (unsigned long long)metrics.underrun_count);
  output.close();
  return 0;
}
