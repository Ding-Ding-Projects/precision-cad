#include "mesh_geometry.h"
#include <qqml.h>
#include <QScopeGuard>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace precision::app::viewport {
namespace { const int registration=qmlRegisterType<MeshGeometry>("PrecisionCad",1,0,"MeshGeometry"); }
MeshGeometry::MeshGeometry(QQuick3DObject *parent): QQuick3DGeometry(parent) { rebuild(); }
void MeshGeometry::setParts(const QVariantList &v) { if(m_parts==v)return; m_parts=v; rebuild(); emit meshChanged(); }
QString MeshGeometry::bodyForTriangle(int index) const { for(const auto &part:m_ranges) if(index>=part.first && index-part.first<part.count)return part.bodyId; return m_parts.isEmpty()?m_fallbackBodyId:QString(); }
void MeshGeometry::setVertices(const QVariantList &v) { if(m_vertexValues==v)return; m_vertexValues=v; if(m_parts.isEmpty())rebuild(); emit meshChanged(); }
void MeshGeometry::setIndices(const QVariantList &v) { if(m_indexValues==v)return; m_indexValues=v; if(m_parts.isEmpty())rebuild(); emit meshChanged(); }
void MeshGeometry::setNormals(const QVariantList &v) { if(m_normalValues==v)return; m_normalValues=v; if(m_parts.isEmpty())rebuild(); emit meshChanged(); }
void MeshGeometry::setFallbackBodyId(const QString &value) { if(value==m_fallbackBodyId)return; m_fallbackBodyId=value;updateColors(); }
void MeshGeometry::setSelectedBodyId(const QString &value) { if(value==m_selectedBodyId)return; m_selectedBodyId=value;updateColors(); }
void MeshGeometry::setBaseColor(QColor value) { if(!value.isValid()||value==m_baseColor)return;m_baseColor=value;updateColors(); }
void MeshGeometry::setSelectionColor(QColor value) { if(!value.isValid()||value==m_selectionColor)return;m_selectionColor=value;updateColors(); }
void MeshGeometry::updateColors() {
  m_selectedTriangleCount=0;
  if(m_valid) {
    QVector<bool> selected(m_positions.size(),false);
    for(int triangle=0;triangle<m_indices.size()/3;++triangle) if(!m_selectedBodyId.isEmpty()&&bodyForTriangle(triangle)==m_selectedBodyId) {
      ++m_selectedTriangleCount;for(int corner=0;corner<3;++corner)selected[m_indices[3*triangle+corner]]=true;
    }
    QByteArray data=vertexData();
    for(qsizetype i=0;i<m_positions.size();++i) { const QColor color=selected[i]?m_selectionColor:m_baseColor; const float rgba[]={float(color.redF()),float(color.greenF()),float(color.blueF()),1.f}; std::memcpy(data.data()+i*40+24,rgba,16); }
    setVertexData(data);update();
  }
  emit appearanceChanged();
}
QVariantList MeshGeometry::sourcePoint(QVector3D p) const { return {m_origin[0]+double(p.x())/m_scale,m_origin[1]+double(p.y())/m_scale,m_origin[2]+double(p.z())/m_scale}; }
void MeshGeometry::rebuild() {
  const auto notify=qScopeGuard([this] { updateColors(); emit sceneChanged(); });
  clear(); m_valid=false; m_suppliedNormals=false; m_positions.clear(); m_indices.clear();
  setBounds({},{}); update();
  QVariantList vertices=m_vertexValues,indices=m_indexValues,normalValues=m_normalValues;
  m_ranges.clear();
  if(!m_parts.isEmpty()) {
    vertices.clear(); indices.clear(); normalValues.clear(); bool allNormals=true;
    for(const auto &entry:m_parts) {
      const auto part=entry.toMap(); const auto pv=part.value("vertices").toList(),pi=part.value("indices").toList(),pn=part.value("normals").toList();
      if(part.value("bodyId").toString().isEmpty()||pv.isEmpty()||pv.size()%3||pi.isEmpty()||pi.size()%3)return;
      const qsizetype base=vertices.size()/3;
      m_ranges.append({part.value("bodyId").toString(),int(indices.size()/3),int(pi.size()/3)});
      for(const auto &v:pi) { bool ok=false; const double n=v.toDouble(&ok); if(!ok||!std::isfinite(n)||n<0||n>=pv.size()/3||std::floor(n)!=n)return; indices.append(n+base); }
      vertices.append(pv); normalValues.append(pn); allNormals=allNormals&&(pn.size()==pv.size());
    }
    if(!allNormals)normalValues.clear();
  }
  const qsizetype count=vertices.size()/3;
  if(count==0 || vertices.size()%3 || indices.isEmpty() || indices.size()%3) return;
  std::array<double,3> lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
  QVector<double> coordinates; coordinates.reserve(vertices.size());
  for(qsizetype i=0;i<vertices.size();++i) {
    bool ok=false; const double x=vertices[i].toDouble(&ok);
    if(!ok || !std::isfinite(x)) return;
    coordinates.append(x); lo[i%3]=std::min(lo[i%3],x); hi[i%3]=std::max(hi[i%3],x);
  }
  for(const auto &v:indices) {
    bool ok=false; const double index=v.toDouble(&ok);
    if(!ok || !std::isfinite(index) || index<0 || index>=count || std::floor(index)!=index) { m_indices.clear(); return; }
    m_indices.append(quint32(index));
  }
  // Subtract the double-precision origin before the only float conversion.
  for(int i=0;i<3;++i) m_origin[i]=lo[i]*.5+hi[i]*.5;
  const double radius=std::hypot((hi[0]*.5-lo[0]*.5),(hi[1]*.5-lo[1]*.5),(hi[2]*.5-lo[2]*.5));
  if(!std::isfinite(radius) || radius<=0) { m_indices.clear(); return; }
  m_scale=100.0/radius;
  if(!std::isfinite(m_scale) || m_scale<=0) { m_indices.clear(); return; }
  QVector3D minimum(100,100,100),maximum(-100,-100,-100);
  for(qsizetype i=0;i<count;++i) {
    QVector3D p;
    for(int axis=0;axis<3;++axis) {
      const double local=(coordinates[3*i+axis]-m_origin[axis])*m_scale;
      if(!std::isfinite(local)) { m_positions.clear(); m_indices.clear(); return; }
      p[axis]=float(local); minimum[axis]=std::min(minimum[axis],p[axis]); maximum[axis]=std::max(maximum[axis],p[axis]);
    }
    m_positions.append(p);
  }
  QVector<QVector3D> normals(count); m_suppliedNormals=normalValues.size()==vertices.size();
  if(m_suppliedNormals) for(qsizetype i=0;i<count;++i) {
    for(int axis=0;axis<3;++axis) { bool ok=false; const double n=normalValues[3*i+axis].toDouble(&ok); if(!ok||!std::isfinite(n)||std::abs(n)>1e20) { m_suppliedNormals=false; break; } normals[i][axis]=float(n); }
    if(!m_suppliedNormals || normals[i].lengthSquared()<1e-12f) { m_suppliedNormals=false; break; }
    normals[i].normalize();
  }
  if(!m_suppliedNormals) {
    normals.fill({});
    for(qsizetype i=0;i<m_indices.size();i+=3) { const auto a=m_indices[i],b=m_indices[i+1],c=m_indices[i+2]; const auto n=QVector3D::crossProduct(m_positions[b]-m_positions[a],m_positions[c]-m_positions[a]); normals[a]+=n; normals[b]+=n; normals[c]+=n; }
    for(auto &n:normals) n=n.lengthSquared()>1e-12f?n.normalized():QVector3D(0,0,1);
  }
  QByteArray data; data.resize(count*40);
  for(qsizetype i=0;i<count;++i) { const float v[]={m_positions[i].x(),m_positions[i].y(),m_positions[i].z(),normals[i].x(),normals[i].y(),normals[i].z()}; std::memcpy(data.data()+i*40,v,24); }
  QByteArray indexData; indexData.resize(m_indices.size()*sizeof(quint32)); std::memcpy(indexData.data(),m_indices.constData(),indexData.size());
  setStride(40); setPrimitiveType(PrimitiveType::Triangles);
  addAttribute(Attribute::PositionSemantic,0,Attribute::F32Type); addAttribute(Attribute::NormalSemantic,12,Attribute::F32Type); addAttribute(Attribute::IndexSemantic,0,Attribute::U32Type); addAttribute(Attribute::ColorSemantic,24,Attribute::F32Type);
  setVertexData(data); setIndexData(indexData); setBounds(minimum,maximum); m_valid=true; update();
}
}
