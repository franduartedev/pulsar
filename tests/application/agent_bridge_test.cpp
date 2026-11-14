#include <pulsar/agent_bridge/agent_bridge.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("pulsar-agent-bridge-" + std::to_string(
                        std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  pulsar::agent_bridge::AgentBridge bridge{root};

  auto run = [&bridge](const std::string& command) {
    const auto result = bridge.execute(command);
    assert(result.find("\"ok\":true") != std::string::npos);
    return result;
  };

  run(R"({"command":"create_project","name":"round-trip.pulsar"})");
  run(R"({"command":"create_test_media","path":"fixture.test"})");
  const auto imported = run(R"({"command":"import_test_media","path":"fixture.test","duration":10,"audio":true})");
  assert(imported.find("media_id") != std::string::npos);
  run(R"({"command":"add_clip","media_id":1,"source_start":0,"source_end":10,"timeline_start":0})");
  run(R"({"command":"split_clip","clip_id":1,"position":4})");
  const auto before = run(R"({"command":"timeline_state"})");
  assert(before.find("\"start\":0.000000") != std::string::npos);
  assert(before.find("\"end\":4.000000") != std::string::npos);
  assert(before.find("\"start\":4.000000") != std::string::npos);
  run(R"({"command":"save_project","path":"round-trip.pulsar"})");
  run(R"({"command":"validate_round_trip"})");
  run(R"({"command":"close_project"})");
  run(R"({"command":"open_project","path":"round-trip.pulsar"})");
  const auto after = run(R"({"command":"timeline_state"})");
  assert(after == before);
  run(R"({"command":"validate_round_trip"})");

  std::filesystem::remove_all(root);
  return 0;
}
