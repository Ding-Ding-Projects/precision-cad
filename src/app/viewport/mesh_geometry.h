#pragma once
#include <QQuick3DGeometry>
#include <QVariantList>
#include <QVector3D>
#include <QVector>
#include <array>
namespace precision::app::viewport {
class MeshGeometry : public QQuick3DGeometry {
  Q_OBJECT
  Q_PROPERTY(QVariantList vertices READ vertices WRITE setVertices NOTIFY meshChanged)
  Q_PROPERTY(QVariantList indices READ indices WRITE setIndices NOTIFY meshChanged)
  Q_PROPERTY(QVariantList normals READ normals WRITE setNormals NOTIFY meshChanged)
  Q_PROPERTY(QVariantList parts READ parts WRITE setParts NOTIFY meshChanged)
  Q_PROPERTY(QString fallbackBodyId MEMBER m_fallbackBodyId NOTIFY meshChanged)
  Q_PROPERTY(bool valid READ valid NOTIFY meshChanged)
  Q_PROPERTY(bool suppliedNormals READ suppliedNormals NOTIFY meshChanged)
public:
  explicit MeshGeometry(QQuick3DObject *parent = nullptr);
  QVariantList vertices() const { return m_vertexValues; }
  QVariantList indices() const { return m_indexValues; }
  QVariantList normals() const { return m_normalValues; }
  QVariantList parts() const { return m_parts; }
  void setParts(const QVariantList &value);
  QString bodyForTriangle(int index) const;
  bool valid() const { return m_valid; }
  bool suppliedNormals() const { return m_suppliedNormals; }
  void setVertices(const QVariantList &value);
  void setIndices(const QVariantList &value);
  void setNormals(const QVariantList &value);
  const QVector<QVector3D> &localPositions() const { return m_positions; }
  const QVector<quint32> &validatedIndices() const { return m_indices; }
  QVariantList sourcePoint(QVector3D local) const;
  double sourceScale() const { return m_scale; }
signals:
  void meshChanged();
  void sceneChanged();
private:
  void rebuild();
  QVariantList m_parts;
  QString m_fallbackBodyId;
  struct PartRange { QString bodyId; int first; int count; };
  QVector<PartRange> m_ranges;
  QVariantList m_vertexValues, m_indexValues, m_normalValues;
  QVector<QVector3D> m_positions;
  QVector<quint32> m_indices;
  std::array<double,3> m_origin{};
  double m_scale=1;
  bool m_valid=false, m_suppliedNormals=false;
};
}
