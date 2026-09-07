#include "GeometryWorker.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <cstdio>
#include <iostream>

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  const QByteArray input = QByteArray::fromStdString(std::string((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>()));
  QJsonParseError parseError; const QJsonDocument request = QJsonDocument::fromJson(input, &parseError);
  QJsonObject reply;
  if (parseError.error != QJsonParseError::NoError || !request.isObject()) reply = QJsonObject{{"protocolVersion", precision::geometry::kProtocolVersion}, {"ok", false}, {"error", QJsonObject{{"code", "invalid_json"}, {"message", "Expected one JSON object."}}}};
  else reply = precision::geometry::executeRequest(request.object());
  const QByteArray output = QJsonDocument(reply).toJson(QJsonDocument::Compact);
  std::fwrite(output.constData(), 1, static_cast<size_t>(output.size()), stdout); std::fwrite("\n", 1, 1, stdout); return reply.value("ok").toBool() ? 0 : 2;
}
