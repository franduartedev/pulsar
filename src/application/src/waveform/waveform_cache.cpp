#include <pulsar/application/waveform/waveform_cache.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace pulsar::application {
namespace {

constexpr std::array<char, 8> kMagic{'P', 'U', 'L', 'S', 'A', 'R', 'W', 'F'};
constexpr std::uint32_t kSchema = 1;
constexpr std::uint32_t kGenerator = 1;

void write_u32(std::ostream& out, std::uint32_t value) {
  for (int i = 0; i < 4; ++i) out.put(static_cast<char>((value >> (i * 8)) & 0xff));
}
void write_u64(std::ostream& out, std::uint64_t value) {
  for (int i = 0; i < 8; ++i) out.put(static_cast<char>((value >> (i * 8)) & 0xff));
}
void write_i64(std::ostream& out, std::int64_t value) { write_u64(out, static_cast<std::uint64_t>(value)); }
void write_i16(std::ostream& out, std::int16_t value) {
  const auto bits = static_cast<std::uint16_t>(value);
  out.put(static_cast<char>(bits & 0xff));
  out.put(static_cast<char>((bits >> 8) & 0xff));
}
std::uint32_t read_u32(std::istream& in) {
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i) { const int c = in.get(); if (c < 0) throw std::runtime_error{"truncated waveform cache"}; value |= static_cast<std::uint32_t>(c) << (i * 8); }
  return value;
}
std::uint64_t read_u64(std::istream& in) {
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i) { const int c = in.get(); if (c < 0) throw std::runtime_error{"truncated waveform cache"}; value |= static_cast<std::uint64_t>(c) << (i * 8); }
  return value;
}
std::int64_t read_i64(std::istream& in) { return static_cast<std::int64_t>(read_u64(in)); }
std::int16_t read_i16(std::istream& in) {
  const int lo = in.get(); const int hi = in.get();
  if (lo < 0 || hi < 0) throw std::runtime_error{"truncated waveform bucket"};
  const auto v = static_cast<std::uint16_t>(lo | (hi << 8));
  return static_cast<std::int16_t>(v);
}
void write_string(std::ostream& out, const std::string& value) { write_u64(out, value.size()); out.write(value.data(), static_cast<std::streamsize>(value.size())); }
std::string read_string(std::istream& in) {
  const auto size = read_u64(in); if (size > 16 * 1024 * 1024) throw std::runtime_error{"invalid waveform key size"};
  std::string value(size, '\0'); in.read(value.data(), static_cast<std::streamsize>(size)); if (!in) throw std::runtime_error{"truncated waveform key"}; return value;
}

std::int64_t sample_index(domain::MediaTime time, domain::MediaTime origin, int rate, bool round_up) {
  const auto seconds = (time - origin).seconds();
  auto numerator = seconds.numerator();
  auto denominator = seconds.denominator();
  auto rate_factor = static_cast<std::int64_t>(rate);
  const auto rate_gcd = std::gcd(rate_factor, denominator);
  rate_factor /= rate_gcd;
  denominator /= rate_gcd;
  const auto numerator_gcd = std::gcd(numerator < 0 ? -(numerator + 1) + 1 : numerator, denominator);
  if (numerator_gcd != 0) { numerator /= numerator_gcd; denominator /= numerator_gcd; }
  if (numerator != 0 && (numerator > std::numeric_limits<std::int64_t>::max() / rate_factor || numerator < std::numeric_limits<std::int64_t>::min() / rate_factor)) throw std::overflow_error{"waveform sample index overflow"};
  numerator *= rate_factor;
  auto quotient = numerator / denominator;
  const auto remainder = numerator % denominator;
  if (!round_up && remainder < 0) --quotient;
  if (round_up && remainder > 0) ++quotient;
  return quotient;
}

std::int16_t quantize(float value) {
  if (!std::isfinite(value)) throw std::invalid_argument{"waveform source contains non-finite PCM"};
  if (value <= -1.0f) return std::numeric_limits<std::int16_t>::min();
  if (value >= 1.0f) return std::numeric_limits<std::int16_t>::max();
  const double scaled = value < 0.0f ? static_cast<double>(value) * 32768.0 : static_cast<double>(value) * 32767.0;
  return static_cast<std::int16_t>(std::lround(scaled));
}

struct LevelIndex { std::uint64_t offset{0}; std::uint64_t count{0}; };

} // namespace

std::string WaveformCacheKey::stable_text() const {
  return source_identity + "|" + std::to_string(stream_index) + "|" + std::to_string(sample_rate) + "|" +
         std::to_string(channel_count) + "|" + std::to_string(time_base.numerator()) + "/" +
         std::to_string(time_base.denominator()) + "|" +
         (stream_origin ? std::to_string(stream_origin->value()) + ":" + std::to_string(stream_origin->time_base().numerator()) + "/" + std::to_string(stream_origin->time_base().denominator()) : "none");
}
bool WaveformCacheKey::operator==(const WaveformCacheKey& other) const { return stable_text() == other.stable_text(); }

WaveformCacheKey make_waveform_cache_key(const std::filesystem::path& source_path, int stream_index,
                                         int sample_rate, int channel_count, domain::Rational time_base,
                                         std::optional<domain::MediaTime> stream_origin) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(source_path, error);
  const auto identity_path = error ? source_path.lexically_normal() : canonical;
  const auto size = std::filesystem::file_size(source_path, error);
  const auto size_text = error ? std::string{"missing"} : std::to_string(size);
  error.clear();
  const auto modified = std::filesystem::last_write_time(source_path, error);
  const auto modified_text = error ? std::string{"unknown"} : std::to_string(modified.time_since_epoch().count());
  return {identity_path.generic_string() + "|size=" + size_text + "|mtime=" + modified_text,
          stream_index, sample_rate, channel_count, time_base, stream_origin};
}

class WaveformCacheBuilder::Impl {
public:
  Impl(WaveformCacheKey key, std::filesystem::path destination, WaveformBuildOptions options)
      : key_{std::move(key)}, destination_{std::move(destination)}, options_{options} {
    if (options_.level_zero_samples == 0 || options_.reduction_factor != 2 || key_.sample_rate <= 0 || key_.channel_count <= 0) throw std::invalid_argument{"invalid waveform cache configuration"};
    if (!destination_.parent_path().empty()) std::filesystem::create_directories(destination_.parent_path());
    static std::atomic<std::uint64_t> serial{0};
    stem_ = destination_.string() + ".partial." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "." + std::to_string(serial.fetch_add(1));
    level_paths_.push_back(stem_ + ".level0");
    level_out_.emplace_back(level_paths_.front(), std::ios::binary | std::ios::trunc);
    if (!level_out_.back()) throw std::runtime_error{"cannot create waveform cache temporary"};
    minimum_.assign(key_.channel_count, std::numeric_limits<std::int16_t>::max());
    maximum_.assign(key_.channel_count, std::numeric_limits<std::int16_t>::min());
    pending_min_.emplace_back(key_.channel_count, std::numeric_limits<std::int16_t>::max());
    pending_max_.emplace_back(key_.channel_count, std::numeric_limits<std::int16_t>::min());
    pending_count_.push_back(0);
    publishing_path_ = stem_ + ".publishing";
  }
  ~Impl() { cleanup(); }
  void consume(const AudioBlock& block, const WaveformCancellation& cancelled, WaveformBuildReport& report) {
    if (finished_ || cancelled_) throw std::logic_error{"waveform builder is not active"};
    if (block.sample_rate() != key_.sample_rate || block.channel_count() != key_.channel_count) throw std::invalid_argument{"waveform PCM format does not match cache key"};
    report.peak_input_frames = std::max(report.peak_input_frames, static_cast<std::uint64_t>(block.frame_count()));
    const auto& samples = block.samples();
    for (std::size_t frame = 0; frame < block.frame_count(); ++frame) {
      if (cancelled && cancelled()) { cancel(); report.cancelled = true; return; }
      for (int channel = 0; channel < key_.channel_count; ++channel) {
        const auto q = quantize(samples[frame * static_cast<std::size_t>(key_.channel_count) + static_cast<std::size_t>(channel)]);
        minimum_[channel] = std::min(minimum_[channel], q); maximum_[channel] = std::max(maximum_[channel], q);
      }
      ++frames_in_bucket_;
      ++source_frames_;
      ++report.source_frames;
      if (frames_in_bucket_ == options_.level_zero_samples) emit_bucket(0, minimum_, maximum_, report);
    }
  }
  void cancel() noexcept { cancelled_ = true; cleanup(); }
  WaveformSlice partial_query(const WaveformQuery& request) {
    if (request.pixel_width == 0 || request.density <= 0.0 || !std::isfinite(request.density) || request.source_end <= request.source_start) throw std::invalid_argument{"invalid waveform query"};
    for (auto& out : level_out_) out.flush();
    WaveformSlice result; result.requested_start = request.source_start; result.requested_end = request.source_end; result.channel_count = key_.channel_count; result.completeness = WaveformCompleteness::Partial;
    const auto origin = key_.stream_origin.value_or(domain::MediaTime::zero()); result.available_start = origin; result.available_end = origin + domain::MediaTime{static_cast<std::int64_t>(source_frames_), domain::Rational{1, key_.sample_rate}};
    const auto delta = request.source_end - request.source_start; const auto delta_seconds = delta.seconds();
    const auto target = static_cast<long double>(delta_seconds.numerator()) / delta_seconds.denominator() * key_.sample_rate / request.pixel_width / request.density;
    std::uint32_t level = 0; std::uint64_t span = options_.level_zero_samples;
    while (level + 1 < level_paths_.size() && static_cast<long double>(span * options_.reduction_factor) <= target) { span *= options_.reduction_factor; ++level; }
    const auto first_sample = std::max<std::int64_t>(0, sample_index(request.source_start, origin, key_.sample_rate, true));
    const auto end_sample = std::min<std::int64_t>(static_cast<std::int64_t>(source_frames_), sample_index(request.source_end, origin, key_.sample_rate, true));
    const auto first = static_cast<std::uint64_t>(first_sample) / span; const auto last = end_sample > first_sample ? (static_cast<std::uint64_t>(end_sample) + span - 1) / span : first;
    const auto bytes = std::filesystem::file_size(level_paths_[level]); const auto available = bytes / (static_cast<std::uint64_t>(key_.channel_count) * 4U); const auto count = std::min(last, available) > first ? std::min(last, available) - first : 0;
    result.level = level; result.bucket_sample_span = span; result.first_bucket = first; result.bucket_count = count; result.buckets.resize(static_cast<std::size_t>(count * key_.channel_count));
    std::ifstream in(level_paths_[level], std::ios::binary); in.seekg(static_cast<std::streamoff>(first * key_.channel_count * 4U)); for (auto& bucket : result.buckets) { bucket.minimum = read_i16(in); bucket.maximum = read_i16(in); }
    return result;
  }
  WaveformBuildReport finish(WaveformBuildReport report) {
    if (finished_) throw std::logic_error{"waveform cache already finished"};
    if (cancelled_) { report.cancelled = true; return report; }
    if (frames_in_bucket_ != 0) emit_bucket(0, minimum_, maximum_, report);
    for (std::size_t level = 0; level < pending_count_.size(); ++level) {
      if (pending_count_[level] == 1 && level + 1 < level_out_.size()) {
        pending_count_[level] = 0;
        emit_bucket(static_cast<std::uint32_t>(level + 1), pending_min_[level], pending_max_[level], report);
      }
    }
    for (auto& out : level_out_) out.close();
    std::vector<LevelIndex> indexes;
    std::uint64_t total_buckets = 0;
    for (std::size_t i = 0; i < level_paths_.size(); ++i) {
      const auto bytes = std::filesystem::file_size(level_paths_[i]);
      if (bytes % (static_cast<std::uint64_t>(key_.channel_count) * 4U) != 0) throw std::runtime_error{"invalid temporary waveform level"};
      indexes.push_back(LevelIndex{0, bytes / (static_cast<std::uint64_t>(key_.channel_count) * 4U)});
      total_buckets += indexes.back().count;
    }
    report.level_count = static_cast<std::uint32_t>(indexes.size());
    report.level_zero_buckets = indexes.front().count;
    std::ofstream out(publishing_path_, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error{"cannot publish waveform cache"};
    out.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size())); write_u32(out, kSchema); write_u32(out, kGenerator);
    write_string(out, key_.stable_text()); write_u32(out, static_cast<std::uint32_t>(key_.sample_rate)); write_u32(out, static_cast<std::uint32_t>(key_.channel_count));
    write_i64(out, key_.time_base.numerator()); write_i64(out, key_.time_base.denominator());
    write_i64(out, key_.stream_origin ? key_.stream_origin->value() : 0); write_i64(out, key_.stream_origin ? key_.stream_origin->time_base().numerator() : 0); write_i64(out, key_.stream_origin ? key_.stream_origin->time_base().denominator() : 1);
    write_u32(out, options_.level_zero_samples); write_u32(out, options_.reduction_factor); write_u32(out, static_cast<std::uint32_t>(indexes.size())); write_u32(out, 1);
    write_u64(out, report.source_frames);
    const auto index_start = static_cast<std::uint64_t>(out.tellp());
    for (const auto& index : indexes) { write_u64(out, 0); write_u64(out, index.count); }
    const std::uint64_t payload_start = static_cast<std::uint64_t>(out.tellp());
    std::uint64_t offset = payload_start;
    for (auto& index : indexes) { index.offset = offset; offset += index.count * static_cast<std::uint64_t>(key_.channel_count) * 4U; }
    out.seekp(static_cast<std::streamoff>(index_start)); for (const auto& index : indexes) { write_u64(out, index.offset); write_u64(out, index.count); }
    out.seekp(static_cast<std::streamoff>(payload_start));
    for (const auto& path : level_paths_) { std::ifstream in(path, std::ios::binary); out << in.rdbuf(); if (!out) throw std::runtime_error{"waveform cache write failed"}; }
    out.close(); if (!out) throw std::runtime_error{"waveform cache close failed"};
    std::filesystem::rename(publishing_path_, destination_);
    finished_ = true; cleanup(); return report;
  }
private:
  void emit_bucket(std::uint32_t level, const std::vector<std::int16_t>& min, const std::vector<std::int16_t>& max, WaveformBuildReport& report) {
    while (level >= level_out_.size()) {
      level_paths_.push_back(stem_ + ".level" + std::to_string(level));
      level_out_.emplace_back(level_paths_.back(), std::ios::binary | std::ios::trunc);
      pending_min_.emplace_back(key_.channel_count, std::numeric_limits<std::int16_t>::max());
      pending_max_.emplace_back(key_.channel_count, std::numeric_limits<std::int16_t>::min());
      pending_count_.push_back(0);
    }
    for (int channel = 0; channel < key_.channel_count; ++channel) { write_i16(level_out_[level], min[channel]); write_i16(level_out_[level], max[channel]); }
    reduce_bucket(level, min, max, report);
    if (level == 0) { frames_in_bucket_ = 0; std::fill(minimum_.begin(), minimum_.end(), std::numeric_limits<std::int16_t>::max()); std::fill(maximum_.begin(), maximum_.end(), std::numeric_limits<std::int16_t>::min()); }
  }
  void reduce_bucket(std::uint32_t level, const std::vector<std::int16_t>& min, const std::vector<std::int16_t>& max, WaveformBuildReport& report) {
    if (pending_count_[level] == 0) {
      pending_min_[level] = min; pending_max_[level] = max; pending_count_[level] = 1;
    } else {
      for (int c = 0; c < key_.channel_count; ++c) { pending_min_[level][c] = std::min(pending_min_[level][c], min[c]); pending_max_[level][c] = std::max(pending_max_[level][c], max[c]); }
      pending_count_[level] = 0; emit_bucket(level + 1, pending_min_[level], pending_max_[level], report);
    }
    report.peak_pending_buckets = std::max(report.peak_pending_buckets, static_cast<std::uint64_t>(pending_count_.size()));
  }
  void cleanup() noexcept {
    for (auto& out : level_out_) if (out.is_open()) out.close();
    for (const auto& path : level_paths_) { std::error_code ec; std::filesystem::remove(path, ec); }
    { std::error_code ec; std::filesystem::remove(publishing_path_, ec); }
  }
  WaveformCacheKey key_; std::filesystem::path destination_; WaveformBuildOptions options_; std::string stem_; std::string publishing_path_; std::vector<std::filesystem::path> level_paths_; std::vector<std::ofstream> level_out_; std::vector<std::int16_t> minimum_, maximum_; std::deque<std::vector<std::int16_t>> pending_min_, pending_max_; std::vector<std::uint32_t> pending_count_; std::uint64_t frames_in_bucket_{0}; std::uint64_t source_frames_{0}; bool cancelled_{false}; bool finished_{false};
};

WaveformCacheBuilder::WaveformCacheBuilder(WaveformCacheKey key, std::filesystem::path destination, WaveformBuildOptions options) : impl_{std::make_unique<Impl>(std::move(key), std::move(destination), options)} {}
WaveformCacheBuilder::~WaveformCacheBuilder() = default;
void WaveformCacheBuilder::consume(const AudioBlock& block, const WaveformCancellation& cancelled) { impl_->consume(block, cancelled, report_); }
WaveformBuildReport WaveformCacheBuilder::finish() { return impl_->finish(report_); }
void WaveformCacheBuilder::cancel() noexcept { impl_->cancel(); report_.cancelled = true; }
WaveformSlice WaveformCacheBuilder::query_partial(const WaveformQuery& query) const { return impl_->partial_query(query); }
WaveformBuildReport generate_waveform_cache(WaveformSource& source, WaveformCacheKey key, const std::filesystem::path& destination, const WaveformCancellation& cancelled, WaveformBuildOptions options) {
  const auto info = source.info(); if (key.sample_rate != info.sample_rate || key.channel_count != info.channel_count || key.time_base != info.time_base || key.stream_origin != info.stream_origin) throw std::invalid_argument{"waveform source metadata does not match key"};
  WaveformCacheBuilder builder{std::move(key), destination, options}; while (auto block = source.next_block()) { builder.consume(*block, cancelled); if (builder.report().cancelled) return builder.report(); } return builder.finish();
}

class WaveformCacheReader::Impl {
public:
  std::filesystem::path path; WaveformCacheKey key; int sample_rate{0}; int channels{0}; domain::Rational time_base; std::optional<domain::MediaTime> origin; std::uint32_t l0{0}, factor{0}, levels{0}; std::uint64_t frames{0}; std::vector<LevelIndex> indexes;
};
WaveformCacheReader::~WaveformCacheReader() = default;
WaveformCacheReader::WaveformCacheReader(WaveformCacheReader&&) noexcept = default;
WaveformCacheReader& WaveformCacheReader::operator=(WaveformCacheReader&&) noexcept = default;
WaveformCacheReader::WaveformCacheReader(std::unique_ptr<Impl> impl) : impl_{std::move(impl)} {}

std::optional<WaveformCacheReader> WaveformCacheReader::open(const std::filesystem::path& path, const WaveformCacheKey& expected_key) {
  try {
    std::ifstream in(path, std::ios::binary); if (!in) return std::nullopt; std::array<char, 8> magic{}; in.read(magic.data(), 8); if (magic != kMagic) return std::nullopt;
    if (read_u32(in) != kSchema || read_u32(in) != kGenerator) return std::nullopt;
    if (read_string(in) != expected_key.stable_text()) return std::nullopt;
    auto impl = std::make_unique<Impl>(); impl->path = path; impl->key = expected_key; impl->sample_rate = static_cast<int>(read_u32(in)); impl->channels = static_cast<int>(read_u32(in)); impl->time_base = domain::Rational{read_i64(in), read_i64(in)};
    const auto origin_value = read_i64(in); const auto origin_num = read_i64(in); const auto origin_den = read_i64(in); if (origin_num != 0) impl->origin = domain::MediaTime{origin_value, domain::Rational{origin_num, origin_den}};
    if (impl->origin != expected_key.stream_origin) return std::nullopt;
    impl->l0 = read_u32(in); impl->factor = read_u32(in); impl->levels = read_u32(in); const auto complete = read_u32(in); impl->frames = read_u64(in); if (complete != 1 || impl->l0 == 0 || impl->factor != 2 || impl->levels == 0 || impl->channels <= 0) return std::nullopt;
    if (impl->sample_rate != expected_key.sample_rate || impl->channels != expected_key.channel_count || impl->time_base != expected_key.time_base) return std::nullopt;
    impl->indexes.resize(impl->levels); const auto file_size = std::filesystem::file_size(path); const auto payload_start = static_cast<std::uint64_t>(in.tellg()) + static_cast<std::uint64_t>(impl->levels) * 16U; std::uint64_t expected_offset = payload_start;
    const auto record_bytes = static_cast<std::uint64_t>(impl->channels) * 4U;
    for (auto& index : impl->indexes) {
      index.offset = read_u64(in); index.count = read_u64(in);
      if (index.offset != expected_offset || index.offset > file_size || index.count > (file_size - index.offset) / record_bytes) return std::nullopt;
      const auto level_bytes = index.count * record_bytes;
      if (level_bytes > std::numeric_limits<std::uint64_t>::max() - expected_offset) return std::nullopt;
      expected_offset += level_bytes;
    }
    if (expected_offset != file_size || impl->indexes.front().count != (impl->frames + impl->l0 - 1) / impl->l0) return std::nullopt;
    for (std::size_t i = 1; i < impl->indexes.size(); ++i) if (impl->indexes[i].count != (impl->indexes[i - 1].count + 1) / 2) return std::nullopt;
    return WaveformCacheReader{std::move(impl)};
  } catch (...) { return std::nullopt; }
}
const WaveformCacheKey& WaveformCacheReader::key() const noexcept { return impl_->key; }
WaveformCacheStatus WaveformCacheReader::status() const noexcept { return impl_ ? WaveformCacheStatus::Ready : WaveformCacheStatus::Invalid; }
std::uint64_t WaveformCacheReader::source_frames() const noexcept { return impl_->frames; }
std::uint32_t WaveformCacheReader::level_count() const noexcept { return impl_->levels; }

WaveformSlice WaveformCacheReader::query(const WaveformQuery& request) const {
  if (!impl_ || request.pixel_width == 0 || request.density <= 0.0 || !std::isfinite(request.density) || request.source_end <= request.source_start) throw std::invalid_argument{"invalid waveform query"};
  WaveformSlice result; result.requested_start = request.source_start; result.requested_end = request.source_end; result.channel_count = impl_->channels; result.completeness = WaveformCompleteness::Complete;
  const auto origin = impl_->origin.value_or(domain::MediaTime::zero());
  std::int64_t first_sample = sample_index(request.source_start, origin, impl_->sample_rate, true); std::int64_t end_sample = sample_index(request.source_end, origin, impl_->sample_rate, true); first_sample = std::max<std::int64_t>(0, first_sample); end_sample = std::min<std::int64_t>(static_cast<std::int64_t>(impl_->frames), std::max(first_sample, end_sample));
  result.available_start = origin; result.available_end = origin + domain::MediaTime{static_cast<std::int64_t>(impl_->frames), domain::Rational{1, impl_->sample_rate}};
  const long double target = (static_cast<long double>(request.source_end.seconds().numerator()) / request.source_end.seconds().denominator() - static_cast<long double>(request.source_start.seconds().numerator()) / request.source_start.seconds().denominator()) * impl_->sample_rate / request.pixel_width / request.density;
  std::uint32_t level = 0; std::uint64_t span = impl_->l0; while (level + 1 < impl_->levels && static_cast<long double>(span * impl_->factor) <= target) { span *= impl_->factor; ++level; }
  result.level = level; result.bucket_sample_span = span; const auto first_bucket = static_cast<std::uint64_t>(first_sample) / span; const auto last_bucket = end_sample == first_sample ? first_bucket : (static_cast<std::uint64_t>(end_sample) + span - 1) / span; const auto count = std::min<std::uint64_t>(last_bucket, impl_->indexes[level].count) > first_bucket ? std::min<std::uint64_t>(last_bucket, impl_->indexes[level].count) - first_bucket : 0;
  result.first_bucket = first_bucket; result.bucket_count = count; result.buckets.resize(static_cast<std::size_t>(count * impl_->channels)); std::ifstream in(impl_->path, std::ios::binary); in.seekg(static_cast<std::streamoff>(impl_->indexes[level].offset + first_bucket * static_cast<std::uint64_t>(impl_->channels) * 4U)); for (auto& bucket : result.buckets) { bucket.minimum = read_i16(in); bucket.maximum = read_i16(in); if (bucket.minimum > bucket.maximum) throw std::runtime_error{"invalid waveform extrema"}; }
  return result;
}

std::filesystem::path waveform_cache_path(const std::filesystem::path& directory, const WaveformCacheKey& key) {
  std::uint64_t hash = 1469598103934665603ULL; for (const auto c : key.stable_text()) { hash ^= static_cast<unsigned char>(c); hash *= 1099511628211ULL; }
  char name[32]{}; std::snprintf(name, sizeof(name), "%016llx.pwfc", static_cast<unsigned long long>(hash)); return directory / name;
}

} // namespace pulsar::application
