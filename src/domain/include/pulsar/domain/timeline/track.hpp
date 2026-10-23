#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <optional>
#include <vector>

namespace pulsar::domain {

struct AudioTrackState {
  double gain_db{0.0};
  bool muted{false};
  bool solo{false};
  friend bool operator==(const AudioTrackState&, const AudioTrackState&) = default;
};

void validate_audio_track_state(const AudioTrackState& state);

class Track {
public:
  Track(TrackId id, TrackKind kind, AudioTrackState audio_state = {});

  [[nodiscard]] constexpr TrackId id() const noexcept {
    return id_;
  }

  [[nodiscard]] constexpr TrackKind kind() const noexcept {
    return kind_;
  }

  [[nodiscard]] const std::vector<Clip>& clips() const noexcept {
    return clips_;
  }
  [[nodiscard]] const AudioTrackState& audio_state() const noexcept { return audio_state_; }

  void insert_clip(Clip clip);

  // Editorial policy for A9.0: clips do not overlap within one track. Multiple
  // audio tracks are allowed, so future mixing can combine independent tracks
  // without changing this track-local invariant.

  // Returned pointers/references are observations only. Do not retain them across mutation.
  [[nodiscard]] const Clip* find_clip(ClipId id) const noexcept;
  // Returns a value copy so callers cannot retain a domain reference across mutation.
  [[nodiscard]] std::optional<Clip> find_clip_at(MediaTime timeline_position) const;
  [[nodiscard]] MediaTime duration() const;

private:
  friend class Timeline;

  void remove_clip(ClipId id);
  void trim_clip_start(ClipId id, MediaTime timeline_start);
  void trim_clip_end(ClipId id, MediaTime timeline_end);
  void split_clip(ClipId id, MediaTime split_timeline_position, ClipId right_clip_id);
  void set_clip_audio_gain(ClipId id, double gain_db);
  void set_clip_muted(ClipId id, bool muted);
  void set_clip_fades(ClipId id, MediaTime fade_in, MediaTime fade_out);
  void set_clip_distant_music(ClipId id, DistantMusicSettings settings);
  void set_clip_visual_transform(ClipId id, VisualTransform transform);
  void set_audio_state(AudioTrackState state);

  TrackId id_;
  TrackKind kind_;
  std::vector<Clip> clips_;
  AudioTrackState audio_state_;
};

} // namespace pulsar::domain
