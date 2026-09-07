#pragma once

#include <QPointF>
#include <QSizeF>
#include <QVector3D>

namespace precision::app::viewport {

enum class Projection { Orthographic, Perspective };
enum class StandardView { Isometric, Front, Back, Left, Right, Top, Bottom };

struct Ray { QVector3D origin; QVector3D direction; };

class ViewportCamera final {
public:
  void setViewport(QSizeF value);
  void setSceneBounds(QVector3D minimum, QVector3D maximum);
  void setProjection(Projection value) { m_projection = value; }
  void setStandardView(StandardView value);
  void setOrbitDegrees(qreal yaw, qreal pitch);
  [[nodiscard]] QPointF project(QVector3D point) const;
  [[nodiscard]] Ray rayForScreenPoint(QPointF point) const;
  [[nodiscard]] QVector3D eye() const;
  [[nodiscard]] QVector3D center() const { return m_center; }
  [[nodiscard]] QVector3D up() const;
  [[nodiscard]] qreal distance() const { return m_distance; }
private:
  [[nodiscard]] QVector3D forward() const;
  [[nodiscard]] QVector3D right() const;
  [[nodiscard]] qreal orthographicHalfHeight() const;
  QSizeF m_viewport{1.0, 1.0};
  QVector3D m_center{};
  qreal m_radius = 1.0;
  qreal m_distance = 3.0;
  qreal m_yaw = -45.0;
  qreal m_pitch = 30.0;
  Projection m_projection = Projection::Perspective;
};

} // namespace precision::app::viewport
