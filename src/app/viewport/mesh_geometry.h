#pragma once

#include <QQuick3DGeometry>
#include <QVariantList>

namespace precision::app::viewport {

class MeshGeometry : public QQuick3DGeometry {
  Q_OBJECT
  Q_PROPERTY(QVariantList vertices READ vertices WRITE setVertices NOTIFY meshChanged)
  Q_PROPERTY(QVariantList indices READ indices WRITE setIndices NOTIFY meshChanged)
  Q_PROPERTY(QVariantList normals READ normals WRITE setNormals NOTIFY meshChanged)
public:
  explicit MeshGeometry(QQuick3DObject *parent = nullptr);
  [[nodiscard]] QVariantList vertices() const { return m_vertexValues; }
  [[nodiscard]] QVariantList indices() const { return m_indexValues; }
  [[nodiscard]] QVariantList normals() const { return m_normalValues; }
  void setVertices(const QVariantList &value);
  void setIndices(const QVariantList &value);
  void setNormals(const QVariantList &value);
signals: void meshChanged();
private:
  void rebuild();
  QVariantList m_vertexValues, m_indexValues, m_normalValues;
};
} // namespace precision::app::viewport
