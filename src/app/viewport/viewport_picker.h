#pragma once
#include "viewport_camera.h"
#include <QVector>

namespace precision::app::viewport {
struct MeshData { QVector<QVector3D> vertices; QVector<quint32> indices; };
enum class PickKind { None, Face, Edge, Vertex };
struct PickResult { PickKind kind = PickKind::None; int triangleIndex = -1; int vertexIndex = -1; int edgeIndex = -1; float distance = -1.0f; };
class ViewportPicker final { public: static PickResult pick(const MeshData &mesh, const Ray &ray, float worldTolerance); };
} // namespace precision::app::viewport
