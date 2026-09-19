#include <pulsar/application/playback/audio_playback_session.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace pulsar;
using application::AudioBlock;
using application::AudioDeviceFormat;
using application::AudioEnqueueResult;
using application::AudioOutput;
using application::AudioOutputError;
using application::AudioOutputMetrics;
using application::AudioOutputState;
using application::AudioPlaybackSession;
using application::AudioPlaybackSource;
using application::AudioPlaybackState;
using application::AudioSourceFormat;
using domain::ClipId;
using domain::MediaId;
using domain::MediaTime;
using domain::Rational;
using domain::TimeRange;

MediaTime time(std::int64_t value, Rational base = Rational{1, 1}) {
  return MediaTime{value, base};
}

class FakeOutput final : public AudioOutput {
public:
  bool open(const AudioSourceFormat& source) override {
    format_ = AudioDeviceFormat{source.sample_rate, source.channel_count,
                                application::AudioSampleFormat::Float32};
    state_ = AudioOutputState::Opened;
    metrics_ = {};
    pending_ = 0;
    return open_ok;
  }
  bool close() override {
    state_ = AudioOutputState::Closed;
    pending_ = 0;
    return true;
  }
  AudioEnqueueResult enqueue(const AudioBlock& block) override {
    if (state_ != AudioOutputState::Opened && state_ != AudioOutputState::Running) {
      return AudioEnqueueResult::InvalidState;
    }
    if (reject_enqueue) return AudioEnqueueResult::Backpressure;
    accepted_blocks.push_back(block);
    ++metrics_.source_frames_accepted;
    ++metrics_.device_frames_submitted;
    pending_ += block.frame_count();
    metrics_.buffered_device_frames = pending_;
    return AudioEnqueueResult::Accepted;
  }
  bool start() override {
    if (state_ != AudioOutputState::Opened || pending_ == 0) return false;
    if (!start_ok) return false;
    state_ = AudioOutputState::Running;
    return true;
  }
  bool request_drain() override {
    if (state_ != AudioOutputState::Running) return false;
    state_ = AudioOutputState::Draining;
    if (pending_ == 0) state_ = AudioOutputState::Completed;
    return true;
  }
  bool stop() override {
    if (state_ == AudioOutputState::Closed) return false;
    state_ = AudioOutputState::Opened;
    pending_ = 0;
    metrics_.buffered_device_frames = 0;
    return true;
  }
  bool flush() override { return stop(); }
  AudioOutputState state() const override { return state_; }
  std::optional<AudioDeviceFormat> device_format() const override { return format_; }
  AudioOutputMetrics metrics() const override { return metrics_; }
  std::optional<AudioOutputError> error() const override { return std::nullopt; }

  void advance(std::uint64_t frames) {
    const auto consumed = std::min(frames, pending_);
    pending_ -= consumed;
    metrics_.device_frames_processed += consumed;
    metrics_.buffered_device_frames = pending_;
    if (state_ == AudioOutputState::Draining && pending_ == 0) {
      state_ = AudioOutputState::Completed;
    }
  }

  void report_underrun() { ++metrics_.underrun_count; }

  bool open_ok{true};
  bool start_ok{true};
  bool reject_enqueue{false};
  std::vector<AudioBlock> accepted_blocks;

private:
  AudioOutputState state_{AudioOutputState::Closed};
  AudioDeviceFormat format_{0, 0, application::AudioSampleFormat::Float32};
  AudioOutputMetrics metrics_{};
  std::uint64_t pending_{0};
};

class FakeSource final : public AudioPlaybackSource {
public:
  explicit FakeSource(std::optional<MediaTime> first_pts, std::size_t block_count = 1,
                      std::optional<MediaTime> stream_origin = std::nullopt)
      : first_pts_{first_pts}, block_count_{block_count}, stream_origin_{stream_origin} {}
  int sample_rate() const noexcept override { return 1000; }
  int channel_count() const noexcept override { return 1; }
  bool seek_to(domain::MediaTime) override { return true; }
  std::optional<MediaTime> stream_origin() const override { return stream_origin_; }
  std::optional<AudioBlock> decode_next() override {
    if (emitted_ >= block_count_) return std::nullopt;
    const auto pts = emitted_ == 0 ? first_pts_ : std::nullopt;
    ++emitted_;
    return AudioBlock{1000, 1, 1000, std::vector<float>(1000, 0.5F), Rational{1, 1000},
                      pts};
  }

private:
  std::optional<MediaTime> first_pts_;
  std::size_t emitted_{0};
  std::size_t block_count_{1};
  std::optional<MediaTime> stream_origin_;
};

class FailingSource final : public AudioPlaybackSource {
public:
  int sample_rate() const noexcept override { return 1000; }
  int channel_count() const noexcept override { return 1; }
  bool seek_to(domain::MediaTime) override { return true; }
  std::optional<AudioBlock> decode_next() override {
    throw std::runtime_error{"synthetic decode failure"};
  }
};

class TemporarilyStarvedSource final : public AudioPlaybackSource {
public:
  int sample_rate() const noexcept override { return 1000; }
  int channel_count() const noexcept override { return 1; }
  bool seek_to(domain::MediaTime) override { return true; }
  std::optional<AudioBlock> decode_next() override { return std::nullopt; }
  ReadResult read_next() override {
    if (first_read_) {
      first_read_ = false;
      return {ReadState::Starved, std::nullopt};
    }
    if (sent_) return {ReadState::Ended, std::nullopt};
    sent_ = true;
    return {ReadState::Block,
            AudioBlock{1000, 1, 1000, std::vector<float>(1000, 0.5F), Rational{1, 1000},
                       std::nullopt}};
  }

private:
  bool first_read_{true};
  bool sent_{false};
};

application::ResolvedClip clip(bool with_audio = true, double gain_db = 0.0, bool muted = false) {
  std::optional<application::AudioSourceBinding> audio;
  if (with_audio) {
    audio = application::AudioSourceBinding{
        "fixture", application::AudioAssetMetadata{1, 1000, 1, Rational{1, 1000},
                                                    time(1000, Rational{1, 1000})}};
  }
  return application::ResolvedClip{ClipId{1}, MediaId{1}, time(0),
                                   TimeRange{time(0), time(2)}, time(0),
                                   TimeRange{time(0), time(2)}, time(0), gain_db, muted, 0, "fixture", audio};
}

void video_only_is_intentional_silence() {
  AudioPlaybackSession session{nullptr, {}};
  session.configure(clip(false), time(0));
  assert(session.state() == AudioPlaybackState::IntentionalSilence);
  assert(session.condition() == application::AudioPlaybackCondition::IntentionalSilence);
  session.play();
  assert(!session.has_device_clock());
}

void source_origin_creates_silence_without_clock_jump() {
  FakeOutput output;
  int created = 0;
  AudioPlaybackSession session{
      &output, [&](const auto&, int) {
        ++created;
        return std::make_unique<FakeSource>(time(100, Rational{1, 1000}));
      }};
  session.configure(clip(), time(0));
  assert(session.state() == AudioPlaybackState::Ready);
  assert(session.condition() == application::AudioPlaybackCondition::Present);
  assert(!output.accepted_blocks.empty());
  assert(output.accepted_blocks.front().samples()[0] == 0.0F);
  session.play();
  assert(session.state() == AudioPlaybackState::Running);
  output.advance(100);
  session.update(time(0));
  assert(session.clock_position().has_value());
  assert(*session.clock_position() == time(100, Rational{1, 1000}));
  const auto generation = session.generation();
  session.seek(time(1));
  assert(session.generation() > generation);
  assert(created == 2);
}

void stream_origin_maps_presentation_relative_source() {
  FakeOutput output;
  AudioPlaybackSession session{
      &output, [](const auto&, int) {
        return std::make_unique<FakeSource>(time(5000, Rational{1, 1000}), 1,
                                            time(5, Rational{1, 1}));
      }};
  session.configure(clip(), time(0));
  assert(session.state() == AudioPlaybackState::Ready);
  assert(!output.accepted_blocks.empty());
  assert(output.accepted_blocks.front().samples()[0] == 0.5F);
}

void clip_gain_reaches_output_without_changing_audio_semantics() {
  FakeOutput attenuated_output;
  AudioPlaybackSession attenuated{
      &attenuated_output, [](const auto&, int) { return std::make_unique<FakeSource>(std::nullopt); }};
  attenuated.configure(clip(true, -6.0), time(0));
  assert(attenuated.condition() == application::AudioPlaybackCondition::Present);
  assert(attenuated_output.accepted_blocks.front().samples()[0] > 0.24F);
  assert(attenuated_output.accepted_blocks.front().samples()[0] < 0.26F);

  FakeOutput muted_output;
  AudioPlaybackSession muted{
      &muted_output, [](const auto&, int) { return std::make_unique<FakeSource>(std::nullopt); }};
  muted.configure(clip(true, -6.0, true), time(0));
  assert(muted.condition() == application::AudioPlaybackCondition::Present);
  for (const auto sample : muted_output.accepted_blocks.front().samples()) assert(sample == 0.0F);
}

void underrun_is_not_intentional_silence() {
  FakeOutput output;
  AudioPlaybackSession session{
      &output, [](const auto&, int) { return std::make_unique<FakeSource>(std::nullopt); }};
  session.configure(clip(), time(0));
  session.play();
  output.report_underrun();
  session.update(time(0));
  assert(session.state() == AudioPlaybackState::Starved);
  assert(session.condition() == application::AudioPlaybackCondition::Starved);
}

void underrun_recovers_through_prebuffer() {
  FakeOutput output;
  AudioPlaybackSession session{
      &output, [](const auto&, int) { return std::make_unique<FakeSource>(std::nullopt, 3); }};
  session.configure(clip(), time(0));
  session.play();
  output.report_underrun();
  session.update(time(0));
  assert(session.condition() == application::AudioPlaybackCondition::Starved);
  session.update(time(0));
  assert(session.state() == AudioPlaybackState::Ready);
  assert(session.condition() == application::AudioPlaybackCondition::Present);
  session.play();
  assert(session.state() == AudioPlaybackState::Running);
}

void no_device_is_explicit_fallback() {
  AudioPlaybackSession session{
      nullptr, [](const auto&, int) { return std::make_unique<FakeSource>(std::nullopt); }};
  session.configure(clip(), time(0));
  assert(session.state() == AudioPlaybackState::DeviceUnavailable);
  assert(session.condition() == application::AudioPlaybackCondition::DeviceUnavailable);
}

void decode_failure_is_terminal_and_not_silence() {
  FakeOutput output;
  AudioPlaybackSession session{
      &output, [](const auto&, int) { return std::make_unique<FailingSource>(); }};
  session.configure(clip(), time(0));
  assert(session.state() == AudioPlaybackState::Error);
  assert(session.condition() == application::AudioPlaybackCondition::Error);
  assert(session.error_message().has_value());
  session.update(time(0));
  assert(session.state() == AudioPlaybackState::Error);
}

void device_start_failure_is_degraded_not_media_eof() {
  FakeOutput output;
  output.start_ok = false;
  AudioPlaybackSession session{
      &output, [](const auto&, int) { return std::make_unique<FakeSource>(std::nullopt, 2); }};
  session.configure(clip(), time(0));
  session.play();
  assert(session.condition() == application::AudioPlaybackCondition::DeviceUnavailable);
  assert(session.condition() != application::AudioPlaybackCondition::SourceEndedEarly);
}

void decoder_starvation_recovers_without_becoming_eof() {
  FakeOutput output;
  AudioPlaybackSession session{
      &output, [](const auto&, int) { return std::make_unique<TemporarilyStarvedSource>(); }};
  session.configure(clip(), time(0));
  assert(session.state() == AudioPlaybackState::Starved);
  assert(session.condition() == application::AudioPlaybackCondition::Starved);
  session.update(time(0));
  assert(session.state() == AudioPlaybackState::Ready);
  assert(session.condition() == application::AudioPlaybackCondition::Present);
}

void legitimate_audio_eof_is_not_starvation() {
  FakeOutput output;
  AudioPlaybackSession session{
      &output, [](const auto&, int) { return std::make_unique<FakeSource>(std::nullopt); }};
  session.configure(clip(), time(0));
  session.play();
  output.advance(1000);
  session.update(time(0));
  assert(session.state() != AudioPlaybackState::Starved);
  output.advance(0);
  session.update(time(0));
  assert(session.state() == AudioPlaybackState::EndedEarly ||
         session.state() == AudioPlaybackState::Running);
  if (session.state() == AudioPlaybackState::EndedEarly) {
    assert(session.condition() == application::AudioPlaybackCondition::SourceEndedEarly);
  }
}

void long_run_clock_has_no_accumulating_float_drift() {
  FakeOutput output;
  AudioPlaybackSession session{
      &output, [](const auto&, int) { return std::make_unique<FakeSource>(std::nullopt, 700); }};
  session.configure(clip(), time(0));
  session.play();
  for (std::size_t block = 1; block <= 600; ++block) {
    output.advance(1000);
    session.update(time(0));
    assert(session.clock_position().has_value());
    assert(*session.clock_position() == time(static_cast<std::int64_t>(block), Rational{1, 1}));
  }
}

} // namespace

int main() {
  video_only_is_intentional_silence();
  source_origin_creates_silence_without_clock_jump();
  stream_origin_maps_presentation_relative_source();
  clip_gain_reaches_output_without_changing_audio_semantics();
  underrun_is_not_intentional_silence();
  underrun_recovers_through_prebuffer();
  no_device_is_explicit_fallback();
  decode_failure_is_terminal_and_not_silence();
  device_start_failure_is_degraded_not_media_eof();
  decoder_starvation_recovers_without_becoming_eof();
  legitimate_audio_eof_is_not_starvation();
  long_run_clock_has_no_accumulating_float_drift();
}
