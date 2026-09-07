#include "viewport_camera.h"
#include "viewport_picker.h"
#include <qqml.h>
#include <QtMath>
#include <algorithm>
#include <cmath>
namespace precision::app::viewport {
namespace { const int registration=qmlRegisterType<ViewportCamera>("PrecisionCad",1,0,"ViewportCamera"); }
void ViewportCamera::setViewport(QSizeF v) { if(!std::isfinite(v.width())||!std::isfinite(v.height()))return; v={std::max(1.,v.width()),std::max(1.,v.height())}; if(v==m_viewport)return; m_viewport=v; emit changed(); }
void ViewportCamera::setSceneBounds(QVector3D lo,QVector3D hi) { m_boundsCenter=(lo+hi)*.5f; m_radius=std::max(.001,double((hi-lo).length())*.5); fit(); }
void ViewportCamera::setGeometry(MeshGeometry *value) {
  if(m_geometry==value)return;
  if(m_geometry)disconnect(m_geometry,nullptr,this,nullptr);
  m_geometry=value;
  const auto refresh=[this] { m_selection.clear(); emit selectionChanged(); if(m_geometry&&m_geometry->valid())setSceneBounds(m_geometry->boundsMin(),m_geometry->boundsMax()); else emit changed(); };
  if(value)connect(value,&MeshGeometry::meshChanged,this,refresh);
  refresh(); emit geometryChanged();
}
void ViewportCamera::setProjection(Projection value) { if(m_projection==value)return; m_projection=value; emit changed(); }
void ViewportCamera::fit() {
  m_center=m_boundsCenter;
  const double angle=std::atan(std::tan(qDegreesToRadians(fieldOfView()*.5))*std::min(1.,m_viewport.width()/m_viewport.height()));
  m_distance=m_radius/std::sin(angle)*1.15; emit changed();
}
void ViewportCamera::setStandardView(StandardView value) {
  switch(value) {
  case StandardView::Isometric:m_yaw=-45;m_pitch=-35.26438968;break;
  case StandardView::Front:m_yaw=0;m_pitch=0;break;
  case StandardView::Back:m_yaw=180;m_pitch=0;break;
  case StandardView::Left:m_yaw=-90;m_pitch=0;break;
  case StandardView::Right:m_yaw=90;m_pitch=0;break;
  case StandardView::Top:m_yaw=0;m_pitch=-89.999;break;
  case StandardView::Bottom:m_yaw=0;m_pitch=89.999;break;
  }
  emit changed();
}
void ViewportCamera::standardView(int value) { if(value>=0&&value<=6)setStandardView(static_cast<StandardView>(value)); }
void ViewportCamera::setOrbitDegrees(qreal yaw,qreal pitch) { if(!std::isfinite(yaw)||!std::isfinite(pitch))return; m_yaw=std::remainder(yaw,360.);m_pitch=std::clamp(pitch,-89.999,89.999);emit changed(); }
void ViewportCamera::orbit(qreal dx,qreal dy) { setOrbitDegrees(m_yaw+dx*.45,m_pitch+dy*.45); }
void ViewportCamera::pan(qreal dx,qreal dy) { if(!std::isfinite(dx)||!std::isfinite(dy))return; const double units=2*halfHeight()/m_viewport.height(); m_center+=right()*float(-dx*units)+up()*float(dy*units);emit changed(); }
void ViewportCamera::zoomBy(qreal factor) { if(!std::isfinite(factor)||factor<=0)return; m_distance=std::clamp(m_distance*factor,m_radius*.02,m_radius*10000);emit changed(); }
QVector3D ViewportCamera::forward() const { const double y=qDegreesToRadians(m_yaw),p=qDegreesToRadians(m_pitch); return QVector3D(-std::sin(y)*std::cos(p),std::sin(p),-std::cos(y)*std::cos(p)).normalized(); }
QVector3D ViewportCamera::right() const { return QVector3D::crossProduct(forward(),{0,1,0}).normalized(); }
QVector3D ViewportCamera::up() const { return QVector3D::crossProduct(right(),forward()).normalized(); }
QVector3D ViewportCamera::eye() const { return m_center-forward()*float(m_distance); }
QQuaternion ViewportCamera::orientation() const { return QQuaternion::fromDirection(-forward(),up()); }
qreal ViewportCamera::halfHeight() const { return m_distance*std::tan(qDegreesToRadians(fieldOfView()*.5)); }
QPointF ViewportCamera::project(QVector3D point) const {
  const auto local=point-m_center;
  double scale=halfHeight();
  if(perspective())scale=QVector3D::dotProduct(point-eye(),forward())*std::tan(qDegreesToRadians(fieldOfView()*.5));
  if(scale<=0)return {NAN,NAN};
  return {m_viewport.width()*.5+QVector3D::dotProduct(local,right())*m_viewport.height()/(2*scale),m_viewport.height()*.5-QVector3D::dotProduct(local,up())*m_viewport.height()/(2*scale)};
}
Ray ViewportCamera::rayForScreenPoint(QPointF point) const {
  const auto offset=right()*float((point.x()-m_viewport.width()*.5)*2*halfHeight()/m_viewport.height())+up()*float((m_viewport.height()*.5-point.y())*2*halfHeight()/m_viewport.height());
  return perspective()?Ray{eye(),(forward()*float(m_distance)+offset).normalized()}:Ray{eye()+offset,forward()};
}
QVariantMap ViewportCamera::pick(qreal x,qreal y) {
  QVariantMap result;
  if(m_geometry&&m_geometry->valid()&&std::isfinite(x)&&std::isfinite(y)&&x>=0&&y>=0&&x<=m_viewport.width()&&y<=m_viewport.height()) {
    const Ray ray=rayForScreenPoint({x,y});
    const auto hit=ViewportPicker::pick({m_geometry->localPositions(),m_geometry->validatedIndices()},ray,0);
    if(hit.kind!=PickKind::None && hit.distance>=clipNear() && hit.distance<=clipFar()) result={{"triangleIndex",hit.triangleIndex},{"bodyId",m_geometry->bodyForTriangle(hit.triangleIndex)},{"position",m_geometry->sourcePoint(ray.origin+ray.direction*hit.distance)},{"kind","meshTriangle"}};
  }
  if(result!=m_selection) { m_selection=result;emit selectionChanged(); }
  return result;
}
}
