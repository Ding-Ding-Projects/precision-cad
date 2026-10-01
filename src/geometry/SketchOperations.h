#pragma once
#include <QJsonObject>
namespace precision::geometry {
// Protocol v2, isolated sketch solving and associative analytic extrusion only.
QJsonObject executeSketchRequest(const QJsonObject &request);
}
