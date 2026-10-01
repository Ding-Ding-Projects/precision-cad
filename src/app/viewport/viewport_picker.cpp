#include "viewport_picker.h"
#include <limits>

namespace precision::app::viewport {
namespace {
bool rayTriangle(const Ray &ray, const QVector3D &a, const QVector3D &b, const QVector3D &c, float *distance) {
  const QVector3D e1=b-a, e2=c-a, p=QVector3D::crossProduct(ray.direction,e2); const float det=QVector3D::dotProduct(e1,p);
  if (qAbs(det) < 1e-7f) return false; const float inv=1.0f/det; const QVector3D t=ray.origin-a; const float u=QVector3D::dotProduct(t,p)*inv;
  if(u < 0 || u > 1) return false; const QVector3D q=QVector3D::crossProduct(t,e1); const float v=QVector3D::dotProduct(ray.direction,q)*inv;
  if(v < 0 || u+v > 1) return false; const float d=QVector3D::dotProduct(e2,q)*inv; if(d <= 1e-6f) return false; *distance=d; return true;
}
float pointRayDistance(const QVector3D &p, const Ray &ray, float *at) { const float t=QVector3D::dotProduct(p-ray.origin,ray.direction); if(t<=0) return std::numeric_limits<float>::infinity(); *at=t; return (p-(ray.origin+ray.direction*t)).length(); }
float segmentRayDistance(const QVector3D &a, const QVector3D &b, const Ray &ray, float *rayT) { const QVector3D u=b-a,w=a-ray.origin; const float uu=QVector3D::dotProduct(u,u), uv=QVector3D::dotProduct(u,ray.direction), uw=QVector3D::dotProduct(u,w), vw=QVector3D::dotProduct(ray.direction,w); const float denom=uu-uv*uv; float s=denom>1e-7f ? std::clamp((uv*vw-uw)/denom,0.0f,1.0f) : 0.0f; const float t=QVector3D::dotProduct((a+u*s)-ray.origin,ray.direction); if(t<=0) return std::numeric_limits<float>::infinity(); *rayT=t; return ((a+u*s)-(ray.origin+ray.direction*t)).length(); }
}
PickResult ViewportPicker::pick(const MeshData &mesh, const Ray &inputRay, float tolerance) {
  if(mesh.indices.size()%3 || inputRay.direction.lengthSquared()<1e-10f || tolerance<0) return {}; Ray ray{inputRay.origin,inputRay.direction.normalized()}; PickResult closest; float best=std::numeric_limits<float>::infinity();
  for(int i=0;i<mesh.indices.size();i+=3){ const auto ia=mesh.indices[i],ib=mesh.indices[i+1],ic=mesh.indices[i+2]; if(ia>=quint32(mesh.vertices.size())||ib>=quint32(mesh.vertices.size())||ic>=quint32(mesh.vertices.size())) return {}; float d; if(rayTriangle(ray,mesh.vertices[ia],mesh.vertices[ib],mesh.vertices[ic],&d)&&d<best){best=d;closest={PickKind::Face,i/3,-1,-1,d};}}
  if(closest.kind==PickKind::None) return closest;
  const int tri=closest.triangleIndex*3; const quint32 ids[]={mesh.indices[tri],mesh.indices[tri+1],mesh.indices[tri+2]};
  for(int i=0;i<3;++i){float d; if(pointRayDistance(mesh.vertices[ids[i]],ray,&d)<=tolerance && d<=best+tolerance) return {PickKind::Vertex,closest.triangleIndex,int(ids[i]),-1,d};}
  for(int i=0;i<3;++i){float d; if(segmentRayDistance(mesh.vertices[ids[i]],mesh.vertices[ids[(i+1)%3]],ray,&d)<=tolerance && d<=best+tolerance) return {PickKind::Edge,closest.triangleIndex,-1,i,d};}
  return closest;
}
} // namespace precision::app::viewport
