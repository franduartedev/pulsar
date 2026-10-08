#pragma once

#include <pulsar/application/project/project_restore_state.hpp>
#include <pulsar/application/project/project_snapshot.hpp>

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>

// Crash recovery, separate from the normal project save: while a project has unsaved changes
// a snapshot is written (atomically) to PULSAR's own data folder, never over the user's
// .pulsar file. On the next start a valid, newer snapshot is offered back.
class RecoveryStore {
public:
  struct Entry {
    std::filesystem::path project_path;   // the .pulsar the session belonged to ("" = never saved)
    std::int64_t saved_at{0};             // milliseconds since the epoch
  };

  explicit RecoveryStore(std::filesystem::path directory) : directory_{std::move(directory)} {}
  // $PULSAR_RECOVERY_DIR, else <XDG data home or ~/.local/share>/PULSAR/recovery.
  [[nodiscard]] static std::filesystem::path default_directory();

  // Atomic and exception free: false when the folder is unusable (the app carries on).
  [[nodiscard]] bool write(const pulsar::application::ProjectSnapshot& snapshot,
                           const std::filesystem::path& project_path) const noexcept;
  [[nodiscard]] std::optional<Entry> find() const noexcept;                      // metadata only
  [[nodiscard]] std::optional<pulsar::application::ProjectRestoreState> load() const noexcept;  // nullopt: corrupt
  void clear() const noexcept;
  [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }

private:
  [[nodiscard]] std::filesystem::path snapshot_path() const { return directory_ / "session.pulsar-recovery"; }
  [[nodiscard]] std::filesystem::path meta_path() const { return directory_ / "session.meta"; }
  std::filesystem::path directory_;
};

// Runs recovery writes off the UI thread: one worker, "latest snapshot wins", orderly shutdown.
class RecoveryService {
public:
  explicit RecoveryService(std::filesystem::path directory);
  ~RecoveryService();
  RecoveryService(const RecoveryService&) = delete;
  RecoveryService& operator=(const RecoveryService&) = delete;

  [[nodiscard]] const RecoveryStore& store() const noexcept { return store_; }
  void submit_write(pulsar::application::ProjectSnapshot snapshot, std::filesystem::path project_path);
  void submit_clear();            // drops any pending write, then removes the files
  void flush();                   // waits until everything submitted so far is on disk (tests, shutdown)
  void shutdown();                // idempotent

  [[nodiscard]] std::uint64_t writes_completed() const;

private:
  void run();
  RecoveryStore store_;
  mutable std::mutex mutex_;
  std::condition_variable wake_;
  std::condition_variable idle_;
  std::optional<std::pair<pulsar::application::ProjectSnapshot, std::filesystem::path>> pending_write_;
  bool pending_clear_{false};
  bool busy_{false};
  bool stop_{false};
  std::uint64_t writes_completed_{0};
  std::thread thread_;
};

// Media locators relative to the project file are made absolute so a snapshot stored elsewhere
// still finds its media.
[[nodiscard]] pulsar::application::ProjectSnapshot absolutize_media_locators(
    pulsar::application::ProjectSnapshot snapshot, const std::filesystem::path& project_file);
