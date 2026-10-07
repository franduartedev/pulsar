#include <pulsar/project_file/file_project_repository.hpp>

#include <pulsar/application/project/project_codec.hpp>
#include <pulsar/project_file/runtime_resources.hpp>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace pulsar::project_file {
namespace {

constexpr std::size_t max_project_bytes = 128 * 1024 * 1024;
constexpr bool project_size_allowed(std::size_t size) noexcept {
  return size <= max_project_bytes;
}
static_assert(project_size_allowed(max_project_bytes));
static_assert(!project_size_allowed(max_project_bytes + 1));
std::atomic<std::uint64_t> temp_counter{0};

[[noreturn]] void fail(const std::string& message) {
  throw application::ProjectRepositoryError{message + ": " + std::generic_category().message(errno)};
}

std::filesystem::path temp_path_for(const std::filesystem::path& destination, int attempt) {
  const auto parent = destination.parent_path().empty() ? std::filesystem::path{"."}
                                                         : destination.parent_path();
  const auto stamp = static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const auto sequence = temp_counter.fetch_add(1, std::memory_order_relaxed);
  return parent / (destination.filename().string() + ".tmp-" + std::to_string(stamp) + "-" +
                   std::to_string(sequence) + "-" + std::to_string(attempt));
}

std::string read_all(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary | std::ios::ate};
  if (!input) throw application::ProjectRepositoryError{"cannot open project file for reading"};
  const auto end = input.tellg();
  if (end < 0 || !project_size_allowed(static_cast<std::size_t>(end))) {
    throw application::ProjectRepositoryError{"project file exceeds size limit"};
  }
  std::string bytes(static_cast<std::size_t>(end), '\0');
  input.seekg(0);
  if (!bytes.empty() && !input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
    throw application::ProjectRepositoryError{"cannot read complete project file"};
  }
  return bytes;
}

void refresh_availability(application::ProjectRestoreState& state,
                          const std::filesystem::path& project_path) {
  const auto parent = project_path.parent_path().empty() ? std::filesystem::path{"."}
                                                          : project_path.parent_path();
  for (auto& asset : state.assets) {
    const auto authored = asset.source_locator();
    const auto logical = authored.generic_string();
    const bool builtin_sound = logical.starts_with("pulsar-sound:");
    const auto check_path = authored.is_relative() ? parent / authored : authored;
    std::error_code error;
    bool readable = false;
    if (builtin_sound) {
      const auto filename = logical.substr(std::string{"pulsar-sound:"}.size()) + ".wav";
      const auto root = assets_directory();
      const auto candidate = root / "sounds" / filename;
      readable = !root.empty() && std::filesystem::is_regular_file(candidate, error) &&
                 ::access(candidate.c_str(), R_OK) == 0;
    } else {
      const bool regular = std::filesystem::is_regular_file(check_path, error);
      readable = !error && regular && ::access(check_path.c_str(), R_OK) == 0;
    }
    const auto availability = readable ? application::MediaAvailability::Available
                                       : application::MediaAvailability::Unavailable;
    std::optional<std::string> reason;
    if (!readable) {
      reason = error ? ("cannot inspect source: " + error.message())
                     : "source is not a readable regular file";
    }
    if (asset.has_video()) {
      asset = application::MediaAsset{asset.id(),         authored, asset.video_stream_index(),
                                      asset.playable_duration(), asset.metadata(), availability,
                                      reason,              asset.audio()};
    } else if (asset.audio().has_value()) {
      asset = application::MediaAsset::audio_only(asset.id(), authored, asset.playable_duration(),
                                                  *asset.audio(), availability, reason);
    }
  }
}

} // namespace

application::ProjectSaveResult FileProjectRepository::save(
    const std::filesystem::path& destination,
    const application::ProjectSnapshot& snapshot) const {
  std::string bytes;
  try {
    bytes = application::ProjectCodec::encode(snapshot);
  } catch (const application::ProjectCodecError& error) {
    throw application::ProjectRepositoryError{std::string{"cannot encode project: "} +
                                              error.what()};
  }
  if (!project_size_allowed(bytes.size())) {
    throw application::ProjectRepositoryError{"encoded project exceeds size limit"};
  }
  int fd = -1;
  std::filesystem::path temporary;
  for (int attempt = 0; attempt < 32; ++attempt) {
    temporary = temp_path_for(destination, attempt);
    fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd >= 0) break;
    if (errno != EEXIST) fail("cannot create project temporary file");
  }
  if (fd < 0) throw application::ProjectRepositoryError{"cannot obtain unique project temporary file"};

  auto cleanup = [&] { (void)::unlink(temporary.c_str()); };
  std::size_t written = 0;
  while (written < bytes.size()) {
    const ssize_t count = ::write(fd, bytes.data() + written, bytes.size() - written);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      const int saved_errno = errno;
      (void)::close(fd);
      cleanup();
      errno = saved_errno;
      fail("cannot write project temporary file");
    }
    written += static_cast<std::size_t>(count);
  }
  if (::fsync(fd) != 0) {
    const int saved_errno = errno;
    (void)::close(fd);
    cleanup();
    errno = saved_errno;
    fail("cannot fsync project temporary file");
  }
  if (::close(fd) != 0) {
    const int saved_errno = errno;
    cleanup();
    errno = saved_errno;
    fail("cannot close project temporary file");
  }

  if (::rename(temporary.c_str(), destination.c_str()) != 0) {
    const int saved_errno = errno;
    cleanup();
    errno = saved_errno;
    fail("cannot atomically install project file");
  }

  const auto parent = destination.parent_path().empty() ? std::filesystem::path{"."}
                                                         : destination.parent_path();
  const int directory_fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (directory_fd < 0) {
    return application::ProjectSaveResult{
        application::ProjectSaveStatus::CommittedDurabilityUncertain};
  }
  const bool durable = ::fsync(directory_fd) == 0;
  const int close_result = ::close(directory_fd);
  if (!durable || close_result != 0) {
    return application::ProjectSaveResult{
        application::ProjectSaveStatus::CommittedDurabilityUncertain};
  }
  return application::ProjectSaveResult{application::ProjectSaveStatus::Durable};
}

application::ProjectRestoreState FileProjectRepository::load(
    const std::filesystem::path& source) const {
  try {
    auto state = application::ProjectCodec::decode(read_all(source));
    refresh_availability(state, source);
    return state;
  } catch (const application::ProjectCodecError& error) {
    throw application::ProjectRepositoryError{std::string{"cannot decode project file: "} +
                                              error.what()};
  }
}

} // namespace pulsar::project_file
