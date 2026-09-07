#include "viewport_camera.h"

#include <QtMath>
#include <algorithm>

namespace precision::app::viewport {

void ViewportCamera::setViewport(QSizeF value) { m_viewport = {std::max<qreal>(1.0, value.width()), std::max<qreal>(1.0, value.height())}; }
void ViewportCamera::setSceneBounds(QVector3D minimum, QVector3D maximum) {
  m_center = (minimum + maximum) * 0.5f;
  m_radius = std::max<qreal>(0.001, (maximum - minimum).length() * 0.5);
  m_distance = std::max<qreal>(m_radius * 2.8, 0.01);
}
void ViewportCamera::setStandardView(StandardView value) {
  switch (value) {
  case StandardView::Isometric: m_yaw = -45.0; m_pitch = 30.0; break;
  case StandardView::Front: m_yaw = 0.0; m_pitch = 0.0; break;
  case StandardView::Back: m_yaw = 180.0; m_pitch = 0.0; break;
  case StandardView::Left: m_yaw = -90.0; m_pitch = 0.0; break;
  case StandardView::Right: m_yaw = 90.0; m_pitch = 0.0; break;
  case StandardView::Top: m_yaw = 0.0; m_pitch = 89.999; break;
  case StandardView::Bottom: m_yaw = 0.0; m_pitch = -89.999; break;
  }
}
void ViewportCamera::setOrbitDegrees(qreal yaw, qreal pitch) { m_yaw = yaw; m_pitch = std::clamp(pitch, qreal(-89.999), qreal(89.999)); }
QVector3D ViewportCamera::forward() const {
  const qreal yaw = qDegreesToRadians(m_yaw), pitch = qDegreesToRadians(m_pitch);
  return QVector3D(-qSin(yaw) * qCos(pitch), qSin(pitch), -qCos(yaw) * qCos(pitch)).normalized();
}
QVector3D ViewportCamera::right() const { return QVector3D::crossProduct(forward(), QVector3D(0, 1, 0)).normalized(); }
QVector3D ViewportCamera::up() const { return QVector3D::crossProduct(right(), forward()).normalized(); }
QVector3D ViewportCamera::eye() const { return m_center - forward() * m_distance; }
qreal ViewportCamera::orthographicHalfHeight() const { return m_radius * 1.25; }
QPointF ViewportCamera::project(QVector3D point) const {
  const QVector3D local = point - m_center;
  const qreal x = QVector3D::dotProduct(local, right());
  const qreal y = QVector3D::dotProduct(local, up());
  const qreal aspect = m_viewport.width() / m_viewport.height();
  qreal scale = orthographicHalfHeight();
  if (m_projection == Projection::Perspective) {
    const qreal depth = std::max<qreal>(0.001, QVector3D::dotProduct(point - eye(), forward()));
    scale *= depth / m_distance;
  }
  return {m_viewport.width() * (0.5 + x / (2.0 * scale * aspect)), m_viewport.height() * (0.5 - y / (2.0 * scale))};
}
Ray ViewportCamera::rayForScreenPoint(QPointF point) const {
  const qreal nx = (point.x() / m_viewport.width() - 0.5) * 2.0;
  const qreal ny = (0.5 - point.y() / m_viewport.height()) * 2.0;
  const qreal scale = orthographicHalfHeight(), aspect = m_viewport.width() / m_viewport.height();
  const QVector3D offset = right() * float(nx * scale * aspect) + up() * float(ny * scale);
  if (m_projection == Projection::Orthographic) return {eye() + offset, forward()};
  return {eye(), (forward() * float(m_distance) + offset).normalized()};
}
} // namespace precision::app::viewport
