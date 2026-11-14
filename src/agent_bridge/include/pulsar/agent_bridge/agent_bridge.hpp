#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace pulsar::agent_bridge {

// Local, line-oriented JSON command bridge. It deliberately owns only an
// application document; media decoding and UI playback remain outside the MVP.
class AgentBridge final {
public:
  explicit AgentBridge(std::filesystem::path authorized_root);
  ~AgentBridge();

  // Executes one JSON object and returns one JSON object. No command can
  // address a path outside authorized_root or invoke the operating system.
  [[nodiscard]] std::string execute(const std::string& request);

private:
  std::filesystem::path authorized_root_;
  std::filesystem::path active_project_;
  class State;
  std::unique_ptr<State> state_;
};

} // namespace pulsar::agent_bridge
