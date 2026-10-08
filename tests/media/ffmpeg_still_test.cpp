// Stills through the real FFmpeg adapter: probe, one held frame at every instant, flattened alpha,
// EXIF orientation and generated colour cards.
#include <pulsar/application/media/still_media.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_media_probe.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_preview_converter.hpp>
#include <pulsar/media/ffmpeg/ffmpeg_video_decode_session.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {
namespace fs = std::filesystem;
using pulsar::domain::MediaTime;
using pulsar::domain::Rational;
using pulsar::media::ffmpeg::FFmpegMediaProbe;
using pulsar::media::ffmpeg::FFmpegPreviewConverter;
using pulsar::media::ffmpeg::FFmpegVideoDecodeSession;

int fail(std::string_view message) {
  std::cerr << "ffmpeg still test failed: " << message << '\n';
  return 1;
}

MediaTime secs(std::int64_t v) { return MediaTime{v, Rational{1, 1}}; }

struct Rgba { int r, g, b, a; };
Rgba pixel(const pulsar::media::ffmpeg::FFmpegPreviewFrame& f, int x, int y) {
  const auto* p = f.pixels.data() + static_cast<std::size_t>(y) * f.stride_bytes + static_cast<std::size_t>(x) * 4;
  return {p[0], p[1], p[2], p[3]};
}
bool near(const Rgba& c, int r, int g, int b) {
  const auto d = [](int a, int b2) { return a > b2 ? a - b2 : b2 - a; };
  return d(c.r, r) <= 6 && d(c.g, g) <= 6 && d(c.b, b) <= 6 && c.a == 255;
}

std::optional<pulsar::media::ffmpeg::FFmpegPreviewFrame> frame_at(const fs::path& path, MediaTime t) {
  FFmpegVideoDecodeSession session{path, 0};
  auto seek = session.seek_to_presentation_timestamp_visual(t);
  if (!seek.frame) return std::nullopt;
  if (session.decode_next_visual().has_value()) return std::nullopt;   // nothing after the held frame
  return FFmpegPreviewConverter{}.convert(*seek.frame);
}

} // namespace

int main() {
  const fs::path fixtures = fs::path{PULSAR_SOURCE_DIR} / "tests/fixtures";
  FFmpegMediaProbe probe;

  // ---- Probe: stills get a generous playable span, a frame rate, no audio ----------------------
  struct Case { const char* file; const char* container; int w; int h; };
  for (const Case c : {Case{"still_wide.png", "png_pipe", 640, 360}, Case{"still_portrait.jpg", "image2", 180, 320},
                       Case{"still_bars.webp", "webp_pipe", 256, 144}}) {
    const auto result = probe.probe(fixtures / c.file);
    if (!result.container_name || *result.container_name != c.container) return fail(std::string{"container of "} + c.file);
    if (result.video_streams.size() != 1 || !result.audio_streams.empty()) return fail("streams");
    const auto& v = result.video_streams.front();
    if (v.width != c.w || v.height != c.h) return fail("dimensions");
    if (!v.duration || *v.duration != pulsar::application::still_playable_duration()) return fail("playable span");
    if (!v.nominal_frame_rate || v.nominal_frame_rate->rational().numerator() != 30) return fail("frame rate");
  }

  // ---- Decode: the same picture at any instant; one frame only ---------------------------------
  for (const char* file : {"still_wide.png", "still_portrait.jpg", "still_bars.webp"}) {
    for (const std::int64_t t : {0, 1, 4, 3599}) {
      const auto frame = frame_at(fixtures / file, secs(t));
      if (!frame) return fail(std::string{"held frame "} + file);
      if (frame->width <= 0 || frame->pixels.empty()) return fail("empty frame");
    }
    FFmpegVideoDecodeSession sequential{fixtures / file, 0};   // sequential read: once, then end
    if (!sequential.decode_next_visual() || sequential.decode_next_visual()) return fail("sequential read");
  }
  {
    const auto a = frame_at(fixtures / "still_wide.png", secs(0));
    const auto b = frame_at(fixtures / "still_wide.png", secs(100));
    if (!a || !b || a->pixels != b->pixels) return fail("identical pixels at any time");
  }

  // ---- Alpha is flattened over black ------------------------------------------------------------
  {
    const auto frame = frame_at(fixtures / "still_alpha.png", secs(2));
    if (!frame || frame->width != 64 || frame->height != 32) return fail("alpha size");
    if (!near(pixel(*frame, 5, 5), 255, 0, 0)) return fail("opaque pixel kept");
    if (!near(pixel(*frame, 50, 5), 0, 0, 0)) return fail("transparent pixel flattened to black");
  }

  // ---- EXIF orientation is applied (stored 60x30, rotated to 30x60) -----------------------------
  {
    const auto frame = frame_at(fixtures / "still_exif_rotated.jpg", secs(1));
    if (!frame) return fail("exif frame");
    if (frame->width != 30 || frame->height != 60) return fail("orientation applied to dimensions");
    if (!near(pixel(*frame, 25, 4), 255, 0, 0)) return fail("orientation applied to pixels");
    if (!near(pixel(*frame, 4, 4), 0, 0, 255)) return fail("orientation: other corner");
  }

  // ---- Generated colour card: no file at all ---------------------------------------------------
  {
    const auto frame = frame_at("pulsar-color:ff8800", secs(3));
    if (!frame || frame->width != 1920 || frame->height != 1080) return fail("colour card size");
    if (!near(pixel(*frame, 10, 10), 255, 136, 0) || !near(pixel(*frame, 1900, 1070), 255, 136, 0)) return fail("colour card colour");
    bool rejected = false;
    try { FFmpegVideoDecodeSession bad{"pulsar-color:xyz", 0}; } catch (const std::exception&) { rejected = true; }
    if (!rejected) return fail("malformed colour card rejected");
  }
  return 0;
}
