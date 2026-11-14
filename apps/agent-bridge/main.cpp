#include <pulsar/agent_bridge/agent_bridge.hpp>

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc != 3 || std::string{argv[1]} != "--root") {
    std::cerr << "usage: pulsar-agent-bridge --root <authorized-directory>\n";
    return 2;
  }
  pulsar::agent_bridge::AgentBridge bridge{argv[2]};
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) continue;
    std::cout << bridge.execute(line) << '\n' << std::flush;
  }
  return 0;
}
