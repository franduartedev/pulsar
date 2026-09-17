#include "editor_session.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace pulsar::desktop {
namespace {

constexpr std::int64_t display_quantum_denominator = 1'000'000;

std::string time_text(domain::MediaTime value) {
  const auto seconds = value.seconds();
  auto whole = seconds.numerator() / seconds.denominator();
  const auto remainder = seconds.numerator() % seconds.denominator();
  auto millis = static_cast<std::int64_t>(
      std::llround(static_cast<long double>(remainder) * 1000.0L /
                   static_cast<long double>(seconds.denominator())));
  if (millis >= 1000) {
    ++whole;
    millis = 0;
  }
  std::ostringstream text;
  text << std::setfill('0') << std::setw(2) << whole / 3600 << ':' << std::setw(2)
       << (whole / 60) % 60 << ':' << std::setw(2) << whole % 60 << '.' << std::setw(3)
       << millis;
  return text.str();
}

} // namespace

std::optional<domain::MediaTime> TimelineViewport::time_at(double pixel, double width) const {
  if (!std::isfinite(pixel) || !std::isfinite(width) || width <= 0.0) {
    return std::nullopt;
  }
  if (duration_ <= domain::MediaTime::zero()) return domain::MediaTime::zero();
  const double bounded = std::clamp(pixel, 0.0, width);
  const long double fraction = static_cast<long double>(bounded / width);
  const long double seconds = static_cast<long double>(duration_.value()) *
                              static_cast<long double>(duration_.time_base().numerator()) /
                              static_cast<long double>(duration_.time_base().denominator());
  const long double quantized = std::round(seconds * fraction * display_quantum_denominator);
  if (quantized <= 0.0L) return domain::MediaTime::zero();
  if (quantized >= seconds * display_quantum_denominator) return duration_;
  if (quantized > static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
    return duration_;
  }
  return domain::MediaTime{static_cast<std::int64_t>(quantized),
                           domain::Rational{1, display_quantum_denominator}};
}

double TimelineViewport::pixel_at(domain::MediaTime time, double width) const {
  if (!std::isfinite(width) || width <= 0.0 || duration_ <= domain::MediaTime::zero()) return 0.0;
  const long double numerator = static_cast<long double>(time.value()) *
                                static_cast<long double>(time.time_base().numerator()) /
                                static_cast<long double>(time.time_base().denominator());
  const long double denominator = static_cast<long double>(duration_.value()) *
                                  static_cast<long double>(duration_.time_base().numerator()) /
                                  static_cast<long double>(duration_.time_base().denominator());
  if (denominator <= 0.0L) return 0.0;
  return static_cast<double>(std::clamp(numerator / denominator, 0.0L, 1.0L) * width);
}

EditorSession::EditorSession()
    : EditorSession{[](const std::filesystem::path&, int) {
        return std::unique_ptr<application::PlaybackMediaWorker>{};
      }} {}

EditorSession::EditorSession(application::PlaybackWorkerFactory worker_factory)
    : processor_{document_}, coordinator_{clock_, std::move(worker_factory)} {
  synchronize_document_state();
}

domain::MediaTime EditorSession::timeline_duration() const noexcept {
  return document_.timeline().duration();
}

void EditorSession::synchronize_document_state() {
  if (synchronized_document_generation_.has_value() &&
      synchronized_document_generation_.value() == document_.document_generation()) {
    return;
  }
  coordinator_.set_snapshot(document_.snapshot());
  synchronized_document_generation_ = document_.document_generation();
  rebuild_read_model();
}

void EditorSession::rebuild_read_model() {
  views_.clear();
  const auto& timeline = document_.timeline();
  const auto* track = timeline.find_track(document_.video_track_id());
  if (track != nullptr) {
    for (const auto& clip : track->clips()) {
      const auto asset = document_.find_asset(clip.media_id());
      TimelineClipView view{clip.id(), std::to_string(clip.id().value()),
                            asset.has_value() ? asset->source_locator().filename().string() : "Offline",
                            time_text(clip.timeline_range().start()),
                            time_text(clip.timeline_range().end()), time_text(clip.duration()),
                            !asset.has_value() || asset->availability() == application::MediaAvailability::Unavailable,
                            selected_clip_.has_value() && selected_clip_.value() == clip.id()};
      views_.push_back(std::move(view));
    }
  }
  if (selected_clip_.has_value() &&
      std::none_of(views_.begin(), views_.end(), [this](const auto& view) {
        return view.id == selected_clip_.value();
      })) {
    selected_clip_.reset();
    for (auto& view : views_) view.selected = false;
  }
  ++read_model_generation_;
}

void EditorSession::seek(domain::MediaTime position) {
  coordinator_.seek(position);
}

domain::MediaTime EditorSession::playback_position() const {
  return coordinator_.timeline_position();
}

void EditorSession::select_clip(std::optional<domain::ClipId> clip) {
  if (selected_clip_ == clip) return;
  selected_clip_ = clip;
  rebuild_read_model();
}

std::optional<domain::MediaTime> EditorSession::time_at(double pixel, double width) const {
  return TimelineViewport{timeline_duration()}.time_at(pixel, width);
}

double EditorSession::pixel_at(domain::MediaTime time, double width) const {
  return TimelineViewport{timeline_duration()}.pixel_at(time, width);
}

} // namespace pulsar::desktop
