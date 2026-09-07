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
  Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY viewChanged)
  Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor NOTIFY viewChanged)
  Q_PROPERTY(QColor bodyColor READ bodyColor WRITE setBodyColor NOTIFY viewChanged)
  Q_PROPERTY(QString emptyText READ emptyText WRITE setEmptyText NOTIFY viewChanged)
public:
  QColor backgroundColor() const { return m_background; }
  QColor foregroundColor() const { return m_foreground; }
  QColor bodyColor() const { return m_body; }
  QString emptyText() const { return m_emptyText; }
  void setBackgroundColor(QColor value) { if(m_background==value) return; m_background=value; update(); emit viewChanged(); }
  void setForegroundColor(QColor value) { if(m_foreground==value) return; m_foreground=value; update(); emit viewChanged(); }
  void setBodyColor(QColor value) { if(m_body==value) return; m_body=value; update(); emit viewChanged(); }
  void setEmptyText(QString value) { if(m_emptyText==value) return; m_emptyText=std::move(value); update(); emit viewChanged(); }
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
  QColor m_background, m_foreground, m_body;
  QString m_emptyText;
  QPointF project(const QVector3D &point) const;
  QVector<QVector3D> m_vertices;
  QVector<int> m_indices;
  QVariantList m_vertexValues, m_indexValues;
  qreal m_yaw = -35.0, m_pitch = 25.0, m_zoom = 1.0, m_panX = 0.0, m_panY = 0.0;
  QVector3D m_center;
  qreal m_extent = 1.0;
};
