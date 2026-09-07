#include "GeometryWorker.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Geom_Surface.hxx>
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
#include <Precision.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <sstream>
#include <cmath>
#include <limits>

namespace precision::geometry {
namespace {
constexpr int kMaxRequestBytes = 4 * 1024 * 1024;
constexpr int kMaxBrepBytes = 2 * 1024 * 1024;
constexpr int kMaxMeshVertices = 60000;
constexpr int kMaxMeshTriangles = 100000;
constexpr int kMaxTopologyFaces = 5000;
constexpr int kMaxTopologyEdges = 15000;
constexpr int kMaxTopologyVertices = 15000;
constexpr int kMaxPolylinePoints = 64;
constexpr int kMaxResponseBytes = 8 * 1024 * 1024;
constexpr double kMaxCoordinate = 1.0e9;

QJsonObject fail(const QJsonObject& identity, const char* code, const QString& detail) {
  QJsonObject response = identity;
  response.insert("ok", false);
  response.insert("error", QJsonObject{{"code", QString::fromLatin1(code)}, {"message", detail}});
  return response;
}

bool finite(double v) { return std::isfinite(v); }
bool coordinate(double v) { return finite(v) && std::abs(v) <= kMaxCoordinate; }
bool number(const QJsonValue& value, double& out) { out = value.toDouble(std::numeric_limits<double>::quiet_NaN()); return coordinate(out); }
bool uuid(const QString& value) { static const QRegularExpression pattern(QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-5][0-9a-fA-F]{3}-[89aAbB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}$")); return pattern.match(value).hasMatch(); }

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
  return true;
}

bool nonZeroVector(const QJsonValue& value, gp_Vec& out) {
  return vector(value, out) && out.SquareMagnitude() > 1.0e-18;
}

TopoDS_Shape readBrep(const QJsonValue& value) {
  if (!value.isString()) throw std::runtime_error("invalid_brep");
  const QByteArray encoded = value.toString().toLatin1();
  const auto decoded = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
  const QByteArray bytes = decoded.decoded;
  if (encoded.isEmpty() || decoded.decodingStatus != QByteArray::Base64DecodingStatus::Ok || bytes.isEmpty() || bytes.size() > kMaxBrepBytes) throw std::runtime_error("invalid_brep");
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
QString primitiveEdgeKey(const QString& operation, const TopoDS_Edge& edge, const Bnd_Box& bounds);

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
    gp_Vec axis(0, 0, 1); if (p.contains("axis") && !nonZeroVector(p.value("axis"), axis)) throw std::runtime_error("invalid_axis");
    return BRepPrimAPI_MakeCylinder(gp_Ax2(origin, gp_Dir(axis)), radius, height).Shape();
  }
  if (operation == "extrude") {
    const QJsonArray points = p.value("polygon").toArray(); gp_Vec direction;
    if (points.size() < 3 || points.size() > 4096 || !nonZeroVector(p.value("vector"), direction)) throw std::runtime_error("invalid_extrude");
    BRepBuilderAPI_MakePolygon polygon;
    double z = 0; bool first = true;
    for (const QJsonValue& entry : points) { gp_Pnt pt; if (!point(entry, pt) || (!first && std::abs(pt.Z() - z) > 1e-7)) throw std::runtime_error("non_planar_polygon"); if (first) { z = pt.Z(); first = false; } polygon.Add(pt); }
    polygon.Close(); if (!polygon.IsDone()) throw std::runtime_error("invalid_polygon");
    return BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(polygon.Wire()).Face(), direction).Shape();
  }
  if (operation == "union" || operation == "cut" || operation == "intersection") {
    const TopoDS_Shape left = operand(p, "leftBrep"); const TopoDS_Shape right = operand(p, "rightBrep");
    if (operation == "union") { BRepAlgoAPI_Fuse algorithm(left, right); if (!algorithm.IsDone() || algorithm.HasErrors()) throw std::runtime_error("boolean_failed"); return algorithm.Shape(); }
    if (operation == "cut") { BRepAlgoAPI_Cut algorithm(left, right); if (!algorithm.IsDone() || algorithm.HasErrors()) throw std::runtime_error("boolean_failed"); return algorithm.Shape(); }
    BRepAlgoAPI_Common algorithm(left, right); if (!algorithm.IsDone() || algorithm.HasErrors()) throw std::runtime_error("boolean_failed"); return algorithm.Shape();
  }
  if (operation == "translate" || operation == "rotate") {
    TopoDS_Shape shape = operand(p, "brep"); gp_Trsf transform;
    if (operation == "translate") { gp_Vec delta; if (!vector(p.value("vector"), delta)) throw std::runtime_error("invalid_vector"); transform.SetTranslation(delta); }
    else { gp_Pnt origin; gp_Vec axis; double radians; if (!point(p.value("origin"), origin) || !nonZeroVector(p.value("axis"), axis) || !number(p.value("radians"), radians) || std::abs(radians) > 1000.0) throw std::runtime_error("invalid_rotation"); transform.SetRotation(gp_Ax1(origin, gp_Dir(axis)), radians); }
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
  }
  if (operation == "fillet") {
    TopoDS_Shape shape = operand(p, "brep"); double radius; if (!number(p.value("radius"), radius) || radius <= 0) throw std::runtime_error("invalid_radius");
    const QJsonObject topologyInput = p.value("topologyInput").toObject(); const QString sourceFeatureId = topologyInput.value("sourceFeatureId").toString(); const QJsonArray refs = topologyInput.value("edgeRefs").toArray();
    if (!uuid(sourceFeatureId) || refs.isEmpty() || refs.size() > kMaxTopologyEdges) throw std::runtime_error("invalid_topology_reference");
    Bnd_Box bounds; BRepBndLib::Add(shape, bounds); QHash<QString, TopoDS_Edge> candidates;
    for (TopExp_Explorer it(shape, TopAbs_EDGE); it.More(); it.Next()) { const TopoDS_Edge edge = TopoDS::Edge(it.Current()); const QString key = primitiveEdgeKey("box", edge, bounds); if (!key.isEmpty()) candidates.insert(key, edge); }
    BRepFilletAPI_MakeFillet builder(shape); QSet<QString> selected;
    for (const QJsonValue& value : refs) { const QJsonObject ref = value.toObject(); const QString key = ref.value("key").toString(); if (ref.value("featureId") != sourceFeatureId || ref.value("entityKind") != "edge" || key.isEmpty() || !candidates.contains(key) || selected.contains(key)) throw std::runtime_error("invalid_topology_reference"); selected.insert(key); builder.Add(radius, candidates.value(key)); }
    builder.Build(); if (!builder.IsDone()) throw std::runtime_error("fillet_failed"); return builder.Shape();
  }
  if (operation == "validate" || operation == "tessellate") return operand(p, "brep");
  throw std::runtime_error("unknown_operation");
}

QJsonArray topologyPoint(const gp_Pnt& p) {
  if (!coordinate(p.X()) || !coordinate(p.Y()) || !coordinate(p.Z())) throw std::runtime_error("non_finite_geometry");
  return QJsonArray{p.X(), p.Y(), p.Z()};
}

QJsonObject topologySignature(const TopoDS_Shape& shape, const char* kind) {
  Bnd_Box box; BRepBndLib::Add(shape, box); Standard_Real xmin, ymin, zmin, xmax, ymax, zmax; box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
  if (!coordinate(xmin) || !coordinate(ymin) || !coordinate(zmin) || !coordinate(xmax) || !coordinate(ymax) || !coordinate(zmax)) throw std::runtime_error("non_finite_geometry");
  return QJsonObject{{"kind", QString::fromLatin1(kind)}, {"bounds", QJsonArray{xmin, ymin, zmin, xmax, ymax, zmax}}};
}

QJsonObject topologySource(const QString& featureId, const char* kind, const TopoDS_Shape& shape, const QString& stableKey = {}) {
  QJsonObject source{{"featureId", featureId}, {"entityKind", QString::fromLatin1(kind)}, {"signature", topologySignature(shape, kind)}};
  if (!stableKey.isEmpty()) source.insert("key", stableKey);
  return source;
}

QString primitiveVertexKey(const QString& operation, const gp_Pnt& p, const Bnd_Box& bounds) {
  if (operation != "box") return {};
  Standard_Real xmin, ymin, zmin, xmax, ymax, zmax; bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
  const auto role = [](double value, double minimum, double maximum) { if (std::abs(value-minimum) <= Precision::Confusion()) return QStringLiteral("min"); if (std::abs(value-maximum) <= Precision::Confusion()) return QStringLiteral("max"); return QString(); };
  const QString x = role(p.X(), xmin, xmax), y = role(p.Y(), ymin, ymax), z = role(p.Z(), zmin, zmax);
  return x.isEmpty() || y.isEmpty() || z.isEmpty() ? QString() : QStringLiteral("box.vertex.x%1.y%2.z%3").arg(x, y, z);
}

QString primitiveEdgeKey(const QString& operation, const TopoDS_Edge& edge, const Bnd_Box& bounds) {
  if (operation != "box") return {};
  TopoDS_Vertex first, last; TopExp::Vertices(edge, first, last); if (first.IsNull() || last.IsNull()) return {};
  const QString a = primitiveVertexKey(operation, BRep_Tool::Pnt(first), bounds), b = primitiveVertexKey(operation, BRep_Tool::Pnt(last), bounds);
  return a.isEmpty() || b.isEmpty() ? QString() : QStringLiteral("box.edge.%1.%2").arg(a < b ? a : b, a < b ? b : a);
}

QString primitiveFaceKey(const QString& operation, const TopoDS_Face& face, const Bnd_Box& bounds) {
  if (operation != "box") return {};
  Bnd_Box faceBounds; BRepBndLib::Add(face, faceBounds); Standard_Real xmin, ymin, zmin, xmax, ymax, zmax, bxmin, bymin, bzmin, bxmax, bymax, bzmax; faceBounds.Get(xmin,ymin,zmin,xmax,ymax,zmax); bounds.Get(bxmin,bymin,bzmin,bxmax,bymax,bzmax);
  const auto same = [](double a, double b) { return std::abs(a-b) <= 1.0e-5; };
  if (same(xmin,xmax)) return same(xmin,bxmin) ? QStringLiteral("box.face.xmin") : same(xmin,bxmax) ? QStringLiteral("box.face.xmax") : QString();
  if (same(ymin,ymax)) return same(ymin,bymin) ? QStringLiteral("box.face.ymin") : same(ymin,bymax) ? QStringLiteral("box.face.ymax") : QString();
  return same(zmin,zmax) ? (same(zmin,bzmin) ? QStringLiteral("box.face.zmin") : same(zmin,bzmax) ? QStringLiteral("box.face.zmax") : QString()) : QString();
}

QJsonObject describe(const TopoDS_Shape& shape, bool meshRequested, bool topologyRequested, const QString& producerFeatureId, const QString& operation) {
  if (shape.IsNull()) throw std::runtime_error("null_shape");
  BRepCheck_Analyzer checker(shape, true); if (!checker.IsValid()) throw std::runtime_error("invalid_geometry");
  GProp_GProps properties; BRepGProp::VolumeProperties(shape, properties); if (!finite(properties.Mass())) throw std::runtime_error("non_finite_geometry");
  Bnd_Box bounds; BRepBndLib::Add(shape, bounds); Standard_Real xmin, ymin, zmin, xmax, ymax, zmax; bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
  if (!coordinate(xmin) || !coordinate(ymin) || !coordinate(zmin) || !coordinate(xmax) || !coordinate(ymax) || !coordinate(zmax)) throw std::runtime_error("non_finite_geometry");
  QJsonObject result{{"brep", writeBrep(shape)}, {"valid", true}, {"volume", properties.Mass()}, {"bounds", QJsonArray{xmin, ymin, zmin, xmax, ymax, zmax}}};
  if (!meshRequested) return result;
  int faceCount = 0, edgeCount = 0; for (TopExp_Explorer faces(shape, TopAbs_FACE); faces.More(); faces.Next()) { if (++faceCount > kMaxTopologyFaces) throw std::runtime_error("topology_too_large"); } for (TopExp_Explorer edges(shape, TopAbs_EDGE); edges.More(); edges.Next()) { if (++edgeCount > kMaxTopologyEdges) throw std::runtime_error("topology_too_large"); }
  const double diagonal = std::sqrt((xmax-xmin)*(xmax-xmin) + (ymax-ymin)*(ymax-ymin) + (zmax-zmin)*(zmax-zmin));
  const double deflection = std::clamp(diagonal * 1.0e-3, 1.0e-4, 10.0);
  BRepMesh_IncrementalMesh mesher(shape, deflection, false, 0.5, true); if (!mesher.IsDone()) throw std::runtime_error("tessellation_failed");
  TopTools_IndexedMapOfShape faces, edges, shapeVertices; TopExp::MapShapes(shape, TopAbs_FACE, faces); TopExp::MapShapes(shape, TopAbs_EDGE, edges); TopExp::MapShapes(shape, TopAbs_VERTEX, shapeVertices);
  if (shapeVertices.Extent() > kMaxTopologyVertices) throw std::runtime_error("topology_too_large");
  QJsonArray vertices, normals, indices, triangleFaceIds; int offset = 0, triangleCount = 0, faceId = 0;
  for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) { ++faceId; const TopoDS_Face face = TopoDS::Face(it.Current()); TopLoc_Location loc; Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc); if (tri.IsNull() || !tri->HasUVNodes()) throw std::runtime_error("incomplete_tessellation"); BRepAdaptor_Surface surface(face); const gp_Trsf trsf = loc.Transformation(); const bool reversed = face.Orientation() == TopAbs_REVERSED; if (offset + tri->NbNodes() > kMaxMeshVertices || triangleCount + tri->NbTriangles() > kMaxMeshTriangles) throw std::runtime_error("mesh_too_large"); for (int n = 1; n <= tri->NbNodes(); ++n) { gp_Pnt p = tri->Node(n).Transformed(trsf); const gp_Pnt2d uv = tri->UVNode(n); BRepLProp_SLProps properties(surface, uv.X(), uv.Y(), 1, Precision::Confusion()); if (!properties.IsNormalDefined()) throw std::runtime_error("tessellation_normal_failed"); gp_Dir normal = properties.Normal(); if (reversed) normal.Reverse(); vertices.append(p.X()); vertices.append(p.Y()); vertices.append(p.Z()); normals.append(normal.X()); normals.append(normal.Y()); normals.append(normal.Z()); } for (int t = 1; t <= tri->NbTriangles(); ++t) { Poly_Triangle triangle = tri->Triangle(t); int a,b,c; triangle.Get(a,b,c); if (reversed) std::swap(b, c); indices.append(offset+a-1); indices.append(offset+b-1); indices.append(offset+c-1); if (topologyRequested) triangleFaceIds.append(faceId); } offset += tri->NbNodes(); triangleCount += tri->NbTriangles(); }
  const double effectiveRelativeDeflection = diagonal > Precision::Confusion() ? deflection / diagonal : 0.0;
  result.insert("mesh", QJsonObject{{"vertices", vertices}, {"normals", normals}, {"indices", indices}, {"absoluteDeflection", deflection}, {"targetRelativeDeflection", 1.0e-3}, {"effectiveRelativeDeflection", effectiveRelativeDeflection}});
  if (!topologyRequested) return result;
  Bnd_Box shapeBounds; BRepBndLib::Add(shape, shapeBounds); QJsonArray entities, topologyVertices, edgePolylines;
  for (int id = 1; id <= shapeVertices.Extent(); ++id) { const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(shapeVertices(id))); entities.append(QJsonObject{{"id", id}, {"source", topologySource(producerFeatureId, "vertex", shapeVertices(id), primitiveVertexKey(operation, p, shapeBounds))}}); topologyVertices.append(QJsonObject{{"vertexId", id}, {"point", topologyPoint(p)}}); }
  for (int id = 1; id <= edges.Extent(); ++id) { const TopoDS_Edge edge = TopoDS::Edge(edges(id)); entities.append(QJsonObject{{"id", id}, {"source", topologySource(producerFeatureId, "edge", edge, primitiveEdgeKey(operation, edge, shapeBounds))}}); TopoDS_Vertex first, last; TopExp::Vertices(edge, first, last); const int firstId = first.IsNull() ? 0 : shapeVertices.FindIndex(first), lastId = last.IsNull() ? 0 : shapeVertices.FindIndex(last); if (firstId <= 0 || lastId <= 0) throw std::runtime_error("topology_incomplete"); BRepAdaptor_Curve curve(edge); const double begin = curve.FirstParameter(), end = curve.LastParameter(); if (!finite(begin) || !finite(end)) throw std::runtime_error("topology_incomplete"); const int sampleCount = std::min(17, kMaxPolylinePoints); QJsonArray points; for (int sample = 0; sample < sampleCount; ++sample) points.append(topologyPoint(curve.Value(begin + (end-begin)*double(sample)/double(sampleCount-1)))); edgePolylines.append(QJsonObject{{"edgeId", id}, {"vertexIds", QJsonArray{firstId,lastId}}, {"points", points}}); }
  for (int id = 1; id <= faces.Extent(); ++id) { const TopoDS_Face face = TopoDS::Face(faces(id)); entities.append(QJsonObject{{"id", id}, {"source", topologySource(producerFeatureId, "face", face, primitiveFaceKey(operation, face, shapeBounds))}}); }
  result.insert("topology", QJsonObject{{"version", 1}, {"entities", entities}, {"triangleFaceIds", triangleFaceIds}, {"edgePolylines", edgePolylines}, {"vertices", topologyVertices}}); return result;
}
} // namespace

QJsonObject executeRequest(const QJsonObject& request) {
  QJsonObject identity{{"protocolVersion", kProtocolVersion}, {"operationId", request.value("operationId")}, {"documentId", request.value("documentId")}, {"revision", request.value("revision")}};
  if (QJsonDocument(request).toJson(QJsonDocument::Compact).size() > kMaxRequestBytes) return fail(identity, "request_too_large", "Request exceeds the worker limit.");
  const QJsonValue version = request.value("protocolVersion"), revision = request.value("revision"); const QString operationId = identity.value("operationId").toString(), documentId = identity.value("documentId").toString(); const double revisionNumber = revision.toDouble(-1.0);
  if (!version.isDouble() || version.toDouble() != kProtocolVersion || operationId.isEmpty() || operationId.size() > 128 || documentId.isEmpty() || documentId.size() > 128 || !revision.isDouble() || revisionNumber < 0 || revisionNumber > 9007199254740991.0 || std::floor(revisionNumber) != revisionNumber) return fail(identity, "invalid_envelope", "Protocol identity is incomplete or unsupported.");
  try { const QString operation = request.value("operation").toString(); if (operation.isEmpty()) return fail(identity, "invalid_operation", "Operation is required."); const bool topologyRequested = request.contains("topologyVersion"); const QString producerFeatureId = request.value("producerFeatureId").toString(); if (topologyRequested && (request.value("topologyVersion") != QJsonValue(1) || !uuid(producerFeatureId))) return fail(identity, "invalid_topology_request", "Topology version 1 requires an explicit UUID producer feature id."); const TopoDS_Shape shape = makeShape(operation, request.value("parameters").toObject()); QJsonObject response = identity; response.insert("ok", true); response.insert("result", describe(shape, operation != "validate", topologyRequested, producerFeatureId, operation)); if (QJsonDocument(response).toJson(QJsonDocument::Compact).size() > kMaxResponseBytes) return fail(identity, "response_too_large", "Result exceeds the worker limit."); return response; }
  catch (const Standard_Failure& error) { return fail(identity, "kernel_error", QString::fromLatin1(error.GetMessageString())); }
  catch (const std::exception& error) { return fail(identity, "geometry_error", QString::fromLatin1(error.what())); }
}

QJsonObject resolveTopologyReference(const QJsonArray& entities, const QJsonObject& reference) {
  const QString featureId = reference.value("featureId").toString(), entityKind = reference.value("entityKind").toString(), key = reference.value("key").toString();
  if (featureId.isEmpty() || entityKind.isEmpty()) return QJsonObject{{"status", "missing"}};
  QJsonArray candidates;
  for (const QJsonValue& value : entities) { const QJsonObject entity = value.toObject(), source = entity.value("source").toObject(); if (source.value("featureId") != featureId || source.value("entityKind") != entityKind) continue; if (!key.isEmpty()) { if (source.value("key") == key) candidates.append(entity); continue; } if (reference.contains("signature") && source.value("signature") == reference.value("signature")) candidates.append(entity); }
  if (candidates.size() == 1) return QJsonObject{{"status", "resolved"}, {"entity", candidates.first()}};
  return QJsonObject{{"status", candidates.isEmpty() ? "missing" : "ambiguous"}};
}
} // namespace precision::geometry
