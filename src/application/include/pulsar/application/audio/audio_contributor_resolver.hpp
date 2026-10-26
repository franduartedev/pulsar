#pragma once

#include <pulsar/application/project/project_snapshot.hpp>
#include <pulsar/domain/time/time_range.hpp>
#include <pulsar/domain/timeline/underwater.hpp>
#include <pulsar/domain/timeline/audio_effect.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pulsar::application {

// Immutable identity and editorial mapping for one source that may contribute
// audio to a requested timeline interval. Track order is retained to make
// resolution and floating-point summation deterministic.
struct AudioContributor {
  domain::TrackId track_id;
  domain::TrackKind track_kind;
  std::size_t track_order;
  domain::ClipId clip_id;
  domain::MediaId media_id;
  std::filesystem::path source_locator;
  int stream_index;
  domain::TimeRange timeline_range;
  domain::TimeRange source_range;
  std::optional<domain::MediaTime> stream_origin;
  double gain_db;
  bool muted;
  int sample_rate;
  int channel_count;
  domain::Rational time_base;
  domain::MediaTime fade_in{domain::MediaTime::zero()};
  domain::MediaTime fade_out{domain::MediaTime::zero()};
  double track_gain_db{0.0};
  bool track_muted{false};
  domain::DistantMusicSettings distant_music{};
  domain::TelephoneSettings telephone{};
  domain::UnderwaterSettings underwater{};
  std::vector<domain::AudioEffectType> audio_effect_order;
};

struct UnavailableAudioContributor {
  domain::TrackId track_id;
  domain::ClipId clip_id;
  domain::MediaId media_id;
  domain::TimeRange timeline_range;
  std::optional<std::string> reason;
};

struct AudioContributorResolution {
  std::vector<AudioContributor> contributors;
  std::vector<UnavailableAudioContributor> unavailable;
};

// Resolves every clip intersecting the half-open requested range. Embedded
// video audio is returned once from the video track; independent audio comes
// only from TrackKind::Audio. Result order is video first, then stable project
// track order and clip timeline order.
[[nodiscard]] AudioContributorResolution resolve_audio_contributors(
    const ProjectSnapshot& snapshot, domain::TimeRange timeline_range);

[[nodiscard]] domain::MediaTime contributor_source_position(
    const AudioContributor& contributor, domain::MediaTime timeline_position,
    std::optional<domain::MediaTime> runtime_stream_origin = std::nullopt);

} // namespace pulsar::application
