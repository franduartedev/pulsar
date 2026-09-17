#pragma once

#include <pulsar/application/playback/playback_time.hpp>

namespace pulsar::application {

class MonotonicClock {
public:
  virtual ~MonotonicClock() = default;

  [[nodiscard]] virtual MonotonicInstant now() const = 0;
};

class SteadyMonotonicClock final : public MonotonicClock {
public:
  [[nodiscard]] MonotonicInstant now() const override;
};

} // namespace pulsar::application
