#pragma once

#include <pulsar/application/playback/playback_time.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace pulsar::application {

struct VideoSourceDescriptor {
  std::optional<StreamTimestamp> stream_origin;
  std::optional<domain::Rational> stream_time_base;
  std::optional<domain::MediaTime> reported_duration;
  bool seekable;
};

class SingleStreamPlaybackClockMap {
public:
  explicit SingleStreamPlaybackClockMap(StreamTimestamp playback_zero_stream_timestamp);

  [[nodiscard]] StreamTimestamp to_stream_timestamp(PlaybackPosition position) const;
  [[nodiscard]] StreamTimestamp playback_zero_stream_timestamp() const noexcept {
    return playback_zero_stream_timestamp_;
  }

private:
  StreamTimestamp playback_zero_stream_timestamp_;
};

struct VideoSample {
  std::uint64_t ordinal;
  std::optional<StreamTimestamp> presentation_timestamp;
  std::optional<domain::MediaTime> duration;
};

struct VideoSourceEndOfStream {
  std::optional<StreamTimestamp> known_presentation_end;
  bool terminal_end_known;
};

struct VideoSourceCancelled {};

struct VideoSourceError {
  std::string message;
};

using VideoReadOutcome =
    std::variant<VideoSample, VideoSourceEndOfStream, VideoSourceCancelled, VideoSourceError>;

struct VideoSeekHit {
  VideoSample sample;
  std::optional<VideoSample> lookahead;
};

struct VideoSeekBeforeFirst {
  std::optional<VideoSample> first_sample;
};

struct VideoSeekGap {
  VideoSample before;
  std::optional<VideoSample> after;
};

struct VideoSeekEndOfStream {
  std::optional<VideoSample> last_sample;
  std::optional<StreamTimestamp> known_presentation_end;
  bool terminal_end_known;
};

using VideoSeekOutcome = std::variant<VideoSeekHit, VideoSeekBeforeFirst, VideoSeekGap,
                                      VideoSeekEndOfStream, VideoSourceCancelled,
                                      VideoSourceError>;

class VideoSource {
public:
  virtual ~VideoSource() = default;

  [[nodiscard]] virtual VideoSourceDescriptor describe() const = 0;
  [[nodiscard]] virtual VideoReadOutcome read_next() = 0;
  [[nodiscard]] virtual VideoSeekOutcome seek_for_presentation(StreamTimestamp target) = 0;
};

} // namespace pulsar::application
