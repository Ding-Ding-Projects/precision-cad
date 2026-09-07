#pragma once

#include <QQuickPaintedItem>
#include <QVector3D>

class MeshCanvas : public QQuickPaintedItem {
  Q_OBJECT
  Q_PROPERTY(QVariantList vertices READ vertices WRITE setVertices NOTIFY meshChanged)
  Q_PROPERTY(QVariantList indices READ indices WRITE setIndices NOTIFY meshChanged)
  Q_PROPERTY(qreal yaw READ yaw WRITE setYaw NOTIFY viewChanged)
  Q_PROPERTY(qreal pitch READ pitch WRITE setPitch NOTIFY viewChanged)
  Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY viewChanged)
public:
  explicit MeshCanvas(QQuickItem *parent = nullptr);
  QVariantList vertices() const;
  QVariantList indices() const;
  qreal yaw() const { return m_yaw; }
  qreal pitch() const { return m_pitch; }
  qreal zoom() const { return m_zoom; }
  void setVertices(const QVariantList &value);
  void setIndices(const QVariantList &value);
  void setYaw(qreal value);
  void setPitch(qreal value);
  void setZoom(qreal value);
  Q_INVOKABLE void orbit(qreal deltaYaw, qreal deltaPitch);
  Q_INVOKABLE void pan(qreal dx, qreal dy);
  Q_INVOKABLE void fit();
  void paint(QPainter *painter) override;
signals:
  void meshChanged();
  void viewChanged();
private:
  QPointF project(const QVector3D &point) const;
  QVector<QVector3D> m_vertices;
  QVector<int> m_indices;
  QVariantList m_vertexValues, m_indexValues;
  qreal m_yaw = -35.0, m_pitch = 25.0, m_zoom = 1.0, m_panX = 0.0, m_panY = 0.0;
  QVector3D m_center;
  qreal m_extent = 1.0;
};
