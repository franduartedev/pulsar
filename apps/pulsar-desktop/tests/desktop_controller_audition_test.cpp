#include <QElapsedTimer>
#include <QCoreApplication>
// Library sound audition: the WAV reader/converter, every shipped sound is playable data, and the
// controller's ▶ semantics (replace / same-again-stops / never edits the project).
#include "../src/audition_audio.hpp"
#include "../src/desktop_controller.hpp"
#include "../src/preview_image_provider.hpp"

#include <QGuiApplication>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {
namespace fs = std::filesystem;
using pulsar::desktop::PcmWav;

std::vector<unsigned char> make_wav(int rate, int channels, int bits, const std::vector<double>& frames_per_channel_values, bool is_float = false,
                                    bool overstate_data = false) {
  std::vector<unsigned char> out;
  const auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(v >> (8 * i))); };
  const auto u16 = [&](std::uint16_t v) { out.push_back(static_cast<unsigned char>(v)); out.push_back(static_cast<unsigned char>(v >> 8)); };
  const std::uint32_t data_bytes = static_cast<std::uint32_t>(frames_per_channel_values.size() * static_cast<std::size_t>(bits / 8));
  out.insert(out.end(), {'R', 'I', 'F', 'F'}); u32(36 + data_bytes); out.insert(out.end(), {'W', 'A', 'V', 'E'});
  out.insert(out.end(), {'f', 'm', 't', ' '}); u32(16); u16(is_float ? 3 : 1); u16(static_cast<std::uint16_t>(channels));
  u32(static_cast<std::uint32_t>(rate)); u32(static_cast<std::uint32_t>(rate * channels * bits / 8)); u16(static_cast<std::uint16_t>(channels * bits / 8)); u16(static_cast<std::uint16_t>(bits));
  out.insert(out.end(), {'d', 'a', 't', 'a'}); u32(overstate_data ? data_bytes + 1000 : data_bytes);
  for (const double v : frames_per_channel_values) {
    if (is_float) { float f = static_cast<float>(v); std::uint32_t raw; std::memcpy(&raw, &f, 4); u32(raw); }
    else if (bits == 16) u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(v * 32767.0)));
    else if (bits == 8) out.push_back(static_cast<unsigned char>(128 + static_cast<int>(v * 127.0)));
  }
  return out;
}
std::vector<double> sine(int frames, int rate, double amplitude = 0.5) {
  std::vector<double> v(static_cast<std::size_t>(frames));
  for (int i = 0; i < frames; ++i) v[static_cast<std::size_t>(i)] = amplitude * std::sin(2.0 * M_PI * 440.0 * i / rate);
  return v;
}
float peak(const std::vector<float>& samples, std::size_t from = 0, std::size_t to = static_cast<std::size_t>(-1)) {
  float p = 0.0F;
  for (std::size_t i = from; i < std::min(to, samples.size()); ++i) p = std::max(p, std::fabs(samples[i]));
  return p;
}
} // namespace

#define REQUIRE(step, condition) do { if (!(condition)) { std::fprintf(stderr, "AUDITION step %d failed: %s\n", step, #condition); return step; } } while (false)

int main(int argc, char** argv) {
  using namespace pulsar::desktop;

  // ---- WAV reader ----
  auto wav = parse_pcm_wav(make_wav(48000, 1, 16, sine(4800, 48000)));
  REQUIRE(1, wav && wav->sample_rate == 48000 && wav->channels == 1 && wav->frames() == 4800 && peak(wav->samples) > 0.45F && peak(wav->samples) < 0.55F);
  auto wav_f = parse_pcm_wav(make_wav(44100, 2, 32, sine(2000, 44100, 0.25), true));   // 1000 stereo frames of float
  REQUIRE(2, wav_f && wav_f->channels == 2 && wav_f->sample_rate == 44100 && wav_f->frames() == 1000);
  auto wav_8 = parse_pcm_wav(make_wav(22050, 1, 8, sine(500, 22050)));
  REQUIRE(3, wav_8 && wav_8->frames() == 500);
  REQUIRE(4, parse_pcm_wav(make_wav(48000, 1, 16, sine(4800, 48000), false, true)).has_value());   // header overstates the data: tolerated
  REQUIRE(5, !parse_pcm_wav({}).has_value() && !parse_pcm_wav(std::vector<unsigned char>(100, 0)).has_value());
  auto truncated = make_wav(48000, 1, 16, sine(4800, 48000)); truncated.resize(30);
  REQUIRE(6, !parse_pcm_wav(truncated).has_value());
  auto compressed = make_wav(48000, 1, 16, sine(480, 48000)); compressed[20] = 2;       // ADPCM tag: not supported, refused
  REQUIRE(7, !parse_pcm_wav(compressed).has_value());

  // ---- conversion to the device: lead-in, tail, channels, rate ----
  const auto stereo = prepare_audition_samples(*wav, 48000, 2, 150, 120);
  REQUIRE(10, stereo.size() == (7200 + 4800 + 5760) * 2);
  REQUIRE(11, peak(stereo, 0, 7200 * 2) == 0.0F && peak(stereo, (7200 + 4800) * 2) == 0.0F);   // silence around the sound
  REQUIRE(12, std::fabs(stereo[(7200 + 100) * 2] - stereo[(7200 + 100) * 2 + 1]) < 1e-6F);    // mono spread to both channels
  REQUIRE(13, peak(stereo) <= 1.0F && peak(stereo) > 0.45F);
  const auto resampled = prepare_audition_samples(*wav_f, 48000, 2, 0, 0);
  REQUIRE(14, std::llabs(static_cast<long long>(resampled.size() / 2) - 1088) <= 1);           // 1000 frames at 44.1k -> ~1088 at 48k
  const auto to_mono = prepare_audition_samples(*wav_f, 44100, 1, 0, 0);
  REQUIRE(15, to_mono.size() == 1000);

  // ---- the shipped sound pack: every file reads as audible PCM (also proves the packaged path works) ----
  QGuiApplication application{argc, argv};
  PreviewImageProvider provider;
  DesktopController controller{provider, {}};
  REQUIRE(19, controller.soundPackAvailable());                            // the test environment points at the pack
  {
    const auto pack = fs::path{PULSAR_SOURCE_DIR} / "assets/sounds";
    int count = 0;
    for (const auto& entry : fs::directory_iterator{pack}) {
      if (entry.path().extension() != ".wav") continue;
      const auto sound = read_pcm_wav(entry.path());
      REQUIRE(18, sound && sound->frames() > 100 && peak(sound->samples) > 0.01F);   // every shipped sound is audible PCM the player can read
      ++count;
    }
    REQUIRE(17, count >= 37);
  }
  {
    const auto library = controller.soundLibrary();
    REQUIRE(20, library.size() >= 37);
    // Controller semantics on a machine with or without an audio device.
    const bool dirty_before = controller.projectDirty();
    const bool undo_before = controller.canUndo();
    const QString first = library.at(0).toMap().value("id").toString();
    const QString second = library.at(1).toMap().value("id").toString();
    controller.auditionSound(QStringLiteral("no-such-sound"));
    REQUIRE(21, controller.auditioningSound().isEmpty() && controller.errorMessage().isEmpty());   // unknown id: nothing happens
    controller.auditionSound(first);
    const bool has_output = controller.auditioningSound() == first;      // false when this machine has no audio output
    if (has_output) {
      controller.auditionSound(second);
      REQUIRE(22, controller.auditioningSound() == second);                // another sound replaces the running one
      controller.auditionSound(second);
      REQUIRE(23, controller.auditioningSound().isEmpty());                // the same sound again stops it
    } else {
      REQUIRE(24, !controller.errorMessage().isEmpty());                    // no device: a clear message, never silence-without-reason
    }
    controller.stopAudition();
    REQUIRE(25, controller.auditioningSound().isEmpty());
    REQUIRE(26, controller.projectDirty() == dirty_before && controller.canUndo() == undo_before && controller.timelineClips()->rowCount() == 0);
  }
  {
    // A sound that plays to its end must clear the active state by itself, promptly (not on a late safety net).
    const auto library = controller.soundLibrary();
    const QString id = library.at(0).toMap().value("id").toString();
    controller.auditionSound(id);
    if (controller.auditioningSound() == id) {
      QElapsedTimer waited; waited.start();
      while (!controller.auditioningSound().isEmpty() && waited.elapsed() < 6000) { QCoreApplication::processEvents(QEventLoop::AllEvents, 20); }
      std::fprintf(stderr, "AUDITION natural end after %lld ms\n", static_cast<long long>(waited.elapsed()));
      REQUIRE(27, controller.auditioningSound().isEmpty());
    }
  }
  std::fprintf(stderr, "AUDITION ok\n");
  return 0;
}
