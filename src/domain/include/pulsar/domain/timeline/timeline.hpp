#pragma once

#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/track.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <optional>
#include <vector>

namespace pulsar::domain {

class Timeline {
public:
  explicit Timeline(TimelineId id);

  [[nodiscard]] constexpr TimelineId id() const noexcept {
    return id_;
  }

  [[nodiscard]] const std::vector<Track>& tracks() const noexcept {
    return tracks_;
  }

  void add_track(Track track);
  void remove_track(TrackId track_id);
  void insert_clip(TrackId track_id, Clip clip);
  void remove_clip(ClipId clip_id);
  void move_clip(ClipId clip_id, TrackId destination_track_id, MediaTime timeline_start);
  void trim_clip_start(ClipId clip_id, MediaTime timeline_start);
  void trim_clip_end(ClipId clip_id, MediaTime timeline_end);
  void split_clip(ClipId clip_id, MediaTime split_timeline_position, ClipId right_clip_id);
  void set_clip_audio_gain(ClipId clip_id, double gain_db);
  void set_clip_muted(ClipId clip_id, bool muted);
  void set_clip_fades(ClipId clip_id, MediaTime fade_in, MediaTime fade_out);
  void set_clip_distant_music(ClipId clip_id, DistantMusicSettings settings);
  void set_clip_telephone(ClipId clip_id, TelephoneSettings settings);
  void set_clip_underwater(ClipId clip_id, UnderwaterSettings settings);
  void set_clip_audio_effect_order(ClipId clip_id, std::vector<AudioEffectType> order);
  void set_clip_visual_transform(ClipId clip_id, VisualTransform transform);
  void set_track_audio_state(TrackId track_id, AudioTrackState state);

  // Internal value restoration used by application history; validates the complete replacement
  // before publishing it.
  void restore_clip(TrackId track_id, Clip clip);
  void restore_split(TrackId track_id, Clip original_clip, ClipId right_clip_id);

  // Returned pointers/references are observations only. Do not retain them across
  // Timeline or Track mutation; vector storage and copied replacement tracks may invalidate them.
  [[nodiscard]] const Track* find_track(TrackId id) const noexcept;
  [[nodiscard]] const Clip* find_clip(ClipId id) const noexcept;
  // Returns a value copy so callers cannot retain a domain reference across mutation.
  [[nodiscard]] std::optional<Clip> find_clip_at(TrackId track_id,
                                                  MediaTime timeline_position) const;
  [[nodiscard]] MediaTime duration() const;

private:
  [[nodiscard]] Track* find_track_mutable(TrackId id) noexcept;
  [[nodiscard]] std::size_t find_track_index(TrackId id) const;

  TimelineId id_;
  std::vector<Track> tracks_;
};

} // namespace pulsar::domain
