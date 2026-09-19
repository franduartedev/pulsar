#include <pulsar/application/waveform/waveform_cache.hpp>
#include <pulsar/application/timeline/timeline_clip_resolver.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <atomic>
#include <optional>
#include <vector>

namespace {
using namespace pulsar;
using application::AudioBlock;
using application::WaveformBucket;
using application::WaveformCacheBuilder;
using application::WaveformCacheKey;
using application::WaveformCacheReader;

class SyntheticSource final : public application::WaveformSource {
public:
  SyntheticSource(int rate, int channels, std::vector<float> samples, std::size_t block)
      : rate_{rate}, channels_{channels}, samples_{std::move(samples)}, block_{block} {}
  application::WaveformSourceInfo info() const override {
    return {rate_, channels_, domain::Rational{1, rate_}, domain::MediaTime::zero()};
  }
  std::optional<AudioBlock> next_block() override {
    if (position_ == samples_.size()) return std::nullopt;
    const auto frames = std::min(block_, (samples_.size() - position_) / static_cast<std::size_t>(channels_));
    std::vector<float> chunk(samples_.begin() + static_cast<std::ptrdiff_t>(position_),
                             samples_.begin() + static_cast<std::ptrdiff_t>(position_ + frames * channels_));
    position_ += frames * static_cast<std::size_t>(channels_);
    return AudioBlock{rate_, channels_, frames, std::move(chunk), domain::Rational{1, rate_}, std::nullopt};
  }
private:
  int rate_; int channels_; std::vector<float> samples_; std::size_t block_; std::size_t position_{0};
};

WaveformCacheKey key() {
  return {"synthetic-source-v1", 2, 48000, 2, domain::Rational{1, 48000}, domain::MediaTime::zero()};
}

void test_precision_and_query() {
  std::vector<float> samples(256 * 5 * 2, 0.0f);
  samples[17 * 2] = 1.0f; samples[256 * 2 + 4 * 2 + 1] = -1.0f;
  for (std::size_t i = 0; i < 256 * 5; ++i) { samples[i * 2] += 0.25f; samples[i * 2 + 1] -= 0.125f; }
  const auto directory = std::filesystem::temp_directory_path() / "pulsar-a7-tests";
  std::filesystem::create_directories(directory);
  const auto path = directory / "precision.pwfc";
  SyntheticSource source{48000, 2, samples, 13};
  const auto report = application::generate_waveform_cache(source, key(), path);
  assert(report.source_frames == 1280); assert(report.peak_input_frames == 13); assert(report.level_count >= 3);
  auto reader = WaveformCacheReader::open(path, key()); assert(reader.has_value());
  const auto query = reader->query({domain::MediaTime{0, domain::Rational{1, 48000}}, domain::MediaTime{1280, domain::Rational{1, 48000}}, 1, 1.0});
  assert(query.channel_count == 2); assert(query.completeness == application::WaveformCompleteness::Complete); assert(!query.buckets.empty());
  bool positive = false, negative = false;
  for (const auto& bucket : query.buckets) { positive |= bucket.maximum == 32767; negative |= bucket.minimum == -32768; }
  assert(positive && negative);
  const auto fine = reader->query({domain::MediaTime{0, domain::Rational{1, 48000}}, domain::MediaTime{1, domain::Rational{1, 48000}}, 100000, 1.0});
  assert(fine.level == 0);
  for (const double density : {0.5, 1.0, 2.0}) {
    const auto density_slice = reader->query({domain::MediaTime{0, domain::Rational{1, 48000}},
                                               domain::MediaTime{1280, domain::Rational{1, 48000}}, 4, density});
    bool impulse_interval_preserved = false;
    for (std::size_t index = 0; index < density_slice.buckets.size(); index += 2) {
      if (density_slice.buckets[index].maximum == 32767) {
        const auto bucket = density_slice.first_bucket + index / 2;
        impulse_interval_preserved = bucket * density_slice.bucket_sample_span <= 17 &&
                                     17 < (bucket + 1) * density_slice.bucket_sample_span;
      }
    }
    assert(impulse_interval_preserved);
  }
  auto wrong = WaveformCacheReader::open(path, WaveformCacheKey{"other", 2, 48000, 2, domain::Rational{1, 48000}, domain::MediaTime::zero()});
  assert(!wrong.has_value());
  std::filesystem::remove_all(directory);
}

void test_streaming_state_is_bounded() {
  const auto directory = std::filesystem::temp_directory_path() / "pulsar-a7-bounded";
  std::filesystem::create_directories(directory);
  const auto path = directory / "bounded.pwfc";
  WaveformCacheBuilder builder{key(), path};
  std::vector<float> block(4096 * 2, 0.0f);
  for (int i = 0; i < 200; ++i) builder.consume(AudioBlock{48000, 2, 4096, block, domain::Rational{1, 48000}, std::nullopt});
  const auto report = builder.finish(); assert(report.source_frames == 819200); assert(report.peak_input_frames == 4096); assert(report.peak_pending_buckets < 64);
  std::filesystem::remove_all(directory);
}

void test_corruption_origin_and_partial() {
  const auto directory = std::filesystem::temp_directory_path() / "pulsar-a7-corruption";
  std::filesystem::create_directories(directory);
  const auto origin = domain::MediaTime{2, domain::Rational{1, 1}};
  const auto origin_key = WaveformCacheKey{"origin", 4, 1000, 1, domain::Rational{1, 1000}, origin};
  WaveformCacheBuilder builder{origin_key, directory / "origin.pwfc"};
  std::vector<float> samples(300, 0.0f); samples[4] = 1.0f;
  builder.consume(AudioBlock{1000, 1, 300, samples, domain::Rational{1, 1000}, std::nullopt});
  const auto partial = builder.query_partial({domain::MediaTime{0, domain::Rational{1, 1}}, domain::MediaTime{2, domain::Rational{1, 1}}, 100, 1.0});
  assert(partial.completeness == application::WaveformCompleteness::Partial);
  assert(partial.buckets.empty());
  const auto finished = builder.finish();
  assert(finished.source_frames == 300);
  auto reader = WaveformCacheReader::open(directory / "origin.pwfc", origin_key); assert(reader.has_value());
  const auto before = reader->query({domain::MediaTime{0, domain::Rational{1, 1}}, domain::MediaTime{2, domain::Rational{1, 1}}, 10, 1.0});
  assert(before.buckets.empty());
  const auto corrupted = directory / "corrupt.pwfc";
  std::filesystem::copy_file(directory / "origin.pwfc", corrupted, std::filesystem::copy_options::overwrite_existing);
  {
    std::fstream mutate(corrupted, std::ios::binary | std::ios::in | std::ios::out);
    mutate.seekg(-1, std::ios::end); const int value = mutate.get(); mutate.seekp(-1, std::ios::end); mutate.put(static_cast<char>(value ^ 0x01));
  }
  assert(!WaveformCacheReader::open(corrupted, origin_key).has_value());
  std::filesystem::copy_file(directory / "origin.pwfc", corrupted, std::filesystem::copy_options::overwrite_existing);
  std::filesystem::resize_file(corrupted, 7);
  assert(!WaveformCacheReader::open(corrupted, origin_key).has_value());
  std::filesystem::remove_all(directory);
}

void test_cancellation_and_transaction() {
  const auto directory = std::filesystem::temp_directory_path() / "pulsar-a7-transaction";
  std::filesystem::create_directories(directory);
  const auto path = directory / "safe.pwfc";
  const auto cache_key = WaveformCacheKey{"transaction", 0, 1000, 1, domain::Rational{1, 1000}, domain::MediaTime::zero()};
  SyntheticSource good{1000, 1, std::vector<float>(1000, 0.25f), 100};
  const auto good_report = application::generate_waveform_cache(good, cache_key, path);
  assert(!good_report.cancelled);
  const auto old_size = std::filesystem::file_size(path);
  bool threw = false;
  try {
    SyntheticSource bad{1000, 1, std::vector<float>{std::numeric_limits<float>::quiet_NaN()}, 1};
    const auto bad_report = application::generate_waveform_cache(bad, cache_key, path);
    assert(!bad_report.cancelled);
  } catch (const std::invalid_argument&) { threw = true; }
  assert(threw); assert(std::filesystem::file_size(path) == old_size);
  WaveformCacheBuilder cancelled{cache_key, directory / "cancelled.pwfc"};
  const auto report = [&] {
    AudioBlock block{1000, 1, 100, std::vector<float>(100, 0.0f), domain::Rational{1, 1000}, std::nullopt};
    cancelled.consume(block, [] { return true; });
    return cancelled.report();
  }();
  assert(report.cancelled); assert(!std::filesystem::exists(directory / "cancelled.pwfc"));
  std::filesystem::remove_all(directory);
}

void test_source_identity_changes() {
  const auto directory = std::filesystem::temp_directory_path() / "pulsar-a7-identity";
  std::filesystem::create_directories(directory);
  const auto source_path = directory / "source.bin";
  { std::ofstream out(source_path, std::ios::binary); out << "one"; }
  const auto first = application::make_waveform_cache_key(source_path, 1, 48000, 2, domain::Rational{1, 48000}, domain::MediaTime::zero());
  { std::ofstream out(source_path, std::ios::binary | std::ios::app); out << "changed"; }
  const auto second = application::make_waveform_cache_key(source_path, 1, 48000, 2, domain::Rational{1, 48000}, domain::MediaTime::zero());
  assert(!(first == second));
  std::filesystem::remove_all(directory);
}

void test_manager_coalesces_and_reuses() {
  const auto directory = std::filesystem::temp_directory_path() / "pulsar-a7-manager";
  std::filesystem::create_directories(directory);
  const auto cache_key = WaveformCacheKey{"manager-source", 0, 1000, 1, domain::Rational{1, 1000}, domain::MediaTime::zero()};
  std::atomic<int> factory_calls{0};
  application::WaveformCacheManager manager{1};
  const auto factory = [&] {
    ++factory_calls;
    return std::make_unique<SyntheticSource>(1000, 1, std::vector<float>(1000, 0.5f), 100);
  };
  auto first = manager.request(directory, cache_key, factory, 1000);
  auto second = manager.request(directory, cache_key, factory, 1000);
  assert(first->wait_for(std::chrono::seconds{5})); assert(second->wait_for(std::chrono::seconds{5}));
  assert(first->progress().status == application::WaveformCacheStatus::Ready);
  assert(second->progress().status == application::WaveformCacheStatus::Ready);
  assert(factory_calls.load() == 1);
  const auto slice = first->query_partial({domain::MediaTime::zero(), domain::MediaTime{1, domain::Rational{1, 1}}, 32, 1.0});
  assert(slice.has_value() && !slice->buckets.empty());
  manager.shutdown();
  application::WaveformCacheManager reuse_manager{1};
  auto reused = reuse_manager.request(directory, cache_key, [&] { ++factory_calls; return std::unique_ptr<application::WaveformSource>{}; });
  assert(reused->wait_for(std::chrono::seconds{1})); assert(reused->progress().status == application::WaveformCacheStatus::Ready); assert(factory_calls.load() == 1);
  reuse_manager.shutdown();
  std::filesystem::remove_all(directory);
}

void test_a5_source_windows_reuse_asset_cache() {
  using domain::MediaId; using domain::MediaTime; using domain::Rational; using domain::TimeRange;
  const auto t = [](std::int64_t seconds) { return MediaTime{seconds, Rational{1, 1}}; };
  const auto make_clip = [&](MediaTime timeline_start, MediaTime source_start, MediaTime duration) {
    application::ResolvedClip clip{domain::ClipId{1}, MediaId{9}, timeline_start,
      TimeRange{timeline_start, timeline_start + duration}, MediaTime::zero(),
      TimeRange{source_start, source_start + duration}, MediaTime::zero(), 0.0, false, 0, {}, std::nullopt};
    return clip;
  };
  const auto cache_key = WaveformCacheKey{"asset-file|size=1|mtime=1", 2, 48000, 2, Rational{1, 48000}, t(0)};
  const auto original = make_clip(t(10), t(100), t(10));
  const auto split = t(104);
  const auto left = make_clip(t(10), t(100), t(4));
  const auto right = make_clip(split, split, t(6));
  assert(application::clip_source_position(left, t(13)) == t(103));
  assert(application::clip_source_position(right, t(104)) == t(104));
  assert(left.source_range.end() == right.source_range.start());
  const auto left_trim = make_clip(t(10), t(103), t(7));
  assert(application::clip_source_position(left_trim, t(10)) == t(103));
  const auto right_trim = make_clip(t(10), t(100), t(5));
  assert(right_trim.source_range.start() == original.source_range.start());
  assert(right_trim.source_range.end() == t(105));
  const auto moved = make_clip(t(30), t(100), t(10));
  assert(moved.source_range.start() == original.source_range.start());
  assert(moved.source_range.end() == original.source_range.end());
  assert(cache_key == cache_key);
}

} // namespace

int main() {
  test_precision_and_query();
  test_streaming_state_is_bounded();
  test_corruption_origin_and_partial();
  test_cancellation_and_transaction();
  test_source_identity_changes();
  test_manager_coalesces_and_reuses();
  test_a5_source_windows_reuse_asset_cache();
  std::cout << "waveform cache tests passed\n";
}
