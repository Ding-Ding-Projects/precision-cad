#include "mesh_canvas.h"

#include <QPainter>
#include <QMatrix4x4>
#include <QtMath>
#include <algorithm>

MeshCanvas::MeshCanvas(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  setRenderTarget(QQuickPaintedItem::FramebufferObject);
}
QVariantList MeshCanvas::vertices() const { return m_vertexValues; }
QVariantList MeshCanvas::indices() const { return m_indexValues; }
void MeshCanvas::setVertices(const QVariantList &value) {
  m_vertexValues = value; m_vertices.clear();
  for (qsizetype i = 0; i + 2 < value.size(); i += 3) m_vertices.append(QVector3D(value[i].toFloat(), value[i+1].toFloat(), value[i+2].toFloat()));
  if (!m_vertices.isEmpty()) { QVector3D lo = m_vertices.first(), hi = lo; for (const auto &p : m_vertices) { lo.setX(std::min(lo.x(), p.x())); lo.setY(std::min(lo.y(), p.y())); lo.setZ(std::min(lo.z(), p.z())); hi.setX(std::max(hi.x(), p.x())); hi.setY(std::max(hi.y(), p.y())); hi.setZ(std::max(hi.z(), p.z())); } m_center = (lo + hi) * .5f; m_extent = std::max<qreal>(0.001, (hi - lo).length()); }
  update(); emit meshChanged();
}
void MeshCanvas::setIndices(const QVariantList &value) { m_indexValues = value; m_indices.clear(); for (const auto &v : value) m_indices.append(v.toInt()); update(); emit meshChanged(); }
void MeshCanvas::setYaw(qreal value) { m_yaw = value; update(); emit viewChanged(); }
void MeshCanvas::setPitch(qreal value) { m_pitch = std::clamp(value, -89.0, 89.0); update(); emit viewChanged(); }
void MeshCanvas::setZoom(qreal value) { m_zoom = std::clamp(value, .1, 20.0); update(); emit viewChanged(); }
void MeshCanvas::orbit(qreal x, qreal y) { setYaw(m_yaw + x); setPitch(m_pitch + y); }
void MeshCanvas::pan(qreal x, qreal y) { m_panX += x; m_panY += y; update(); emit viewChanged(); }
void MeshCanvas::fit() { m_yaw = -35; m_pitch = 25; m_zoom = 1; m_panX = m_panY = 0; update(); emit viewChanged(); }
QPointF MeshCanvas::project(const QVector3D &point) const {
  QMatrix4x4 rotation; rotation.rotate(m_yaw, 0, 1, 0); rotation.rotate(m_pitch, 1, 0, 0);
  const QVector3D v = rotation * (point - m_center);
  const qreal scale = .78 * std::min(width(), height()) * m_zoom / m_extent;
  return {width() / 2 + m_panX + v.x() * scale, height() / 2 + m_panY - v.y() * scale};
}
void MeshCanvas::paint(QPainter *painter) {
  painter->fillRect(boundingRect(), QColor("#10131a"));
  if (m_vertices.isEmpty() || m_indices.size() < 3) { painter->setPen(QColor("#c6c9d3")); painter->drawText(boundingRect(), Qt::AlignCenter, tr("No regenerated mesh")); return; }
  painter->setRenderHint(QPainter::Antialiasing);
  QVector<QPair<qreal, QPolygonF>> faces;
  QMatrix4x4 rotation; rotation.rotate(m_yaw, 0, 1, 0); rotation.rotate(m_pitch, 1, 0, 0);
  for (int i = 0; i + 2 < m_indices.size(); i += 3) { int a=m_indices[i], b=m_indices[i+1], c=m_indices[i+2]; if (a<0||b<0||c<0||a>=m_vertices.size()||b>=m_vertices.size()||c>=m_vertices.size()) continue; const QVector3D av=rotation*(m_vertices[a]-m_center), bv=rotation*(m_vertices[b]-m_center), cv=rotation*(m_vertices[c]-m_center); QPolygonF poly; poly << project(m_vertices[a]) << project(m_vertices[b]) << project(m_vertices[c]); faces.append({(av.z()+bv.z()+cv.z())/3, poly}); }
  std::sort(faces.begin(), faces.end(), [](const auto &a, const auto &b){ return a.first < b.first; });
  painter->setPen(QPen(QColor("#99cbff"), 0)); painter->setBrush(QColor("#2b6cb0")); for (const auto &face : faces) painter->drawPolygon(face.second);
}
