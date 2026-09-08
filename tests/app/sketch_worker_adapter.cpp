// Test-only dispatch bridge. The production parser and both actual numerical
// implementations are shared unchanged; no synthetic geometry is supplied.
#include "GeometryWorker.h"
#include "SketchOperations.h"
namespace precision::geometry {
QJsonObject dispatchRequest(const QJsonObject &request) {
    return request.value("protocolVersion")==QJsonValue(2) ? executeSketchRequest(request) : executeRequest(request);
}
}
#define executeRequest dispatchRequest
#include "../../src/geometry/main.cpp"
#undef executeRequest
