#pragma once

#include <pulsar/application/audio/audio_contributor_resolver.hpp>
#include <pulsar/application/audio/clip_gain.hpp>
#include <pulsar/application/export/export_plan.hpp>

#include <cstdint>
#include <array>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace pulsar::application::exporting {

struct OfflineAudioSourceBlock {
  int sample_rate;
  int channel_count;
  std::size_t frame_count;
  std::vector<float> samples;
  // Timestamp in raw source-stream time coordinates. The renderer subtracts
  // OfflineAudioSource::stream_origin() before editorial alignment.
  std::optional<domain::MediaTime> presentation_timestamp;
};

class OfflineAudioSource {
public:
  virtual ~OfflineAudioSource() = default;
  [[nodiscard]] virtual int sample_rate() const noexcept = 0;
  [[nodiscard]] virtual int channel_count() const noexcept = 0;
  [[nodiscard]] virtual std::optional<domain::MediaTime> stream_origin() const = 0;
  // Timestamp is expressed in raw stream coordinates, matching stream_origin().
  virtual void seek_to(domain::MediaTime stream_timestamp) = 0;
  [[nodiscard]] virtual std::optional<OfflineAudioSourceBlock> decode_next() = 0;
};

class OfflineAudioSourceFactory {
public:
  virtual ~OfflineAudioSourceFactory() = default;
  [[nodiscard]] virtual std::unique_ptr<OfflineAudioSource> open(
      const std::filesystem::path& logical_locator, int audio_stream_index) = 0;
};

enum class OfflineAudioRenderErrorCode {
  SourceOpenFailed,
  SeekFailed,
  DecodeFailed,
  TimestampMissing,
  SourceEndedEarly,
  UnsupportedChannels,
  InvalidSourceFormat,
  MissingMedia,
  ResampleFailure,
  InternalInvariant,
};

class OfflineAudioRenderError final : public std::runtime_error {
public:
  OfflineAudioRenderError(OfflineAudioRenderErrorCode code, std::string message)
      : std::runtime_error{std::move(message)}, code_{code} {}

  [[nodiscard]] OfflineAudioRenderErrorCode code() const noexcept { return code_; }

private:
  OfflineAudioRenderErrorCode code_;
};

struct OfflineAudioBlock {
  std::int64_t first_sample_index;
  std::size_t frame_count;
  int sample_rate;
  int channel_count;
  std::vector<float> samples;
};

struct OfflineAudioRenderMetrics {
  std::uint64_t output_samples{0};
  std::uint64_t source_opens{0};
  std::uint64_t seeks{0};
  std::uint64_t decoded_blocks{0};
  std::uint64_t decoded_source_frames{0};
  std::uint64_t inserted_source_gap_silence{0};
  std::uint64_t trimmed_overlap_frames{0};
  std::uint64_t peak_active_sources{0};
  std::uint64_t peak_buffered_source_frames{0};
};

class OfflineAudioRenderer final {
public:
  static constexpr std::size_t default_max_render_frames = 4096;

  OfflineAudioRenderer(const ExportPlan& plan, OfflineAudioSourceFactory& source_factory,
                       std::size_t max_render_frames = default_max_render_frames);
  ~OfflineAudioRenderer();

  OfflineAudioRenderer(const OfflineAudioRenderer&) = delete;
  OfflineAudioRenderer& operator=(const OfflineAudioRenderer&) = delete;

  [[nodiscard]] std::optional<OfflineAudioBlock> render_next_block();
  [[nodiscard]] const OfflineAudioRenderMetrics& metrics() const noexcept { return metrics_; }

private:
  struct SourceBlock;
  struct ContributorState;

  [[nodiscard]] ContributorState& state_for(const AudioContributor& contributor);
  void load_until(ContributorState& state, domain::MediaTime relative_time);
  [[nodiscard]] float sample_at_channel(ContributorState& state,
                                        domain::MediaTime relative_time, int channel);
  [[nodiscard]] std::array<float, 2> contributor_sample(ContributorState& state,
                                                         domain::MediaTime timeline_time,
                                                         std::int64_t output_frame);
  [[nodiscard]] static float channel_sample(const SourceBlock& block, std::int64_t index,
                                            int channel);
  [[nodiscard]] static std::string render_context(const ContributorState& state,
                                                   domain::MediaTime timeline_time,
                                                   domain::MediaTime source_time,
                                                   domain::MediaTime source_grid_time,
                                                   domain::MediaTime next_grid_time);
  void update_buffer_metrics() noexcept;
  void prune_inactive_states(const std::vector<AudioContributor>& active);

  const ExportPlan& plan_;
  OfflineAudioSourceFactory& source_factory_;
  std::size_t max_render_frames_;
  std::int64_t next_sample_index_{0};
  std::unordered_map<std::uint64_t, std::unique_ptr<ContributorState>> states_;
  OfflineAudioRenderMetrics metrics_;
};

} // namespace pulsar::application::exporting
