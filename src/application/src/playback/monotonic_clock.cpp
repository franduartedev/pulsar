#include <pulsar/application/playback/monotonic_clock.hpp>

#include <chrono>

namespace pulsar::application {

MonotonicInstant SteadyMonotonicClock::now() const {
  const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
  const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
  return MonotonicInstant{domain::MediaTime{nanoseconds, domain::Rational{1, 1'000'000'000}}};
}

} // namespace pulsar::application
