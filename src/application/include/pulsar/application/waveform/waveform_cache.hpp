#pragma once

#include <pulsar/application/media/audio_block.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pulsar::application {

struct WaveformBucket {
  std::int16_t minimum{0};
  std::int16_t maximum{0};
};

struct WaveformCacheKey {
  std::string source_identity;
  int stream_index{0};
  int sample_rate{0};
  int channel_count{0};
  domain::Rational time_base;
  std::optional<domain::MediaTime> stream_origin;

  [[nodiscard]] std::string stable_text() const;
  [[nodiscard]] bool operator==(const WaveformCacheKey& other) const;
};

[[nodiscard]] WaveformCacheKey make_waveform_cache_key(
    const std::filesystem::path& source_path, int stream_index, int sample_rate, int channel_count,
    domain::Rational time_base, std::optional<domain::MediaTime> stream_origin);

struct WaveformSourceInfo {
  int sample_rate{0};
  int channel_count{0};
  domain::Rational time_base;
  std::optional<domain::MediaTime> stream_origin;
};

class WaveformSource {
public:
  virtual ~WaveformSource() = default;
  [[nodiscard]] virtual WaveformSourceInfo info() const = 0;
  [[nodiscard]] virtual std::optional<AudioBlock> next_block() = 0;
};

enum class WaveformCacheStatus { Missing, Generating, Partial, Ready, Invalid, Failed };
enum class WaveformCompleteness { Missing, Partial, Complete };
struct WaveformQuery;
struct WaveformSlice;

struct WaveformBuildOptions {
  std::uint32_t level_zero_samples{256};
  std::uint32_t reduction_factor{2};
  std::size_t write_chunk_bytes{256 * 1024};
};

struct WaveformBuildReport {
  std::uint64_t source_frames{0};
  std::uint64_t level_zero_buckets{0};
  std::uint32_t level_count{0};
  std::uint64_t peak_input_frames{0};
  std::uint64_t peak_pending_buckets{0};
  bool cancelled{false};
};

using WaveformCancellation = std::function<bool()>;

class WaveformCacheBuilder {
public:
  WaveformCacheBuilder(WaveformCacheKey key, std::filesystem::path destination,
                       WaveformBuildOptions options = {});
  ~WaveformCacheBuilder();

  WaveformCacheBuilder(const WaveformCacheBuilder&) = delete;
  WaveformCacheBuilder& operator=(const WaveformCacheBuilder&) = delete;

  void consume(const AudioBlock& block, const WaveformCancellation& cancelled = {});
  [[nodiscard]] WaveformBuildReport finish();
  void cancel() noexcept;
  [[nodiscard]] const WaveformBuildReport& report() const noexcept { return report_; }
  [[nodiscard]] WaveformSlice query_partial(const WaveformQuery& query) const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
  WaveformBuildReport report_;
};

[[nodiscard]] WaveformBuildReport generate_waveform_cache(
    WaveformSource& source, WaveformCacheKey key, const std::filesystem::path& destination,
    const WaveformCancellation& cancelled = {}, WaveformBuildOptions options = {});

struct WaveformQuery {
  domain::MediaTime source_start;
  domain::MediaTime source_end;
  std::uint32_t pixel_width{1};
  double density{1.0};
};

struct WaveformSlice {
  domain::MediaTime requested_start{domain::MediaTime::zero()};
  domain::MediaTime requested_end{domain::MediaTime::zero()};
  domain::MediaTime available_start{domain::MediaTime::zero()};
  domain::MediaTime available_end{domain::MediaTime::zero()};
  std::uint32_t level{0};
  std::uint64_t bucket_sample_span{0};
  int channel_count{0};
  std::uint64_t first_bucket{0};
  std::uint64_t bucket_count{0};
  std::vector<WaveformBucket> buckets; // bucket-major: [bucket][channel]
  WaveformCompleteness completeness{WaveformCompleteness::Missing};
};

class WaveformCacheReader {
public:
  WaveformCacheReader() = default;
  ~WaveformCacheReader();
  WaveformCacheReader(WaveformCacheReader&&) noexcept;
  WaveformCacheReader& operator=(WaveformCacheReader&&) noexcept;
  WaveformCacheReader(const WaveformCacheReader&) = delete;
  WaveformCacheReader& operator=(const WaveformCacheReader&) = delete;

  [[nodiscard]] static std::optional<WaveformCacheReader>
  open(const std::filesystem::path& path, const WaveformCacheKey& expected_key);
  [[nodiscard]] WaveformSlice query(const WaveformQuery& query) const;
  [[nodiscard]] const WaveformCacheKey& key() const noexcept;
  [[nodiscard]] WaveformCacheStatus status() const noexcept;
  [[nodiscard]] std::uint64_t source_frames() const noexcept;
  [[nodiscard]] std::uint32_t level_count() const noexcept;

private:
  class Impl;
  explicit WaveformCacheReader(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::filesystem::path waveform_cache_path(const std::filesystem::path& directory,
                                                         const WaveformCacheKey& key);

} // namespace pulsar::application
