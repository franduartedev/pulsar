#include <pulsar/application/playback/video_source.hpp>

namespace pulsar::application {

SingleStreamPlaybackClockMap::SingleStreamPlaybackClockMap(
    StreamTimestamp playback_zero_stream_timestamp)
    : playback_zero_stream_timestamp_{playback_zero_stream_timestamp} {}

StreamTimestamp SingleStreamPlaybackClockMap::to_stream_timestamp(PlaybackPosition position) const {
  return StreamTimestamp{playback_zero_stream_timestamp_.value() + position.value()};
}

} // namespace pulsar::application
