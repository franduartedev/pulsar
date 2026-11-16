#pragma once

#include <QObject>
#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <filesystem>

class DesktopController;
class QLocalServer;
class QLocalSocket;
class QQuickWindow;

// Explicit, local-only automation endpoint for the real desktop controller.
// QLocalServer maps to a named pipe on Windows and a Unix-domain socket on
// Linux. All requests are serialized on the Qt UI thread.
class DesktopAgentBridge final : public QObject {
  Q_OBJECT

public:
  DesktopAgentBridge(DesktopController& controller, QQuickWindow& window,
                     std::filesystem::path workspace, QObject* parent = nullptr);
  ~DesktopAgentBridge() override;

  [[nodiscard]] bool start(QString* error = nullptr);
  [[nodiscard]] QString descriptorPath() const;
  [[nodiscard]] QString endpoint() const { return endpoint_; }

private:
  void acceptConnection();
  void readRequests(QLocalSocket& socket);
  [[nodiscard]] QJsonObject execute(const QJsonObject& request);
  [[nodiscard]] QJsonObject dispatch(const QJsonObject& request);
  [[nodiscard]] std::filesystem::path safePath(const QString& relative, bool must_exist,
                                               const char* purpose) const;
  [[nodiscard]] QJsonObject failure(const QJsonValue& id, const QString& code,
                                    const QString& message) const;

  DesktopController& controller_;
  QQuickWindow& window_;
  std::filesystem::path workspace_;
  QString endpoint_;
  QString token_;
  QLocalServer* server_{nullptr};
  QLocalSocket* active_socket_{nullptr};
  QByteArray input_;
};
