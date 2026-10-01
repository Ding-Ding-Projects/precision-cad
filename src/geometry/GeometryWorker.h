#pragma once

#include <QJsonArray>
#include <QJsonObject>

namespace precision::geometry {

constexpr int kProtocolVersion = 1;

// Executes one immutable, revision-bound worker request. The returned object is safe
// to write directly as a single JSON line on stdout.
QJsonObject executeRequest(const QJsonObject& request);

// Applies the same compact-JSON byte boundary used by executeRequest. Exposed
// separately so the real serializer boundary is exercised without a huge mesh.
QJsonObject boundResponse(const QJsonObject& response);

// Malformed schemas or duplicate IDs are invalid. Zero candidates is missing and more than
// one candidate is ambiguous, so neither state can silently select an entity.
QJsonObject resolveTopologyReference(const QJsonArray& entities, const QJsonObject& reference);

} // namespace precision::geometry
