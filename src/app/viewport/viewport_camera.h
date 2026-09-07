#pragma once
#include <QObject>
#include <QPointF>
#include <QSizeF>
#include <QVector3D>
#include <QQuaternion>
#include <QPointer>
#include <QVariantMap>
#include "mesh_geometry.h"
namespace precision::app::viewport {
enum class Projection { Orthographic, Perspective };
enum class StandardView { Isometric, Front, Back, Left, Right, Top, Bottom };
struct Ray { QVector3D origin; QVector3D direction; };
class ViewportCamera : public QObject {
  Q_OBJECT
  Q_PROPERTY(QSizeF viewportSize READ viewportSize WRITE setViewport NOTIFY changed)
  Q_PROPERTY(MeshGeometry* geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)
  Q_PROPERTY(QVector3D position READ eye NOTIFY changed)
  Q_PROPERTY(QQuaternion orientation READ orientation NOTIFY changed)
  Q_PROPERTY(bool perspective READ perspective WRITE setPerspective NOTIFY changed)
  Q_PROPERTY(qreal magnification READ magnification NOTIFY changed)
  Q_PROPERTY(qreal clipNear READ clipNear NOTIFY changed)
  Q_PROPERTY(qreal clipFar READ clipFar NOTIFY changed)
  Q_PROPERTY(qreal fieldOfView READ fieldOfView CONSTANT)
  Q_PROPERTY(QVariantMap selection READ selection NOTIFY selectionChanged)
public:
  explicit ViewportCamera(QObject *parent=nullptr):QObject(parent){}
  QSizeF viewportSize() const { return m_viewport; }
  void setViewport(QSizeF value);
  void setSceneBounds(QVector3D minimum,QVector3D maximum);
  void setProjection(Projection value);
  void setStandardView(StandardView value);
  void setOrbitDegrees(qreal yaw,qreal pitch);
  Q_INVOKABLE void standardView(int value);
  Q_INVOKABLE void orbit(qreal dx,qreal dy);
  Q_INVOKABLE void pan(qreal dx,qreal dy);
  Q_INVOKABLE void zoomBy(qreal factor);
  Q_INVOKABLE void fit();
  Q_INVOKABLE QVariantMap pick(qreal x,qreal y);
  Q_INVOKABLE QPointF project(QVector3D point) const;
  Ray rayForScreenPoint(QPointF point) const;
  QVector3D eye() const;
  QVector3D center() const { return m_center; }
  QVector3D up() const;
  qreal distance() const { return m_distance; }
  QQuaternion orientation() const;
  bool perspective() const { return m_projection==Projection::Perspective; }
  void setPerspective(bool value) { setProjection(value?Projection::Perspective:Projection::Orthographic); }
  qreal magnification() const { return m_viewport.height()/(2*halfHeight()); }
  qreal clipNear() const { return 0.001; }
  qreal clipFar() const { return m_distance+(m_center-m_boundsCenter).length()+m_radius*4+1; }
  qreal fieldOfView() const { return 45; }
  MeshGeometry *geometry() const { return m_geometry; }
  void setGeometry(MeshGeometry *value);
  QVariantMap selection() const { return m_selection; }
signals:
  void changed();
  void geometryChanged();
  void selectionChanged();
private:
  QVector3D forward() const;
  QVector3D right() const;
  qreal halfHeight() const;
  QSizeF m_viewport{1,1};
  QVector3D m_center{},m_boundsCenter{};
  qreal m_radius=100,m_distance=350,m_yaw=-45,m_pitch=-30;
  Projection m_projection=Projection::Perspective;
  QPointer<MeshGeometry> m_geometry;
  QVariantMap m_selection;
};
}
