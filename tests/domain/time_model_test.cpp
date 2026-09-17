#include <pulsar/domain/time/frame_number.hpp>
#include <pulsar/domain/time/frame_rate.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>

#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

using pulsar::domain::FrameNumber;
using pulsar::domain::FrameRate;
using pulsar::domain::MediaTime;
using pulsar::domain::Rational;
using pulsar::domain::RoundingMode;
using pulsar::domain::frame_at;

int fail(std::string_view message) {
  std::cerr << "time model test failed: " << message << '\n';
  return 1;
}

template <typename Exception, typename Callable>
bool throws(Callable callable) {
  try {
    callable();
  } catch (const Exception&) {
    return true;
  } catch (...) {
    return false;
  }

  return false;
}

bool rational_tests() {
  constexpr auto int64_min = std::numeric_limits<std::int64_t>::min();
  constexpr auto int64_max = std::numeric_limits<std::int64_t>::max();

  if (Rational{1, 2} != Rational{2, 4}) {
    return false;
  }
  if (Rational{2, 4}.numerator() != 1 || Rational{2, 4}.denominator() != 2) {
    return false;
  }
  if (Rational{1, -2} != Rational{-1, 2}) {
    return false;
  }
  if (Rational{-1, -2} != Rational{1, 2}) {
    return false;
  }
  if (Rational{0, 20} != Rational{0, 1}) {
    return false;
  }
  if (Rational{0, -5} != Rational{0, 1}) {
    return false;
  }
  if (Rational{0, int64_max} != Rational{0, 1}) {
    return false;
  }
  if (Rational{0, int64_min} != Rational{0, 1}) {
    return false;
  }
  if (!throws<std::invalid_argument>([] { (void)Rational{1, 0}; })) {
    return false;
  }
  if (!(Rational{1, 3} < Rational{1, 2}) || !(Rational{3, 2} > Rational{4, 3})) {
    return false;
  }
  if (Rational{9'000'000'000LL, 3'000'000'000LL} != Rational{3, 1}) {
    return false;
  }
  if (Rational{int64_min, 1}.numerator() != int64_min ||
      Rational{int64_min, 1}.denominator() != 1) {
    return false;
  }
  if (Rational{int64_min, 2} != Rational{int64_min / 2, 1}) {
    return false;
  }
  if (!throws<std::overflow_error>([] { (void)Rational{1, int64_min}; })) {
    return false;
  }
  if (!throws<std::overflow_error>([] { (void)Rational{int64_min, -1}; })) {
    return false;
  }
  if (!(Rational{int64_max, int64_max - 1} > Rational{int64_max - 1, int64_max})) {
    return false;
  }

  return true;
}

bool frame_rate_tests() {
  (void)FrameRate{24, 1};
  (void)FrameRate{25, 1};
  (void)FrameRate{30000, 1001};
  (void)FrameRate{60000, 1001};

  if (!throws<std::invalid_argument>([] { (void)FrameRate{0, 1}; })) {
    return false;
  }
  if (!throws<std::invalid_argument>([] { (void)FrameRate{-24, 1}; })) {
    return false;
  }

  return true;
}

bool frame_number_tests() {
  if (FrameNumber{0}.value() != 0) {
    return false;
  }
  if (FrameNumber{25} != FrameNumber{25}) {
    return false;
  }
  if (!(FrameNumber{25} > FrameNumber{1})) {
    return false;
  }
  if (!(FrameNumber{-1} < FrameNumber{0})) {
    return false;
  }

  return true;
}

bool frame_to_time_tests() {
  if (MediaTime::from_frame(FrameNumber{0}, FrameRate{24, 1}).seconds() != Rational{0, 1}) {
    return false;
  }
  if (MediaTime::from_frame(FrameNumber{24}, FrameRate{24, 1}).seconds() != Rational{1, 1}) {
    return false;
  }
  if (MediaTime::from_frame(FrameNumber{25}, FrameRate{25, 1}).seconds() != Rational{1, 1}) {
    return false;
  }
  if (MediaTime::from_frame(FrameNumber{30}, FrameRate{30, 1}).seconds() != Rational{1, 1}) {
    return false;
  }
  if (MediaTime::from_frame(FrameNumber{30000}, FrameRate{30000, 1001}).seconds() !=
      Rational{1001, 1}) {
    return false;
  }
  if (MediaTime::from_frame(FrameNumber{24000}, FrameRate{24000, 1001}).seconds() !=
      Rational{1001, 1}) {
    return false;
  }

  return true;
}

bool media_time_invariant_tests() {
  (void)MediaTime{1, Rational{1, 1}};
  (void)MediaTime{1, Rational{1, 1000}};
  (void)MediaTime{1, Rational{1001, 30000}};
  (void)MediaTime{-5, Rational{1, 1000}};

  if (!throws<std::invalid_argument>([] { (void)MediaTime{1, Rational{0, 1}}; })) {
    return false;
  }
  if (!throws<std::invalid_argument>([] { (void)MediaTime{5, Rational{-1, 1000}}; })) {
    return false;
  }

  return true;
}

bool time_to_frame_tests() {
  constexpr auto int64_max = std::numeric_limits<std::int64_t>::max();

  if (frame_at(MediaTime{1, Rational{1, 1}}, FrameRate{24, 1}, RoundingMode::Floor) !=
      FrameNumber{24}) {
    return false;
  }
  if (frame_at(MediaTime{1, Rational{1, 2}}, FrameRate{24, 1}, RoundingMode::Floor) !=
      FrameNumber{12}) {
    return false;
  }
  if (frame_at(MediaTime{1, Rational{1, 3}}, FrameRate{24, 1}, RoundingMode::Floor) !=
      FrameNumber{8}) {
    return false;
  }
  if (frame_at(MediaTime{1, Rational{1, 3}}, FrameRate{24, 1}, RoundingMode::Ceil) !=
      FrameNumber{8}) {
    return false;
  }
  if (frame_at(MediaTime{1, Rational{1, 5}}, FrameRate{24, 1}, RoundingMode::Floor) !=
      FrameNumber{4}) {
    return false;
  }
  if (frame_at(MediaTime{1, Rational{1, 5}}, FrameRate{24, 1}, RoundingMode::Ceil) !=
      FrameNumber{5}) {
    return false;
  }
  if (frame_at(MediaTime{1, Rational{3, 16}}, FrameRate{24, 1}, RoundingMode::Nearest) !=
      FrameNumber{5}) {
    return false;
  }
  if (frame_at(MediaTime{-1, Rational{3, 16}}, FrameRate{24, 1}, RoundingMode::Nearest) !=
      FrameNumber{-5}) {
    return false;
  }
  if (frame_at(MediaTime{-5, Rational{1, 16}}, FrameRate{4, 1}, RoundingMode::Floor) !=
      FrameNumber{-2}) {
    return false;
  }
  if (frame_at(MediaTime{-5, Rational{1, 16}}, FrameRate{4, 1}, RoundingMode::Ceil) !=
      FrameNumber{-1}) {
    return false;
  }
  if (frame_at(MediaTime{-5, Rational{1, 16}}, FrameRate{4, 1}, RoundingMode::Nearest) !=
      FrameNumber{-1}) {
    return false;
  }
  if (frame_at(MediaTime{-3, Rational{1, 8}}, FrameRate{4, 1}, RoundingMode::Nearest) !=
      FrameNumber{-2}) {
    return false;
  }
  if (!throws<std::overflow_error>([] {
        (void)frame_at(MediaTime{std::numeric_limits<std::int64_t>::max(),
                                 Rational{std::numeric_limits<std::int64_t>::max(), 1}},
                       FrameRate{std::numeric_limits<std::int64_t>::max(), 1},
                       RoundingMode::Floor);
      })) {
    return false;
  }
  if (!throws<std::overflow_error>([int64_max] {
        (void)frame_at(MediaTime{int64_max, Rational{1, 1}}, FrameRate{2, 1},
                       RoundingMode::Floor);
      })) {
    return false;
  }

  return true;
}

bool comparison_tests() {
  constexpr auto int64_max = std::numeric_limits<std::int64_t>::max();
  constexpr auto int64_min = std::numeric_limits<std::int64_t>::min();

  if (MediaTime{1, Rational{1, 2}} != MediaTime{500, Rational{1, 1000}}) {
    return false;
  }
  if (!(MediaTime{1, Rational{1, 3}} < MediaTime{1, Rational{1, 2}})) {
    return false;
  }
  if (!(MediaTime{2000, Rational{1, 1000}} > MediaTime{1, Rational{1, 1}})) {
    return false;
  }

  const MediaTime huge{int64_max, Rational{2, 1}};
  const MediaTime huge_nearby{int64_max - 1, Rational{2, 1}};
  if (!(huge > huge_nearby) || huge == huge_nearby || huge < huge_nearby) {
    return false;
  }

  const MediaTime huge_cross_base{int64_max, Rational{int64_max, 1}};
  const MediaTime huge_cross_base_smaller{int64_max - 1, Rational{int64_max, 1}};
  if (!(huge_cross_base > huge_cross_base_smaller)) {
    return false;
  }

  const MediaTime huge_negative{int64_min, Rational{int64_max, 1}};
  const MediaTime less_negative{int64_min + 1, Rational{int64_max, 1}};
  if (!(huge_negative < less_negative)) {
    return false;
  }

  if (!(MediaTime{-1, Rational{1, 1}} < MediaTime{0, Rational{1, 1}}) ||
      !(MediaTime{0, Rational{1, 1}} < MediaTime{1, Rational{1, 1}})) {
    return false;
  }

  const MediaTime equal_a{int64_max / 2, Rational{2, 1}};
  const MediaTime equal_b{int64_max - 1, Rational{1, 1}};
  if (!(equal_a == equal_b) || equal_a < equal_b || equal_b < equal_a ||
      equal_a != equal_b || !(equal_a <= equal_b) || !(equal_a >= equal_b)) {
    return false;
  }

  const MediaTime awkward_a{1001, Rational{1001, 30000}};
  const MediaTime awkward_b{1002001, Rational{1, 30000}};
  if (!(awkward_a == awkward_b)) {
    return false;
  }

  const MediaTime transitive_a{int64_max - 2, Rational{int64_max, 1}};
  const MediaTime transitive_b{int64_max - 1, Rational{int64_max, 1}};
  const MediaTime transitive_c{int64_max, Rational{int64_max, 1}};
  if (!(transitive_a < transitive_b) || !(transitive_b < transitive_c) ||
      !(transitive_a < transitive_c)) {
    return false;
  }

  return true;
}

bool round_trip_tests() {
  const FrameRate rates[] = {FrameRate{24, 1},       FrameRate{25, 1},
                             FrameRate{30, 1},       FrameRate{24000, 1001},
                             FrameRate{30000, 1001}, FrameRate{60000, 1001}};
  const FrameNumber frames[] = {FrameNumber{0}, FrameNumber{1}, FrameNumber{123},
                                FrameNumber{30000}};

  for (const auto rate : rates) {
    for (const auto frame : frames) {
      const auto time = MediaTime::from_frame(frame, rate);
      if (frame_at(time, rate, RoundingMode::Floor) != frame) {
        return false;
      }
    }
  }

  return true;
}

} // namespace

int main() {
  if (!rational_tests()) {
    return fail("rational tests");
  }
  if (!frame_rate_tests()) {
    return fail("frame-rate tests");
  }
  if (!frame_number_tests()) {
    return fail("frame-number tests");
  }
  if (!frame_to_time_tests()) {
    return fail("frame-to-time tests");
  }
  if (!media_time_invariant_tests()) {
    return fail("media-time invariant tests");
  }
  if (!time_to_frame_tests()) {
    return fail("time-to-frame tests");
  }
  if (!comparison_tests()) {
    return fail("comparison tests");
  }
  if (!round_trip_tests()) {
    return fail("round-trip tests");
  }

  return 0;
}
