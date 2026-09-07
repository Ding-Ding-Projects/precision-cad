#include "mesh_geometry.h"
#include <QByteArray>
#include <QDataStream>
#include <QVector3D>
#include <qqml.h>

namespace precision::app::viewport {
namespace { const int meshGeometryRegistration = qmlRegisterType<MeshGeometry>("PrecisionCad", 1, 0, "MeshGeometry"); }
MeshGeometry::MeshGeometry(QQuick3DObject *parent) : QQuick3DGeometry(parent) {
  addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0, QQuick3DGeometry::Attribute::F32Type);
  addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, 12, QQuick3DGeometry::Attribute::F32Type);
  addAttribute(QQuick3DGeometry::Attribute::IndexSemantic, 0, QQuick3DGeometry::Attribute::U32Type);
  setStride(24); setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
}
void MeshGeometry::setVertices(const QVariantList &value) { if(m_vertexValues==value)return; m_vertexValues=value; rebuild(); emit meshChanged(); }
void MeshGeometry::setIndices(const QVariantList &value) { if(m_indexValues==value)return; m_indexValues=value; rebuild(); emit meshChanged(); }
void MeshGeometry::setNormals(const QVariantList &value) { if(m_normalValues==value)return; m_normalValues=value; rebuild(); emit meshChanged(); }
void MeshGeometry::rebuild() {
  const int count=m_vertexValues.size()/3; if(count<=0 || m_indexValues.size()<3 || m_vertexValues.size()%3 || m_indexValues.size()%3){setVertexData({});setIndexData({});return;}
  QVector<QVector3D> positions; positions.reserve(count); for(int i=0;i<count;++i)positions.append({m_vertexValues[3*i].toFloat(),m_vertexValues[3*i+1].toFloat(),m_vertexValues[3*i+2].toFloat()});
  QVector<QVector3D> normals(count); if(m_normalValues.size()==m_vertexValues.size()) for(int i=0;i<count;++i) normals[i]={m_normalValues[3*i].toFloat(),m_normalValues[3*i+1].toFloat(),m_normalValues[3*i+2].toFloat()};
  else { for(int i=0;i+2<m_indexValues.size();i+=3){const int a=m_indexValues[i].toInt(),b=m_indexValues[i+1].toInt(),c=m_indexValues[i+2].toInt();if(a<0||b<0||c<0||a>=count||b>=count||c>=count)continue;const auto n=QVector3D::crossProduct(positions[b]-positions[a],positions[c]-positions[a]);normals[a]+=n;normals[b]+=n;normals[c]+=n;} for(auto &n:normals)n=n.lengthSquared()>0?n.normalized():QVector3D(0,0,1); }
  QByteArray vertexBytes; vertexBytes.resize(count*24); char *out=vertexBytes.data(); for(int i=0;i<count;++i){float values[]={positions[i].x(),positions[i].y(),positions[i].z(),normals[i].x(),normals[i].y(),normals[i].z()}; memcpy(out+i*24,values,24);} QByteArray indexBytes; indexBytes.resize(m_indexValues.size()*4); auto *ids=reinterpret_cast<quint32*>(indexBytes.data()); for(int i=0;i<m_indexValues.size();++i)ids[i]=quint32(qMax(0,m_indexValues[i].toInt())); setVertexData(vertexBytes);setIndexData(indexBytes);
}
} // namespace precision::app::viewport
