#include "recovery_service.hpp"

#include <pulsar/application/media/still_media.hpp>

#include <pulsar/project_file/file_project_repository.hpp>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

fs::path RecoveryStore::default_directory() {
  if (const char* override_dir = std::getenv("PULSAR_RECOVERY_DIR"); override_dir != nullptr && *override_dir != '\0') {
    return fs::path{override_dir};
  }
#if defined(_WIN32)
  if (const char* local = std::getenv("LOCALAPPDATA"); local != nullptr && *local != '\0') return fs::path{local} / "PULSAR" / "recovery";
  if (const char* roaming = std::getenv("APPDATA"); roaming != nullptr && *roaming != '\0') return fs::path{roaming} / "PULSAR" / "recovery";
#endif
  if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg != nullptr && *xdg != '\0') return fs::path{xdg} / "PULSAR" / "recovery";
  if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') return fs::path{home} / ".local" / "share" / "PULSAR" / "recovery";
  return fs::temp_directory_path() / "PULSAR" / "recovery";
}

bool RecoveryStore::write(const pulsar::application::ProjectSnapshot& snapshot, const fs::path& project_path) const noexcept {
  try {
    std::error_code error;
    fs::create_directories(directory_, error);
    if (error) return false;
    // The project file adapter writes atomically (temporary sibling, fsync, rename).
    (void)pulsar::project_file::FileProjectRepository{}.save(snapshot_path(), snapshot);
    const auto temporary = directory_ / "session.meta.tmp";
    {
      std::ofstream out{temporary, std::ios::binary | std::ios::trunc};
      if (!out) return false;
      const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      out << "PULSAR_RECOVERY 1\nsaved_at " << now << "\nproject " << project_path.string() << "\nEND\n";
      out.flush();
      if (!out) { fs::remove(temporary, error); return false; }
    }
    fs::rename(temporary, meta_path(), error);
    if (error) { fs::remove(temporary, error); return false; }
    return true;
  } catch (...) {
    return false;
  }
}

std::optional<RecoveryStore::Entry> RecoveryStore::find() const noexcept {
  try {
    std::error_code error;
    if (!fs::is_regular_file(meta_path(), error) || !fs::is_regular_file(snapshot_path(), error)) return std::nullopt;
    std::ifstream in{meta_path(), std::ios::binary};
    std::string line;
    if (!std::getline(in, line) || line != "PULSAR_RECOVERY 1") return std::nullopt;
    Entry entry;
    bool have_time = false, ended = false;
    while (std::getline(in, line)) {
      if (line.rfind("saved_at ", 0) == 0) {
        try { entry.saved_at = std::stoll(line.substr(9)); have_time = true; } catch (...) { return std::nullopt; }
      } else if (line.rfind("project", 0) == 0) {
        entry.project_path = line.size() > 8 ? fs::path{line.substr(8)} : fs::path{};
      } else if (line == "END") { ended = true; break; }
    }
    if (!have_time || !ended) return std::nullopt;
    return entry;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<pulsar::application::ProjectRestoreState> RecoveryStore::load() const noexcept {
  try {
    return pulsar::project_file::FileProjectRepository{}.load(snapshot_path());
  } catch (...) {
    return std::nullopt;
  }
}

void RecoveryStore::clear() const noexcept {
  std::error_code error;
  fs::remove(meta_path(), error);
  fs::remove(snapshot_path(), error);
  fs::remove(directory_ / "session.meta.tmp", error);
}

RecoveryService::RecoveryService(fs::path directory) : store_{std::move(directory)} {
  thread_ = std::thread{[this] { run(); }};
}

RecoveryService::~RecoveryService() { shutdown(); }

void RecoveryService::submit_write(pulsar::application::ProjectSnapshot snapshot, fs::path project_path) {
  {
    std::lock_guard lock{mutex_};
    if (stop_) return;
    pending_write_.emplace(std::move(snapshot), std::move(project_path));
    pending_clear_ = false;
  }
  wake_.notify_one();
}

void RecoveryService::submit_clear() {
  {
    std::lock_guard lock{mutex_};
    if (stop_) { store_.clear(); return; }
    pending_write_.reset();
    pending_clear_ = true;
  }
  wake_.notify_one();
}

void RecoveryService::flush() {
  std::unique_lock lock{mutex_};
  idle_.wait(lock, [this] { return stop_ || (!busy_ && !pending_write_.has_value() && !pending_clear_); });
}

void RecoveryService::shutdown() {
  {
    std::lock_guard lock{mutex_};
    if (stop_) return;
    stop_ = true;
    pending_write_.reset();   // an unfinished write is simply abandoned: the old file stays valid
  }
  wake_.notify_all();
  idle_.notify_all();
  if (thread_.joinable()) thread_.join();
}

std::uint64_t RecoveryService::writes_completed() const {
  std::lock_guard lock{mutex_};
  return writes_completed_;
}

void RecoveryService::run() {
  for (;;) {
    std::optional<std::pair<pulsar::application::ProjectSnapshot, fs::path>> job;
    bool clear = false;
    {
      std::unique_lock lock{mutex_};
      wake_.wait(lock, [this] { return stop_ || pending_write_.has_value() || pending_clear_; });
      if (stop_) return;
      if (pending_clear_) { clear = true; pending_clear_ = false; }
      else { job = std::move(pending_write_); pending_write_.reset(); }
      busy_ = true;
    }
    if (clear) store_.clear();
    else if (job) {
      const bool ok = store_.write(job->first, job->second);
      std::lock_guard lock{mutex_};
      if (ok) ++writes_completed_;
    }
    {
      std::lock_guard lock{mutex_};
      busy_ = false;
    }
    idle_.notify_all();
  }
}

pulsar::application::ProjectSnapshot absolutize_media_locators(pulsar::application::ProjectSnapshot snapshot,
                                                                const fs::path& project_file) {
  using pulsar::application::MediaAsset;
  const auto base = project_file.parent_path();
  for (auto& asset : snapshot.assets) {
    const auto& locator = asset.source_locator();
    if (locator.is_absolute() || locator.string().rfind("pulsar-sound:", 0) == 0 || pulsar::application::is_color_card_locator(locator.generic_string()) || base.empty()) continue;
    const auto absolute = (base / locator).lexically_normal();
    if (asset.has_video()) {
      asset = MediaAsset{asset.id(), absolute, asset.video_stream_index(), asset.playable_duration(), asset.metadata(),
                         asset.availability(), asset.availability_reason(), asset.audio()};
    } else if (asset.audio().has_value()) {
      asset = MediaAsset::audio_only(asset.id(), absolute, asset.playable_duration(), *asset.audio(),
                                     asset.availability(), asset.availability_reason());
    }
  }
  return snapshot;
}
