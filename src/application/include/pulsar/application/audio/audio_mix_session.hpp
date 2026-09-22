#pragma once

#include <pulsar/application/audio/audio_contributor_resolver.hpp>
#include <pulsar/application/media/audio_block.hpp>
#include <pulsar/application/playback/audio_playback_session.hpp>
#include <pulsar/application/project/project_snapshot.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pulsar::application {

struct AudioMixFormat {
  int sample_rate{48'000};
  int channel_count{2};
};

enum class AudioContributorFailure {
  Unavailable,
  UnsupportedChannels,
  SourceOpen,
  SourceSeek,
  Starved,
  Decode,
  EndedEarly,
};

struct AudioMixIssue {
  domain::TrackId track_id;
  domain::ClipId clip_id;
  domain::MediaId media_id;
  AudioContributorFailure failure;
  std::string message;
};

enum class AudioMixClockRecommendation {
  Software,
  Device,
};

struct AudioMixRenderResult {
  AudioBlock block;
  domain::MediaTime timeline_position;
  std::vector<AudioMixIssue> issues;
  std::size_t resolved_contributors{0};
  // Sources whose aligned PCM advanced during this render. Muted contributors
  // remain usable and clock-capable even though they contribute audible zero.
  std::size_t usable_contributors{0};
  // Unmuted usable contributors whose PCM passed through gain into the sum.
  // This is deliberately not a clock-availability proxy.
  std::size_t mixed_contributors{0};
  AudioMixClockRecommendation clock{AudioMixClockRecommendation::Software};
};

struct AudioMixMetrics {
  std::size_t live_sources{0};
  std::size_t peak_live_sources{0};
  std::size_t queued_frames{0};
  std::size_t peak_queued_frames{0};
  std::uint64_t source_opens{0};
  std::uint64_t decoded_blocks{0};
};

// Application-owned preview mixer. It emits one canonical Float32 interleaved
// PCM stream. It never owns an AudioOutput: A9.3 may enqueue this one stream
// into the existing single-device pipeline and use the device clock iff a
// render recommends Device and that output is actually Running.
class AudioMixSession final {
public:
  static constexpr std::size_t maximum_render_frames = 8192;
  static constexpr std::size_t maximum_source_block_frames = 32768;
  static constexpr std::size_t maximum_queued_frames_per_source = 65536;

  explicit AudioMixSession(AudioPlaybackSourceFactory source_factory,
                           AudioMixFormat format = {});
  ~AudioMixSession();

  AudioMixSession(const AudioMixSession&) = delete;
  AudioMixSession& operator=(const AudioMixSession&) = delete;

  // Publishing a snapshot is the generation boundary. Existing decoders and
  // queued PCM are discarded atomically before the new truth can render.
  void configure(ProjectSnapshot snapshot);
  void reset() noexcept;

  [[nodiscard]] AudioMixRenderResult render(domain::MediaTime timeline_position,
                                             std::size_t frame_count);
  [[nodiscard]] AudioMixFormat format() const noexcept { return format_; }
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] const AudioMixMetrics& metrics() const noexcept { return metrics_; }

private:
  class SourceState;

  [[nodiscard]] SourceState* find_source(const AudioContributor& contributor) noexcept;
  [[nodiscard]] SourceState* open_source(const AudioContributor& contributor,
                                         domain::MediaTime timeline_position,
                                         std::vector<AudioMixIssue>& issues);
  void prune_sources(const std::vector<AudioContributor>& contributors);
  void select_range(domain::MediaTime start, domain::MediaTime end, bool discontinuity);
  void update_metrics() noexcept;

  AudioPlaybackSourceFactory source_factory_;
  AudioMixFormat format_;
  std::optional<ProjectSnapshot> snapshot_;
  std::vector<AudioContributor> contributors_;
  std::vector<UnavailableAudioContributor> unavailable_;
  std::vector<AudioContributor> active_contributors_;
  std::vector<UnavailableAudioContributor> active_unavailable_;
  std::size_t next_contributor_{0};
  std::size_t next_unavailable_{0};
  std::vector<std::unique_ptr<SourceState>> sources_;
  std::optional<domain::MediaTime> next_timeline_position_;
  std::uint64_t generation_{0};
  AudioMixMetrics metrics_;
};

} // namespace pulsar::application
