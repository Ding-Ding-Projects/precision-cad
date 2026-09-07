#include "GeometryWorker.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp.hxx>
#include <sstream>
#include <cmath>
#include <limits>

namespace precision::geometry {
namespace {
constexpr int kMaxRequestBytes = 4 * 1024 * 1024;
constexpr int kMaxBrepBytes = 2 * 1024 * 1024;
constexpr int kMaxMeshVertices = 60000;
constexpr int kMaxResponseBytes = 8 * 1024 * 1024;
constexpr double kMaxCoordinate = 1.0e9;

QJsonObject fail(const QJsonObject& identity, const char* code, const QString& detail) {
  QJsonObject response = identity;
  response.insert("ok", false);
  response.insert("error", QJsonObject{{"code", QString::fromLatin1(code)}, {"message", detail}});
  return response;
}

bool finite(double v) { return std::isfinite(v) && std::abs(v) <= kMaxCoordinate; }
bool number(const QJsonValue& value, double& out) { out = value.toDouble(std::numeric_limits<double>::quiet_NaN()); return finite(out); }

bool point(const QJsonValue& value, gp_Pnt& out) {
  const QJsonArray a = value.toArray();
  if (a.size() != 3) return false;
  double x, y, z;
  if (!number(a.at(0), x) || !number(a.at(1), y) || !number(a.at(2), z)) return false;
  out = gp_Pnt(x, y, z); return true;
}

bool vector(const QJsonValue& value, gp_Vec& out) {
  gp_Pnt p;
  if (!point(value, p)) return false;
  out = gp_Vec(p.X(), p.Y(), p.Z());
  return out.SquareMagnitude() > 1.0e-18;
}

TopoDS_Shape readBrep(const QJsonValue& value) {
  const QByteArray encoded = value.toString().toLatin1();
  const QByteArray bytes = QByteArray::fromBase64(encoded);
  if (encoded.isEmpty() || bytes.isEmpty() || bytes.size() > kMaxBrepBytes) throw std::runtime_error("invalid_brep");
  std::istringstream stream(std::string(bytes.constData(), static_cast<size_t>(bytes.size())));
  TopoDS_Shape shape;
  BRep_Builder builder;
  BRepTools::Read(shape, stream, builder);
  if (shape.IsNull()) throw std::runtime_error("invalid_brep");
  return shape;
}

QString writeBrep(const TopoDS_Shape& shape) {
  std::ostringstream stream;
  BRepTools::Write(shape, stream);
  const std::string value = stream.str();
  if (value.empty() || value.size() > static_cast<size_t>(kMaxBrepBytes)) throw std::runtime_error("brep_too_large");
  return QString::fromLatin1(QByteArray(value.data(), static_cast<int>(value.size())).toBase64());
}

TopoDS_Shape operand(const QJsonObject& p, const char* key) { return readBrep(p.value(QLatin1String(key))); }

TopoDS_Shape makeShape(const QString& operation, const QJsonObject& p) {
  if (operation == "box") {
    double dx, dy, dz;
    if (!number(p.value("dx"), dx) || !number(p.value("dy"), dy) || !number(p.value("dz"), dz) || dx <= 0 || dy <= 0 || dz <= 0) throw std::runtime_error("invalid_dimensions");
    gp_Pnt origin(0, 0, 0); if (p.contains("origin") && !point(p.value("origin"), origin)) throw std::runtime_error("invalid_origin");
    return BRepPrimAPI_MakeBox(origin, dx, dy, dz).Shape();
  }
  if (operation == "cylinder") {
    double radius, height;
    if (!number(p.value("radius"), radius) || !number(p.value("height"), height) || radius <= 0 || height <= 0) throw std::runtime_error("invalid_dimensions");
    gp_Pnt origin(0, 0, 0); if (p.contains("origin") && !point(p.value("origin"), origin)) throw std::runtime_error("invalid_origin");
    gp_Vec axis(0, 0, 1); if (p.contains("axis") && !vector(p.value("axis"), axis)) throw std::runtime_error("invalid_axis");
    return BRepPrimAPI_MakeCylinder(gp_Ax2(origin, gp_Dir(axis)), radius, height).Shape();
  }
  if (operation == "extrude") {
    const QJsonArray points = p.value("polygon").toArray(); gp_Vec direction;
    if (points.size() < 3 || points.size() > 4096 || !vector(p.value("vector"), direction)) throw std::runtime_error("invalid_extrude");
    BRepBuilderAPI_MakePolygon polygon;
    double z = 0; bool first = true;
    for (const QJsonValue& entry : points) { gp_Pnt pt; if (!point(entry, pt) || (!first && std::abs(pt.Z() - z) > 1e-7)) throw std::runtime_error("non_planar_polygon"); if (first) { z = pt.Z(); first = false; } polygon.Add(pt); }
    polygon.Close(); if (!polygon.IsDone()) throw std::runtime_error("invalid_polygon");
    return BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(polygon.Wire()).Face(), direction).Shape();
  }
  if (operation == "union" || operation == "cut" || operation == "intersection") {
    const TopoDS_Shape left = operand(p, "leftBrep"); const TopoDS_Shape right = operand(p, "rightBrep");
    if (operation == "union") return BRepAlgoAPI_Fuse(left, right).Shape();
    if (operation == "cut") return BRepAlgoAPI_Cut(left, right).Shape();
    return BRepAlgoAPI_Common(left, right).Shape();
  }
  if (operation == "translate" || operation == "rotate") {
    TopoDS_Shape shape = operand(p, "brep"); gp_Trsf transform;
    if (operation == "translate") { gp_Vec delta; if (!vector(p.value("vector"), delta)) throw std::runtime_error("invalid_vector"); transform.SetTranslation(delta); }
    else { gp_Pnt origin; gp_Vec axis; double radians; if (!point(p.value("origin"), origin) || !vector(p.value("axis"), axis) || !number(p.value("radians"), radians) || std::abs(radians) > 1000.0) throw std::runtime_error("invalid_rotation"); transform.SetRotation(gp_Ax1(origin, gp_Dir(axis)), radians); }
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
  }
  if (operation == "fillet") {
    TopoDS_Shape shape = operand(p, "brep"); double radius; if (!number(p.value("radius"), radius) || radius <= 0) throw std::runtime_error("invalid_radius");
    BRepFilletAPI_MakeFillet builder(shape); for (TopExp_Explorer it(shape, TopAbs_EDGE); it.More(); it.Next()) builder.Add(radius, TopoDS::Edge(it.Current())); builder.Build(); if (!builder.IsDone()) throw std::runtime_error("fillet_failed"); return builder.Shape();
  }
  if (operation == "validate" || operation == "tessellate") return operand(p, "brep");
  throw std::runtime_error("unknown_operation");
}

QJsonObject describe(const TopoDS_Shape& shape, bool meshRequested) {
  if (shape.IsNull()) throw std::runtime_error("null_shape");
  BRepCheck_Analyzer checker(shape, true); if (!checker.IsValid()) throw std::runtime_error("invalid_geometry");
  GProp_GProps properties; BRepGProp::VolumeProperties(shape, properties);
  Bnd_Box bounds; BRepBndLib::Add(shape, bounds); Standard_Real xmin, ymin, zmin, xmax, ymax, zmax; bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
  QJsonObject result{{"brep", writeBrep(shape)}, {"valid", true}, {"volume", properties.Mass()}, {"bounds", QJsonArray{xmin, ymin, zmin, xmax, ymax, zmax}}};
  if (!meshRequested) return result;
  BRepMesh_IncrementalMesh mesher(shape, 0.1, false, 0.5, true); if (!mesher.IsDone()) throw std::runtime_error("tessellation_failed");
  QJsonArray vertices, normals, indices; int offset = 0;
  for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) { TopLoc_Location loc; Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(TopoDS::Face(it.Current()), loc); if (tri.IsNull()) continue; const gp_Trsf trsf = loc.Transformation(); if (offset + tri->NbNodes() > kMaxMeshVertices) throw std::runtime_error("mesh_too_large"); gp_Vec faceNormal(0, 0, 1); if (tri->NbTriangles() > 0) { Poly_Triangle first = tri->Triangle(1); int a,b,c; first.Get(a,b,c); const gp_Pnt pa = tri->Node(a).Transformed(trsf), pb = tri->Node(b).Transformed(trsf), pc = tri->Node(c).Transformed(trsf); faceNormal = gp_Vec(pa, pb).Crossed(gp_Vec(pa, pc)); if (faceNormal.SquareMagnitude() > 1e-20) faceNormal.Normalize(); else faceNormal = gp_Vec(0, 0, 1); } for (int n = 1; n <= tri->NbNodes(); ++n) { gp_Pnt p = tri->Node(n).Transformed(trsf); vertices.append(p.X()); vertices.append(p.Y()); vertices.append(p.Z()); normals.append(faceNormal.X()); normals.append(faceNormal.Y()); normals.append(faceNormal.Z()); } for (int t = 1; t <= tri->NbTriangles(); ++t) { Poly_Triangle triangle = tri->Triangle(t); int a,b,c; triangle.Get(a,b,c); indices.append(offset+a-1); indices.append(offset+b-1); indices.append(offset+c-1); } offset += tri->NbNodes(); }
  result.insert("mesh", QJsonObject{{"vertices", vertices}, {"normals", normals}, {"indices", indices}}); return result;
}
} // namespace

QJsonObject executeRequest(const QJsonObject& request) {
  QJsonObject identity{{"protocolVersion", kProtocolVersion}, {"operationId", request.value("operationId")}, {"documentId", request.value("documentId")}, {"revision", request.value("revision")}};
  if (QJsonDocument(request).toJson(QJsonDocument::Compact).size() > kMaxRequestBytes) return fail(identity, "request_too_large", "Request exceeds the worker limit.");
  if (request.value("protocolVersion").toInt() != kProtocolVersion || identity.value("operationId").toString().isEmpty() || identity.value("documentId").toString().isEmpty() || !request.value("revision").isDouble()) return fail(identity, "invalid_envelope", "Protocol identity is incomplete or unsupported.");
  try { const QString operation = request.value("operation").toString(); if (operation.isEmpty()) return fail(identity, "invalid_operation", "Operation is required."); const TopoDS_Shape shape = makeShape(operation, request.value("parameters").toObject()); QJsonObject response = identity; response.insert("ok", true); response.insert("result", describe(shape, operation != "validate")); if (QJsonDocument(response).toJson(QJsonDocument::Compact).size() > kMaxResponseBytes) return fail(identity, "response_too_large", "Result exceeds the worker limit."); return response; }
  catch (const Standard_Failure& error) { return fail(identity, "kernel_error", QString::fromLatin1(error.GetMessageString())); }
  catch (const std::exception& error) { return fail(identity, "geometry_error", QString::fromLatin1(error.what())); }
}
} // namespace precision::geometry
