#include <pulsar/application/bootstrap.hpp>
#include <pulsar/domain/time/frame_number.hpp>
#include <pulsar/domain/time/frame_rate.hpp>
#include <pulsar/domain/time/media_time.hpp>

#include <iostream>

int main() {
  const auto rate = pulsar::domain::FrameRate{30000, 1001};
  const auto frame = pulsar::domain::FrameNumber{30000};
  const auto time = pulsar::domain::MediaTime::from_frame(frame, rate);
  const auto seconds = time.seconds();

  std::cout << "PULSAR Engine Lab\n";
  std::cout << "Foundation build OK\n";
  std::cout << "Time model\n";
  std::cout << "Rate: " << rate.rational().numerator() << '/' << rate.rational().denominator()
            << " fps\n";
  std::cout << "Frame: " << frame.value() << '\n';
  std::cout << "Time: " << seconds.numerator();
  if (seconds.denominator() != 1) {
    std::cout << '/' << seconds.denominator();
  }
  std::cout << " s\n";
  return 0;
}
