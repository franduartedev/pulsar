#include "desktop_agent_bridge.hpp"

#include "desktop_controller.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQuickWindow>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStringList>
#include <QVariant>

#include <algorithm>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <system_error>

namespace {

constexpr qsizetype kMaximumRequestBytes = 1024 * 1024;
constexpr int kMaximumTimeoutMs = 5000;

QString randomToken() {
  QByteArray bytes(32, Qt::Uninitialized);
  for (qsizetype offset = 0; offset < bytes.size(); offset += 4) {
    const auto value = QRandomGenerator::system()->generate();
    const auto count = std::min<qsizetype>(4, bytes.size() - offset);
    std::memcpy(bytes.data() + offset, &value, static_cast<std::size_t>(count));
  }
  return QString::fromLatin1(bytes.toHex());
}

QJsonObject success(const QJsonValue& id, const QJsonObject& payload = {}) {
  QJsonObject response{{QStringLiteral("id"), id}, {QStringLiteral("ok"), true}};
  for (auto it = payload.begin(); it != payload.end(); ++it) response.insert(it.key(), it.value());
  return response;
}

bool pathIsWithin(const std::filesystem::path& root, const std::filesystem::path& path) {
  auto root_it = root.begin();
  auto path_it = path.begin();
  for (; root_it != root.end(); ++root_it, ++path_it) {
    if (path_it == path.end() || *root_it != *path_it) return false;
  }
  return true;
}

} // namespace

DesktopAgentBridge::DesktopAgentBridge(DesktopController& controller, QQuickWindow& window,
                                       std::filesystem::path workspace, QObject* parent)
    : QObject{parent}, controller_{controller}, window_{window}, workspace_{std::move(workspace)} {
  std::error_code error;
  workspace_ = std::filesystem::weakly_canonical(std::filesystem::absolute(workspace_), error);
  if (error) workspace_.clear();
}

DesktopAgentBridge::~DesktopAgentBridge() {
  if (server_) server_->close();
  if (!endpoint_.isEmpty()) QLocalServer::removeServer(endpoint_);
  const auto descriptor = descriptorPath();
  if (!descriptor.isEmpty()) QFile::remove(descriptor);
}

QString DesktopAgentBridge::descriptorPath() const {
  if (workspace_.empty()) return {};
  return QString::fromStdString((workspace_ / "agent-bridge.json").string());
}

bool DesktopAgentBridge::start(QString* error) {
  if (workspace_.empty() || !std::filesystem::is_directory(workspace_)) {
    if (error) *error = QStringLiteral("workspace must be an existing directory");
    return false;
  }
  const auto identity =
      QString::fromStdString(workspace_.generic_string()).toUtf8() +
      QByteArray::number(QCoreApplication::applicationPid()) + randomToken().toUtf8();
  const auto endpoint_hash =
      QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(24);
  endpoint_ = QStringLiteral("pulsar-agent-%1")
                  .arg(QString::fromLatin1(endpoint_hash));
  token_ = randomToken();

  server_ = new QLocalServer{this};
  server_->setSocketOptions(QLocalServer::UserAccessOption);
  connect(server_, &QLocalServer::newConnection, this, &DesktopAgentBridge::acceptConnection);
  if (!server_->listen(endpoint_)) {
    if (error) {
      *error = QStringLiteral("cannot create local agent endpoint: %1")
                   .arg(server_->errorString());
    }
    return false;
  }

  QJsonObject descriptor{{QStringLiteral("protocol"), QStringLiteral("pulsar-agent-jsonl-v1")},
                         {QStringLiteral("transport"), QStringLiteral("qt-local")},
                         {QStringLiteral("endpoint"), endpoint_},
                         {QStringLiteral("token"), token_},
                         {QStringLiteral("pid"),
                          static_cast<qint64>(QCoreApplication::applicationPid())},
                         {QStringLiteral("workspace"),
                          QString::fromStdString(workspace_.generic_string())}};
  QSaveFile file{descriptorPath()};
  if (!file.open(QIODevice::WriteOnly)) {
    if (error) *error = QStringLiteral("cannot write agent descriptor");
    server_->close();
    return false;
  }
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  file.write(QJsonDocument{descriptor}.toJson(QJsonDocument::Compact));
  file.write("\n");
  if (!file.commit()) {
    if (error) *error = QStringLiteral("cannot commit agent descriptor");
    server_->close();
    return false;
  }
  QFile::setPermissions(descriptorPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  return true;
}

void DesktopAgentBridge::acceptConnection() {
  while (server_ && server_->hasPendingConnections()) {
    auto* socket = server_->nextPendingConnection();
    if (active_socket_ && active_socket_->state() == QLocalSocket::ConnectedState) {
      const auto response = failure({}, QStringLiteral("busy"),
                                    QStringLiteral("another agent connection is active"));
      socket->write(QJsonDocument{response}.toJson(QJsonDocument::Compact) + '\n');
      socket->flush();
      socket->disconnectFromServer();
      socket->deleteLater();
      continue;
    }
    active_socket_ = socket;
    input_.clear();
    connect(socket, &QLocalSocket::readyRead, this, [this, socket] { readRequests(*socket); });
    connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
      if (active_socket_ == socket) {
        active_socket_ = nullptr;
        input_.clear();
      }
      socket->deleteLater();
    });
    if (socket->bytesAvailable() > 0) readRequests(*socket);
  }
}

void DesktopAgentBridge::readRequests(QLocalSocket& socket) {
  input_ += socket.readAll();
  if (input_.size() > kMaximumRequestBytes) {
    socket.write(QJsonDocument{failure({}, QStringLiteral("request_too_large"),
                                       QStringLiteral("request exceeds 1 MiB"))}
                     .toJson(QJsonDocument::Compact) +
                 '\n');
    socket.disconnectFromServer();
    return;
  }
  qsizetype newline = -1;
  while ((newline = input_.indexOf('\n')) >= 0) {
    const QByteArray line = input_.left(newline).trimmed();
    input_.remove(0, newline + 1);
    if (line.isEmpty()) continue;
    QJsonParseError parse_error;
    const auto document = QJsonDocument::fromJson(line, &parse_error);
    QJsonObject response;
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
      response = failure({}, QStringLiteral("invalid_json"),
                         QStringLiteral("request must be a JSON object"));
    } else {
      response = execute(document.object());
    }
    socket.write(QJsonDocument{response}.toJson(QJsonDocument::Compact) + '\n');
    socket.flush();
  }
}

QJsonObject DesktopAgentBridge::failure(const QJsonValue& id, const QString& code,
                                        const QString& message) const {
  return {{QStringLiteral("id"), id},
          {QStringLiteral("ok"), false},
          {QStringLiteral("error"),
           QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("message"), message}}}};
}

QJsonObject DesktopAgentBridge::execute(const QJsonObject& request) {
  const auto id = request.value(QStringLiteral("id"));
  if (id.isUndefined() || id.isNull()) {
    return failure({}, QStringLiteral("invalid_request"), QStringLiteral("id is required"));
  }
  if (request.value(QStringLiteral("token")).toString() != token_) {
    return failure(id, QStringLiteral("unauthorized"), QStringLiteral("invalid bridge token"));
  }
  const int timeout = request.value(QStringLiteral("timeout_ms")).toInt(kMaximumTimeoutMs);
  if (timeout < 1 || timeout > kMaximumTimeoutMs) {
    return failure(id, QStringLiteral("invalid_timeout"),
                   QStringLiteral("timeout_ms must be between 1 and 5000"));
  }
  QElapsedTimer elapsed;
  elapsed.start();
  auto response = dispatch(request);
  if (elapsed.elapsed() > timeout) {
    return failure(id, QStringLiteral("timeout"), QStringLiteral("request exceeded its deadline"));
  }
  return response;
}

std::filesystem::path DesktopAgentBridge::safePath(const QString& text, bool must_exist,
                                                   const char* purpose) const {
  const auto relative = std::filesystem::path{text.toStdString()};
  if (relative.empty() || relative.is_absolute()) {
    throw std::invalid_argument{std::string{purpose} + " path must be relative to the workspace"};
  }
  std::error_code error;
  const auto resolved = std::filesystem::weakly_canonical(workspace_ / relative, error);
  if (error || !pathIsWithin(workspace_, resolved)) {
    throw std::invalid_argument{std::string{purpose} + " path escapes the workspace"};
  }
  if (must_exist && !std::filesystem::is_regular_file(resolved)) {
    throw std::invalid_argument{std::string{purpose} + " file does not exist"};
  }
  return resolved;
}

QJsonObject DesktopAgentBridge::dispatch(const QJsonObject& request) {
  const auto id = request.value(QStringLiteral("id"));
  const auto command = request.value(QStringLiteral("command")).toString();
  if (command.isEmpty()) {
    return failure(id, QStringLiteral("invalid_request"), QStringLiteral("command is required"));
  }
  try {
    if (command == QStringLiteral("status")) {
      return success(
          id, {{QStringLiteral("status"),
                QJsonObject::fromVariantMap(controller_.agentStatus())}});
    }
    if (command == QStringLiteral("open_project")) {
      const auto path =
          safePath(request.value(QStringLiteral("path")).toString(), true, "project");
      if (path.extension() != ".pulsar") {
        throw std::invalid_argument{"project must use the .pulsar extension"};
      }
      if (!controller_.openProject(QString::fromStdString(path.string()))) {
        return failure(id, QStringLiteral("open_failed"), controller_.errorMessage());
      }
      return success(
          id, {{QStringLiteral("path"), QString::fromStdString(path.generic_string())}});
    }
    if (command == QStringLiteral("save_project")) {
      const auto path =
          safePath(request.value(QStringLiteral("path")).toString(), false, "project");
      if (path.extension() != ".pulsar") {
        throw std::invalid_argument{"project must use the .pulsar extension"};
      }
      if (!controller_.saveProject(QString::fromStdString(path.string()))) {
        return failure(id, QStringLiteral("save_failed"), controller_.errorMessage());
      }
      return success(
          id, {{QStringLiteral("path"), QString::fromStdString(path.generic_string())}});
    }
    if (command == QStringLiteral("timeline_state")) {
      return success(id, {{QStringLiteral("tracks"),
                           QJsonArray::fromVariantList(controller_.agentTimelineState())}});
    }
    if (command == QStringLiteral("import_media")) {
      const auto path = safePath(request.value(QStringLiteral("path")).toString(), true, "media");
      auto extension = QString::fromStdString(path.extension().string()).toLower();
      static const QStringList supported{QStringLiteral(".mp4"), QStringLiteral(".mov"),
                                         QStringLiteral(".mkv"), QStringLiteral(".avi"),
                                         QStringLiteral(".webm"), QStringLiteral(".m4v"),
                                         QStringLiteral(".wav"), QStringLiteral(".mp3"),
                                         QStringLiteral(".m4a"), QStringLiteral(".aac"),
                                         QStringLiteral(".flac")};
      if (!supported.contains(extension)) {
        throw std::invalid_argument{"media must use a supported video or audio extension"};
      }
      QString problem;
      if (!controller_.agentImportMedia(QString::fromStdString(path.string()), &problem)) {
        return failure(id, QStringLiteral("import_rejected"), problem);
      }
      return success(id, {{QStringLiteral("accepted"), true},
                          {QStringLiteral("path"), QString::fromStdString(path.generic_string())}});
    }
    if (command == QStringLiteral("media_assets")) {
      return success(id, {{QStringLiteral("assets"),
                           QJsonArray::fromVariantList(controller_.agentMediaAssets())},
                          {QStringLiteral("import_pending"), controller_.importPending()},
                          {QStringLiteral("busy"), controller_.busy()}});
    }
    if (command == QStringLiteral("add_clip")) {
      QString problem;
      std::optional<double> source_start;
      std::optional<double> source_end;
      if (request.contains(QStringLiteral("source_start_seconds"))) {
        source_start = request.value(QStringLiteral("source_start_seconds")).toDouble(-1.0);
      }
      if (request.contains(QStringLiteral("source_end_seconds"))) {
        source_end = request.value(QStringLiteral("source_end_seconds")).toDouble(-1.0);
      }
      const auto created = controller_.agentAddClip(
          request.value(QStringLiteral("media_id")).toVariant().toULongLong(),
          request.value(QStringLiteral("track_id")).toVariant().toULongLong(),
          request.value(QStringLiteral("timeline_start_seconds")).toDouble(-1.0),
          source_start, source_end, &problem);
      if (!created) return failure(id, QStringLiteral("edit_rejected"), problem);
      return success(id, {{QStringLiteral("clip_id"), static_cast<qint64>(*created)}});
    }
    if (command == QStringLiteral("remove_clip")) {
      const auto clip_id = request.value(QStringLiteral("clip_id")).toVariant().toULongLong();
      QString problem;
      if (!controller_.agentRemoveClip(clip_id, &problem)) {
        return failure(id, QStringLiteral("edit_rejected"), problem);
      }
      return success(id, {{QStringLiteral("removed_clip_id"), static_cast<qint64>(clip_id)}});
    }
    if (command == QStringLiteral("split_clip")) {
      QString problem;
      const auto created = controller_.agentSplitClip(
          request.value(QStringLiteral("clip_id")).toVariant().toULongLong(),
          request.value(QStringLiteral("position_seconds")).toDouble(-1.0), &problem);
      if (!created) return failure(id, QStringLiteral("edit_rejected"), problem);
      return success(id, {{QStringLiteral("created_clip_id"), static_cast<qint64>(*created)}});
    }
    if (command == QStringLiteral("seek")) {
      QString problem;
      if (!controller_.agentSeekSeconds(
              request.value(QStringLiteral("position_seconds")).toDouble(-1.0), &problem)) {
        return failure(id, QStringLiteral("seek_rejected"), problem);
      }
      return success(id, {{QStringLiteral("position_seconds"),
                           controller_.agentPlaybackPositionSeconds()}});
    }
    if (command == QStringLiteral("play")) {
      controller_.play();
      if (!controller_.playing()) {
        return failure(id, QStringLiteral("play_rejected"),
                       QStringLiteral("playback could not start"));
      }
      return success(id, {{QStringLiteral("playing"), true}});
    }
    if (command == QStringLiteral("pause")) {
      controller_.pause();
      return success(id, {{QStringLiteral("playing"), controller_.playing()}});
    }
    if (command == QStringLiteral("capture_screenshot")) {
      const auto path =
          safePath(request.value(QStringLiteral("path")).toString(), false, "screenshot");
      if (path.extension() != ".png") {
        throw std::invalid_argument{"screenshot must use the .png extension"};
      }
      if (!window_.grabWindow().save(QString::fromStdString(path.string()), "PNG")) {
        return failure(id, QStringLiteral("capture_failed"),
                       QStringLiteral("window capture could not be written"));
      }
      return success(
          id, {{QStringLiteral("path"), QString::fromStdString(path.generic_string())},
               {QStringLiteral("bytes"),
                static_cast<qint64>(std::filesystem::file_size(path))}});
    }
    if (command == QStringLiteral("get_metrics")) {
      // performanceSnapshot() queries live playback workers and can wait on a
      // hardware audio callback. While playing, use the diagnostics already
      // sampled by DesktopController::updateTick so the UI thread stays
      // responsive and the values still describe the real playback session.
      const auto diagnostics = controller_.debugMetrics();
      const auto performance =
          controller_.playing() ? diagnostics : controller_.performanceSnapshot();
      return success(id, {{QStringLiteral("performance"), performance},
                          {QStringLiteral("diagnostics"), diagnostics},
                          {QStringLiteral("position_seconds"),
                           controller_.agentPlaybackPositionSeconds()},
                          {QStringLiteral("playing"), controller_.playing()}});
    }
    return failure(id, QStringLiteral("unknown_command"),
                   QStringLiteral("command is not supported"));
  } catch (const std::exception& exception) {
    return failure(id, QStringLiteral("invalid_request"), QString::fromUtf8(exception.what()));
  }
}
