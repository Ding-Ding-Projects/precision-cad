#pragma once

#include <QJsonObject>

namespace precision::geometry {

constexpr int kProtocolVersion = 1;

// Executes one immutable, revision-bound worker request. The returned object is safe
// to write directly as a single JSON line on stdout.
QJsonObject executeRequest(const QJsonObject& request);

} // namespace precision::geometry
