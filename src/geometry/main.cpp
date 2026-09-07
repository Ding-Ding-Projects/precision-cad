#include "GeometryWorker.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <cstdio>
#include <iostream>
#include <algorithm>

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  constexpr std::streamsize kMaxInputBytes = 4 * 1024 * 1024;
  QByteArray input; char buffer[8192]; std::streamsize remaining = kMaxInputBytes + 1;
  while (remaining > 0 && std::cin.good()) { const std::streamsize wanted = std::min<std::streamsize>(remaining, sizeof(buffer)); std::cin.read(buffer, wanted); const std::streamsize count = std::cin.gcount(); if (count > 0) input.append(buffer, static_cast<int>(count)); remaining -= count; }
  QJsonParseError parseError; const QJsonDocument request = QJsonDocument::fromJson(input, &parseError);
  QJsonObject reply;
  if (input.size() > kMaxInputBytes) reply = QJsonObject{{"protocolVersion", precision::geometry::kProtocolVersion}, {"ok", false}, {"error", QJsonObject{{"code", "request_too_large"}, {"message", "Request exceeds the worker limit."}}}};
  else if (parseError.error != QJsonParseError::NoError || !request.isObject()) reply = QJsonObject{{"protocolVersion", precision::geometry::kProtocolVersion}, {"ok", false}, {"error", QJsonObject{{"code", "invalid_json"}, {"message", "Expected one JSON object."}}}};
  else reply = precision::geometry::executeRequest(request.object());
  const QByteArray output = QJsonDocument(reply).toJson(QJsonDocument::Compact);
  std::fwrite(output.constData(), 1, static_cast<size_t>(output.size()), stdout); std::fwrite("\n", 1, 1, stdout); return reply.value("ok").toBool() ? 0 : 2;
}
