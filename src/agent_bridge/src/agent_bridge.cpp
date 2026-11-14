#include <pulsar/agent_bridge/agent_bridge.hpp>

#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/application/project/project_codec.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/project_file/file_project_repository.hpp>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace pulsar::agent_bridge {
namespace {

using application::CommandProcessor;
using application::MediaProbe;
using application::MediaProbeResult;
using application::ProjectCodec;
using application::ProjectDocument;
using application::ProjectRestoreState;
using domain::MediaTime;
using domain::Rational;
using domain::TimeRange;

struct Object {
  std::map<std::string, std::string, std::less<>> fields;
};

class Parser {
public:
  explicit Parser(std::string_view input) : input_{input} {}

  Object object() {
    skip(); expect('{'); Object result; skip();
    if (take('}')) return result;
    for (;;) {
      const auto key = string(); skip(); expect(':'); skip();
      result.fields.emplace(key, value()); skip();
      if (take('}')) return result;
      expect(','); skip();
    }
  }

private:
  void skip() { while (pos_ < input_.size() && input_[pos_] <= ' ') ++pos_; }
  void expect(char c) { if (pos_ >= input_.size() || input_[pos_++] != c) throw std::runtime_error{"invalid JSON"}; }
  bool take(char c) { if (pos_ < input_.size() && input_[pos_] == c) { ++pos_; return true; } return false; }
  std::string string() {
    expect('"'); std::string result;
    while (pos_ < input_.size()) {
      const char c = input_[pos_++];
      if (c == '"') return result;
      if (c == '\\') {
        if (pos_ >= input_.size()) throw std::runtime_error{"invalid JSON string"};
        const char escaped = input_[pos_++];
        if (escaped == '"' || escaped == '\\' || escaped == '/') result.push_back(escaped);
        else if (escaped == 'n') result.push_back('\n');
        else if (escaped == 'r') result.push_back('\r');
        else if (escaped == 't') result.push_back('\t');
        else throw std::runtime_error{"unsupported JSON escape"};
      } else result.push_back(c);
    }
    throw std::runtime_error{"unterminated JSON string"};
  }
  std::string value() {
    if (pos_ < input_.size() && input_[pos_] == '"') return string();
    const auto begin = pos_;
    while (pos_ < input_.size() && input_[pos_] != ',' && input_[pos_] != '}' && input_[pos_] > ' ') ++pos_;
    if (begin == pos_) throw std::runtime_error{"missing JSON value"};
    return std::string{input_.substr(begin, pos_ - begin)};
  }
  std::string_view input_; std::size_t pos_{0};
};

std::string json_escape(std::string_view value) {
  std::string result{"\""};
  for (const char c : value) {
    if (c == '"' || c == '\\') { result.push_back('\\'); result.push_back(c); }
    else if (c == '\n') result += "\\n";
    else if (c == '\r') result += "\\r";
    else result.push_back(c);
  }
  result.push_back('"'); return result;
}

std::string response_ok(std::string_view command, std::string_view extra = {}) {
  return "{\"ok\":true,\"command\":" + json_escape(command) +
         (extra.empty() ? "}" : "," + std::string{extra} + "}");
}

class TestProbe final : public MediaProbe {
public:
  TestProbe(double duration, bool audio) : duration_{duration}, audio_{audio} {}
  MediaProbeResult probe(const std::filesystem::path& path) const override {
    const auto ticks = static_cast<std::int64_t>(duration_ * 1000.0 + 0.5);
    MediaProbeResult result{path, std::string{"agent-test"}, MediaTime{ticks, Rational{1, 1000}}, {}, {}};
    result.video_streams.push_back({0, "agent-test-video", 640, 360,
                                    Rational{1, 1000}, result.duration,
                                    domain::FrameRate{30, 1}});
    if (audio_) result.audio_streams.push_back({1, "agent-test-audio", 48000, 2,
                                                Rational{1, 48000}, result.duration});
    return result;
  }
private:
  double duration_; bool audio_;
};

std::uint64_t integer(const Object& object, std::string_view key) {
  const auto it = object.fields.find(key);
  if (it == object.fields.end()) throw std::runtime_error{"missing field: " + std::string{key}};
  std::uint64_t result{};
  const auto [end, error] = std::from_chars(it->second.data(), it->second.data() + it->second.size(), result);
  if (error != std::errc{} || end != it->second.data() + it->second.size()) throw std::runtime_error{"invalid integer: " + std::string{key}};
  return result;
}

double real(const Object& object, std::string_view key, double fallback = 0.0) {
  const auto it = object.fields.find(key);
  if (it == object.fields.end()) return fallback;
  std::size_t used{}; const auto result = std::stod(it->second, &used);
  if (used != it->second.size()) throw std::runtime_error{"invalid number: " + std::string{key}};
  return result;
}

bool boolean(const Object& object, std::string_view key, bool fallback = false) {
  const auto it = object.fields.find(key);
  if (it == object.fields.end()) return fallback;
  if (it->second == "true") return true;
  if (it->second == "false") return false;
  throw std::runtime_error{"invalid boolean: " + std::string{key}};
}

MediaTime seconds(double value) {
  return MediaTime{static_cast<std::int64_t>(value * 1000.0 + (value >= 0 ? 0.5 : -0.5)), Rational{1, 1000}};
}

std::string time_json(MediaTime value) {
  std::ostringstream out; out << std::fixed << std::setprecision(6) << static_cast<double>(value);
  return out.str();
}

} // namespace

class AgentBridge::State {
public:
  std::unique_ptr<ProjectDocument> document;
  std::unique_ptr<CommandProcessor> commands;
  std::uint64_t imported_media{0};
  std::optional<domain::TrackId> audio_track;
};

AgentBridge::AgentBridge(std::filesystem::path authorized_root)
    : authorized_root_{std::filesystem::absolute(std::move(authorized_root)).lexically_normal()},
      state_{std::make_unique<State>()} {
  std::filesystem::create_directories(authorized_root_);
  authorized_root_ = std::filesystem::weakly_canonical(authorized_root_);
}

AgentBridge::~AgentBridge() = default;

std::string AgentBridge::execute(const std::string& request) {
  try {
    const auto object = Parser{request}.object();
    const auto command_it = object.fields.find("command");
    if (command_it == object.fields.end()) throw std::runtime_error{"missing field: command"};
    const std::string& command = command_it->second;
    const auto safe_path = [this](std::string_view text) {
      const std::filesystem::path relative{std::string{text}};
      if (relative.empty() || relative.is_absolute()) throw std::runtime_error{"path must be relative to authorized root"};
      // weakly_canonical resolves existing symlink components while still
      // allowing a new file below the authorized directory.
      const auto path = std::filesystem::weakly_canonical(authorized_root_ / relative);
      const auto root_text = authorized_root_.generic_string();
      const auto path_text = path.generic_string();
      const auto prefix = root_text.ends_with('/') ? root_text : root_text + "/";
      if (path_text != root_text && !path_text.starts_with(prefix)) throw std::runtime_error{"path escapes authorized root"};
      return path;
    };
    const auto field = [&object](std::string_view key) -> std::string {
      const auto it = object.fields.find(key);
      if (it == object.fields.end()) throw std::runtime_error{"missing field: " + std::string{key}};
      return it->second;
    };

    if (command == "create_project") {
      const auto path = safe_path(field("name"));
      if (std::filesystem::exists(path)) throw std::runtime_error{"project already exists"};
      state_ = std::make_unique<State>(); state_->document = std::make_unique<ProjectDocument>();
      state_->commands = std::make_unique<CommandProcessor>(*state_->document); active_project_ = path;
      return response_ok(command, "\"path\":" + json_escape(path.generic_string()));
    }
    if (command == "create_test_media") {
      const auto path = safe_path(field("path"));
      if (std::filesystem::exists(path)) throw std::runtime_error{"media already exists"};
      std::ofstream output{path, std::ios::binary}; if (!output) throw std::runtime_error{"cannot create test media"};
      output << "PULSAR_AGENT_TEST_MEDIA\n";
      return response_ok(command, "\"path\":" + json_escape(path.generic_string()));
    }
    if (command == "open_project") {
      const auto path = safe_path(field("path"));
      auto restored = project_file::FileProjectRepository{}.load(path);
      auto document = std::make_unique<ProjectDocument>(ProjectDocument::restore(restored));
      state_ = std::make_unique<State>(); state_->document = std::move(document);
      state_->commands = std::make_unique<CommandProcessor>(*state_->document); active_project_ = path;
      return response_ok(command, "\"path\":" + json_escape(path.generic_string()));
    }
    if (command == "close_project") { state_ = std::make_unique<State>(); active_project_.clear(); return response_ok(command); }
    if (!state_->document || !state_->commands) throw std::runtime_error{"no project is open"};
    if (command == "import_media" || command == "import_test_media") {
      const auto path = safe_path(field("path"));
      if (!std::filesystem::is_regular_file(path)) throw std::runtime_error{"media file does not exist"};
      const auto id = state_->commands->import_asset(path, TestProbe{real(object, "duration", 30.0), boolean(object, "audio", true)});
      ++state_->imported_media;
      return response_ok(command, "\"media_id\":" + std::to_string(id.value()));
    }
    if (command == "add_clip") {
      const auto media = domain::MediaId{integer(object, "media_id")};
      const auto start = real(object, "timeline_start");
      const auto source_start = real(object, "source_start");
      const auto source_end = real(object, "source_end");
      const auto track = object.fields.find("track");
      domain::ClipId id{1};
      if (track != object.fields.end() && track->second == "audio") {
        if (!state_->audio_track.has_value()) {
          state_->audio_track = state_->commands->add_track(domain::TrackKind::Audio);
        }
        id = state_->commands->add_clip(*state_->audio_track, media,
                                        TimeRange{seconds(source_start), seconds(source_end)},
                                        seconds(start));
      } else {
        id = state_->commands->add_clip(media,
                                        TimeRange{seconds(source_start), seconds(source_end)},
                                        seconds(start));
      }
      return response_ok(command, "\"clip_id\":" + std::to_string(id.value()));
    }
    if (command == "split_clip") {
      const auto id = state_->commands->split_clip(domain::ClipId{integer(object, "clip_id")}, seconds(real(object, "position")));
      return response_ok(command, "\"clip_id\":" + std::to_string(id.value()));
    }
    if (command == "timeline_state") {
      std::ostringstream out; out << "\"tracks\":["; bool first_track = true;
      for (const auto& track : state_->document->timeline().tracks()) {
        if (!first_track) out << ','; first_track = false;
        out << "{\"id\":" << track.id().value() << ",\"clips\":["; bool first_clip = true;
        for (const auto& clip : track.clips()) {
          if (!first_clip) out << ','; first_clip = false;
          out << "{\"id\":" << clip.id().value() << ",\"media_id\":" << clip.media_id().value()
              << ",\"start\":" << time_json(clip.timeline_range().start())
              << ",\"end\":" << time_json(clip.timeline_range().end()) << '}';
        }
        out << "]}";
      }
      out << "]"; return response_ok(command, out.str());
    }
    if (command == "save_project") {
      const auto path = safe_path(field("path"));
      if (std::filesystem::exists(path) && path != active_project_) {
        throw std::runtime_error{"refusing to overwrite a project outside the active session"};
      }
      const auto result = project_file::FileProjectRepository{}.save(path, state_->document->snapshot());
      state_->document->mark_history_state_saved(state_->document->history_state_id()); active_project_ = path;
      return response_ok(command, "\"durable\":" + std::string{result.status == application::ProjectSaveStatus::Durable ? "true" : "false"});
    }
    if (command == "validate_round_trip") {
      if (active_project_.empty()) throw std::runtime_error{"project has not been saved"};
      const auto before = ProjectCodec::encode(state_->document->snapshot());
      auto restored = project_file::FileProjectRepository{}.load(active_project_);
      const auto after = ProjectCodec::encode(ProjectDocument::restore(restored).snapshot());
      if (before != after) throw std::runtime_error{"round-trip timeline mismatch"};
      return response_ok(command, "\"equal\":true");
    }
    throw std::runtime_error{"unknown command: " + command};
  } catch (const std::exception& error) {
    return "{\"ok\":false,\"error\":\"bridge_error\",\"message\":" + json_escape(error.what()) + "}";
  }
}

} // namespace pulsar::agent_bridge
