#include "sound_auditioner.hpp"

#include "audition_audio.hpp"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QByteArray>
#include <QLoggingCategory>
#include <QMediaDevices>
#include <QTimer>

#include <algorithm>
#include <cstring>

namespace pulsar::desktop {
namespace {
Q_LOGGING_CATEGORY(auditionLog, "pulsar.audition")
constexpr int lead_ms = 150;   // a device waking from idle clips the first moments
constexpr int tail_ms = 120;
}

SoundAuditioner::SoundAuditioner(QObject* parent) : QObject{parent} {}
SoundAuditioner::~SoundAuditioner() { teardown(); }

void SoundAuditioner::teardown() {
  if (guard_) guard_->stop();
  guard_.reset();
  if (sink_) {
    sink_->disconnect(this);
    sink_->stop();
  }
  sink_.reset();
  buffer_.reset();
}

void SoundAuditioner::stop() {
  const bool was_playing = sink_ != nullptr;
  teardown();
  if (was_playing) emit finished();
}

QString SoundAuditioner::play(const std::filesystem::path& wav_path) {
  teardown();
  const auto wav = read_pcm_wav(wav_path);
  if (!wav) {
    qCWarning(auditionLog) << "cannot read" << QString::fromStdString(wav_path.string());
    return QStringLiteral("No se pudo leer el sonido.");
  }
  const QAudioDevice device = QMediaDevices::defaultAudioOutput();
  if (device.isNull()) return QStringLiteral("No hay una salida de audio disponible.");

  // Same negotiation as the timeline engine: float at the source rate when the device takes it,
  // otherwise whatever the device prefers.
  QAudioFormat format;
  format.setSampleRate(48000);
  format.setChannelCount(2);
  format.setSampleFormat(QAudioFormat::Float);
  if (!device.isFormatSupported(format)) format = device.preferredFormat();
  const int rate = format.sampleRate() > 0 ? format.sampleRate() : 48000;
  const int channels = std::clamp(format.channelCount(), 1, 8);
  const auto samples = prepare_audition_samples(*wav, rate, channels, lead_ms, tail_ms);

  QByteArray pcm;
  switch (format.sampleFormat()) {
    case QAudioFormat::Float:
      pcm = QByteArray{reinterpret_cast<const char*>(samples.data()), static_cast<qsizetype>(samples.size() * sizeof(float))};
      break;
    case QAudioFormat::Int16: {
      pcm.resize(static_cast<qsizetype>(samples.size() * sizeof(qint16)));
      auto* out = reinterpret_cast<qint16*>(pcm.data());
      for (std::size_t i = 0; i < samples.size(); ++i) out[i] = static_cast<qint16>(std::clamp(samples[i], -1.0F, 1.0F) * 32767.0F);
      break;
    }
    case QAudioFormat::Int32: {
      pcm.resize(static_cast<qsizetype>(samples.size() * sizeof(qint32)));
      auto* out = reinterpret_cast<qint32*>(pcm.data());
      for (std::size_t i = 0; i < samples.size(); ++i) out[i] = static_cast<qint32>(static_cast<double>(std::clamp(samples[i], -1.0F, 1.0F)) * 2147483647.0);
      break;
    }
    default:
      return QStringLiteral("La salida de audio no acepta el formato del sonido.");
  }
  format.setSampleRate(rate);
  format.setChannelCount(channels);

  buffer_ = std::make_unique<QBuffer>();
  buffer_->setData(pcm);
  buffer_->open(QIODevice::ReadOnly);
  sink_ = std::make_unique<QAudioSink>(device, format);
  sink_->setVolume(0.9F);
  connect(sink_.get(), &QAudioSink::stateChanged, this, [this](QAudio::State state) {
    // The buffer ran dry: the sound is over. Tear down after the backend callback has returned
    // instead of destroying the sink from inside its own signal.
    if (state == QAudio::IdleState) QTimer::singleShot(0, this, [this] { stop(); });
    else if (state == QAudio::StoppedState && sink_ && sink_->error() != QAudio::NoError) {
      qCWarning(auditionLog) << "audio sink failed, error" << static_cast<int>(sink_->error());
      stop();
    }
  });
  sink_->start(buffer_.get());
  if (sink_->state() == QAudio::StoppedState && sink_->error() != QAudio::NoError) {
    qCWarning(auditionLog) << "audio sink did not start, error" << static_cast<int>(sink_->error());
    teardown();
    return QStringLiteral("La salida de audio no pudo reproducir el sonido.");
  }
  // Safety net for backends that never report Idle: the sound (lead and tail included) is over
  // at `duration_ms`, so clear shortly after that rather than leave a stale active state.
  guard_ = std::make_unique<QTimer>();
  guard_->setSingleShot(true);
  connect(guard_.get(), &QTimer::timeout, this, [this] { stop(); });
  const qint64 duration_ms = static_cast<qint64>(samples.size() / static_cast<std::size_t>(channels)) * 1000 / rate;
  guard_->start(static_cast<int>(duration_ms + 150));
  return {};
}

} // namespace pulsar::desktop
