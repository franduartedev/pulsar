#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTextStream>

namespace {

int fail(const QString& message) {
  QTextStream{stderr} << message << '\n';
  return 2;
}

} // namespace

int main(int argc, char** argv) {
  QCoreApplication app{argc, argv};
  QString descriptor;
  int timeout_ms = 5000;
  for (int index = 1; index < argc; ++index) {
    const QString argument = QString::fromLocal8Bit(argv[index]);
    if (argument == QStringLiteral("--descriptor") && index + 1 < argc) {
      descriptor = QString::fromLocal8Bit(argv[++index]);
    } else if (argument == QStringLiteral("--workspace") && index + 1 < argc) {
      descriptor = QString::fromLocal8Bit(argv[++index]) + QStringLiteral("/agent-bridge.json");
    } else if (argument == QStringLiteral("--timeout-ms") && index + 1 < argc) {
      bool ok = false;
      timeout_ms = QString::fromLocal8Bit(argv[++index]).toInt(&ok);
      if (!ok || timeout_ms < 1 || timeout_ms > 5000) {
        return fail(QStringLiteral("invalid timeout"));
      }
    } else {
      return fail(QStringLiteral(
          "usage: pulsar-agent-client (--workspace <dir> | --descriptor <file>) [--timeout-ms N]"));
    }
  }
  if (descriptor.isEmpty()) return fail(QStringLiteral("agent descriptor is required"));

  QFile file{descriptor};
  if (!file.open(QIODevice::ReadOnly)) return fail(QStringLiteral("cannot read agent descriptor"));
  const auto document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) return fail(QStringLiteral("invalid agent descriptor"));
  const auto endpoint = document.object().value(QStringLiteral("endpoint")).toString();
  const auto token = document.object().value(QStringLiteral("token")).toString();
  if (endpoint.isEmpty() || token.isEmpty()) {
    return fail(QStringLiteral("incomplete agent descriptor"));
  }

  QLocalSocket socket;
  socket.connectToServer(endpoint, QIODevice::ReadWrite);
  if (!socket.waitForConnected(timeout_ms)) {
    return fail(QStringLiteral("cannot connect to agent bridge"));
  }

  QTextStream input{stdin};
  QTextStream output{stdout};
  QByteArray pending;
  while (!input.atEnd()) {
    const auto line = input.readLine().trimmed();
    if (line.isEmpty()) continue;
    const auto request_document = QJsonDocument::fromJson(line.toUtf8());
    if (!request_document.isObject()) {
      return fail(QStringLiteral("stdin line is not a JSON object"));
    }
    auto request = request_document.object();
    request.insert(QStringLiteral("token"), token);
    socket.write(QJsonDocument{request}.toJson(QJsonDocument::Compact) + '\n');
    if (!socket.waitForBytesWritten(timeout_ms)) {
      return fail(QStringLiteral("agent request write timed out"));
    }
    while (!pending.contains('\n')) {
      if (!socket.waitForReadyRead(timeout_ms)) {
        return fail(QStringLiteral("agent response timed out"));
      }
      pending += socket.readAll();
    }
    const auto newline = pending.indexOf('\n');
    output << QString::fromUtf8(pending.left(newline)) << '\n';
    output.flush();
    pending.remove(0, newline + 1);
  }
  socket.disconnectFromServer();
  return 0;
}
