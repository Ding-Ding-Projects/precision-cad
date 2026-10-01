#include "GeometryWorker.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QJsonDocument>
#include <BRep_Builder.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <Geom_BezierCurve.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <gp_Trsf.hxx>
#include <gp_Pln.hxx>
#include <gp_Circ.hxx>
#include <algorithm>
#include <sstream>
#include <QtTest/QTest>

using precision::geometry::executeRequest;
class GeometryWorkerTests final : public QObject {
  Q_OBJECT
  QJsonObject request(const QString& operation, QJsonObject parameters) { return {{"protocolVersion", 1}, {"operationId", "op-1"}, {"documentId", "doc-1"}, {"revision", 7}, {"operation", operation}, {"parameters", parameters}}; }
  QJsonObject topologyRequest(double dx, double dy, double dz) { auto value = request("box", {{"dx", dx}, {"dy", dy}, {"dz", dz}}); value.insert("topologyVersion", 1); value.insert("producerFeatureId", "123e4567-e89b-42d3-a456-426614174000"); return value; }
  static QString encode(const TopoDS_Shape& shape) {
    std::ostringstream stream; BRepTools::Write(shape, stream);
    return QString::fromLatin1(QByteArray::fromStdString(stream.str()).toBase64());
  }
  static TopoDS_Shape decode(const QString& encoded) {
    const auto bytes = QByteArray::fromBase64(encoded.toLatin1()); std::istringstream stream(bytes.toStdString());
    TopoDS_Shape shape; BRep_Builder builder; BRepTools::Read(shape, stream, builder); return shape;
  }
  QJsonObject shapeRequest(const TopoDS_Shape& shape, bool topology = true) {
    auto value = request("tessellate", {{"brep", encode(shape)}});
    if (topology) { value.insert("topologyVersion", 1); value.insert("producerFeatureId", "123e4567-e89b-42d3-a456-426614174000"); }
    return value;
  }
  static QJsonObject edgeReference(const QJsonObject& topology) {
    for (const auto& value : topology.value("entities").toArray()) {
      auto source = value.toObject().value("source").toObject();
      if (source.value("entityKind") != "edge") continue;
      if (source.contains("key")) source.remove("signature");
      return source;
    }
    return {};
  }
  static QJsonObject selection(const QJsonObject& result) {
    const auto topology = result.value("topology").toObject(), producer = topology.value("producer").toObject();
    return {{"version",1},{"mode","selectedEdges"},{"producerFeatureId",producer.value("featureId")},{"producerOperation",producer.value("operation")},{"brepSha256",topology.value("brepSha256")},{"topology",topology},{"edgeRefs",QJsonArray{edgeReference(topology)}}};
  }
  void verifyFaceMembership(const QJsonObject& reply) {
    QVERIFY2(reply.value("ok").toBool(), QJsonDocument(reply).toJson().constData());
    const auto result = reply.value("result").toObject(), topology = result.value("topology").toObject(), mesh = result.value("mesh").toObject();
    TopTools_IndexedMapOfShape faces, edges, shapeVertices;
    const auto shape = decode(result.value("brep").toString());
    TopExp::MapShapes(shape, TopAbs_FACE, faces); TopExp::MapShapes(shape, TopAbs_EDGE, edges); TopExp::MapShapes(shape, TopAbs_VERTEX, shapeVertices);
    const auto positions = mesh.value("vertices").toArray(), indices = mesh.value("indices").toArray(), owners = topology.value("triangleFaceIds").toArray();
    QCOMPARE(indices.size(), owners.size()*3);
    QSet<int> uniqueIds;
    for (const auto& entry : topology.value("entities").toArray()) { const int id = entry.toObject().value("id").toInt(); QVERIFY(!uniqueIds.contains(id)); uniqueIds.insert(id); }
    for (int t = 0; t < owners.size(); ++t) {
      const int index = owners[t].toInt() - shapeVertices.Extent() - edges.Extent();
      QVERIFY(index > 0 && index <= faces.Extent());
      for (int corner = 0; corner < 3; ++corner) {
        const int vertex = indices[t*3+corner].toInt();
        const gp_Pnt p(positions[vertex*3].toDouble(), positions[vertex*3+1].toDouble(), positions[vertex*3+2].toDouble());
        BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(p).Vertex(), faces(index));
        QVERIFY(distance.IsDone()); QVERIFY2(distance.Value() < 1.0e-6, "Triangle corner is not on its named face");
      }
    }
  }
private slots:
  void primitiveVolumesAndTessellation() { const auto reply = executeRequest(request("box", {{"dx", 10.0}, {"dy", 20.0}, {"dz", 30.0}})); QVERIFY(reply.value("ok").toBool()); const auto result = reply.value("result").toObject(); QCOMPARE(result.value("volume").toDouble(), 6000.0); QVERIFY(!result.value("brep").toString().isEmpty()); QVERIFY(result.value("mesh").toObject().value("indices").toArray().size() >= 36); QVERIFY(executeRequest(request("box", {{"dx", 2000.0}, {"dy", 2000.0}, {"dz", 2000.0}})).value("ok").toBool()); }
  void drilledBracketBoolean() { const auto box = executeRequest(request("box", {{"dx", 20.0}, {"dy", 20.0}, {"dz", 10.0}})).value("result").toObject().value("brep").toString(); const auto hole = executeRequest(request("cylinder", {{"radius", 4.0}, {"height", 10.0}, {"origin", QJsonArray{10.0, 10.0, 0.0}}})).value("result").toObject().value("brep").toString(); const auto result = executeRequest(request("cut", {{"leftBrep", box}, {"rightBrep", hole}})); QVERIFY(result.value("ok").toBool()); QVERIFY(result.value("result").toObject().value("volume").toDouble() < 4000.0); }
  void serializationRoundTrip() { const auto first = executeRequest(request("cylinder", {{"radius", 2.0}, {"height", 10.0}})); const auto encoded = first.value("result").toObject().value("brep").toString(); const auto second = executeRequest(request("validate", {{"brep", encoded}})); QVERIFY(second.value("ok").toBool()); QCOMPARE(second.value("result").toObject().value("volume").toDouble(), first.value("result").toObject().value("volume").toDouble()); }
  void cylinderNormalsAndWinding() { const auto result = executeRequest(request("cylinder", {{"radius", 2.0}, {"height", 6.0}})).value("result").toObject().value("mesh").toObject(); const auto vertices = result.value("vertices").toArray(), normals = result.value("normals").toArray(), indices = result.value("indices").toArray(); QVERIFY(vertices.size() == normals.size()); bool radial = false; for (int i = 0; i < vertices.size(); i += 3) { const double x = vertices.at(i).toDouble(), y = vertices.at(i + 1).toDouble(), nz = normals.at(i + 2).toDouble(); if (x*x + y*y > 3.5 && std::abs(nz) < 0.2) { const double dot = (x * normals.at(i).toDouble() + y * normals.at(i + 1).toDouble()) / std::sqrt(x*x+y*y); QVERIFY(dot > 0.9); radial = true; } } QVERIFY(radial); for (int i = 0; i < indices.size(); i += 3) { const int ia = indices.at(i).toInt(), ib = indices.at(i+1).toInt(), ic = indices.at(i+2).toInt(); const auto p = [&vertices](int n, int c) { return vertices.at(n*3+c).toDouble(); }; const double ux=p(ib,0)-p(ia,0), uy=p(ib,1)-p(ia,1), uz=p(ib,2)-p(ia,2), vx=p(ic,0)-p(ia,0), vy=p(ic,1)-p(ia,1), vz=p(ic,2)-p(ia,2); const double nx=normals.at(ia*3).toDouble()+normals.at(ib*3).toDouble()+normals.at(ic*3).toDouble(), ny=normals.at(ia*3+1).toDouble()+normals.at(ib*3+1).toDouble()+normals.at(ic*3+1).toDouble(), nz=normals.at(ia*3+2).toDouble()+normals.at(ib*3+2).toDouble()+normals.at(ic*3+2).toDouble(); QVERIFY((uy*vz-uz*vy)*nx+(uz*vx-ux*vz)*ny+(ux*vy-uy*vx)*nz > 0.0); } }
  void topologyOptInMapsEveryTriangleToAnExistingFace() { const auto standard = executeRequest(request("box", {{"dx", 10.0}, {"dy", 20.0}, {"dz", 30.0}})); QVERIFY(!standard.value("result").toObject().contains("topology")); const auto topology = executeRequest(topologyRequest(10.0, 20.0, 30.0)).value("result").toObject().value("topology").toObject(); QCOMPARE(topology.value("version").toInt(), 1); const auto triangleFaceIds = topology.value("triangleFaceIds").toArray(), indices = executeRequest(topologyRequest(10.0, 20.0, 30.0)).value("result").toObject().value("mesh").toObject().value("indices").toArray(); QCOMPARE(triangleFaceIds.size(), indices.size()/3); QSet<int> faces; for (const auto& entity : topology.value("entities").toArray()) { const auto object = entity.toObject(); if (object.value("source").toObject().value("entityKind") == "face") faces.insert(object.value("id").toInt()); } QVERIFY(!faces.isEmpty()); for (const auto& id : triangleFaceIds) QVERIFY(faces.contains(id.toInt())); QVERIFY(!topology.value("edgePolylines").toArray().isEmpty()); }
  void primitiveTopologyKeysRemainSemanticWhenDimensionsChange() { const auto first = executeRequest(topologyRequest(10.0, 20.0, 30.0)).value("result").toObject().value("topology").toObject(); const auto second = executeRequest(topologyRequest(11.0, 20.0, 30.0)).value("result").toObject().value("topology").toObject(); QJsonObject firstFace, secondFace; for (const auto& entity : first.value("entities").toArray()) if (entity.toObject().value("source").toObject().value("key") == "box.face.xmax") firstFace = entity.toObject(); for (const auto& entity : second.value("entities").toArray()) if (entity.toObject().value("source").toObject().value("key") == "box.face.xmax") secondFace = entity.toObject(); QVERIFY(!firstFace.isEmpty()); QVERIFY(!secondFace.isEmpty()); QVERIFY(firstFace.value("source").toObject().value("signature") != secondFace.value("source").toObject().value("signature")); }
  void resolverRejectsSymmetricSignatureAmbiguity() {
    auto result = executeRequest(topologyRequest(10,20,30)).value("result").toObject();
    auto entities = result.value("topology").toObject().value("entities").toArray();
    auto entity = entities[0].toObject(); auto source = entity.value("source").toObject(); source.remove("key");
    entity.insert("source", source); auto duplicate = entity; duplicate.insert("id", 200);
    QCOMPARE(precision::geometry::resolveTopologyReference({entity, duplicate}, source).value("status").toString(), "ambiguous");
    QCOMPARE(precision::geometry::resolveTopologyReference({entity}, source).value("status").toString(), "resolved");
    auto absent = source; absent.insert("key", "absent");
    QCOMPARE(precision::geometry::resolveTopologyReference({entity}, absent).value("status").toString(), "missing");
  }
  void filletKeepsLegacyCompatibilityAndRejectsBadSelectedTopology() { const auto box = executeRequest(request("box", {{"dx", 10.0}, {"dy", 20.0}, {"dz", 30.0}})).value("result").toObject().value("brep").toString(); QVERIFY(executeRequest(request("fillet", {{"brep", box}, {"radius", 1.0}})).value("ok").toBool()); QJsonObject selected{{"version",1},{"mode","selectedEdges"},{"producerFeatureId","123e4567-e89b-42d3-a456-426614174000"},{"producerOperation","box"},{"brepSha256","wrong"},{"edgeRefs",QJsonArray{}}}; QVERIFY(!executeRequest(request("fillet", {{"brep", box}, {"radius", 1.0}, {"topologyInput", selected}})).value("ok").toBool()); }
  void resolverValidatesExactSchemaAndUniqueIds() {
    const auto entities = executeRequest(topologyRequest(10,20,30)).value("result").toObject().value("topology").toObject().value("entities").toArray();
    const auto entity = entities[0].toObject(), source = entity.value("source").toObject();
    auto invalid = [&](const QJsonArray& entries, const QJsonObject& ref) { QCOMPARE(precision::geometry::resolveTopologyReference(entries,ref).value("status").toString(), "invalid"); };
    for (const QString field : {"featureId", "entityKind"}) { auto ref = source; ref.remove(field); invalid({entity},ref); }
    for (const QString badUuid : {"f1", "123e4567-e89b-02d3-a456-426614174000", "123e4567-e89b-42d3-7456-426614174000"}) { auto ref=source; ref.insert("featureId",badUuid); invalid({entity},ref); }
    auto ref=source; ref.insert("unknown",true); invalid({entity},ref);
    ref=source; ref.insert("entityKind","solid"); invalid({entity},ref);
    ref=source; ref.insert("key",false); invalid({entity},ref);
    for (const QJsonValue signature : {QJsonValue(false), QJsonValue(QJsonObject{{"kind","vertex"}}), QJsonValue(QJsonObject{{"kind","vertex"},{"bounds",QJsonArray{0,0,0,1,1,QJsonValue()}}}), QJsonValue(QJsonObject{{"kind","vertex"},{"bounds",QJsonArray{2,0,0,1,1,1}}}), QJsonValue(QJsonObject{{"kind","vertex"},{"bounds",QJsonArray{0,0,0,1,1,1}},{"extra",1}})}) { ref=source; ref.insert("signature",signature); invalid({entity},ref); }
    invalid({entity,entity},source); invalid({entity,false},source);
    auto bad=entity; bad.insert("id",1.5); invalid({bad},source);
    bad=entity; bad.insert("id",0); invalid({bad},source);
    bad=entity; bad.insert("extra",true); invalid({bad},source);
    bad=entity; auto badSource=source; badSource.insert("featureId","not-a-uuid"); bad.insert("source",badSource); invalid({bad},source);
    ref=source; auto signature=ref.value("signature").toObject(); signature.insert("bounds",QJsonArray{0,0,0,1,1,1}); ref.insert("signature",signature);
    QCOMPARE(precision::geometry::resolveTopologyReference({entity},ref).value("status").toString(),"missing");
  }
  void triangleOwnersMatchGeometryIncludingSharedFaces() {
    verifyFaceMembership(executeRequest(topologyRequest(10,20,30)));
    const auto box = BRepPrimAPI_MakeBox(10,20,30).Shape();
    TopExp_Explorer explorer(box,TopAbs_FACE); const auto shared=explorer.Current();
    TopoDS_Compound compound; BRep_Builder builder; builder.MakeCompound(compound);
    builder.Add(compound,shared); builder.Add(compound,box);
    gp_Trsf move; move.SetTranslation(gp_Vec(50,0,0)); builder.Add(compound,box.Moved(TopLoc_Location(move)));
    builder.Add(compound,shared.Reversed());
    verifyFaceMembership(executeRequest(shapeRequest(compound)));
  }
  void adaptivePolylinesBoundWholeCurvesAndHandleDegeneracy() {
    const auto box = executeRequest(topologyRequest(10,20,30)).value("result").toObject().value("topology").toObject();
    for (const auto& entry : box.value("edgePolylines").toArray()) { QCOMPARE(entry.toObject().value("points").toArray().size(),2); QVERIFY(!entry.toObject().value("degenerate").toBool()); }
    TColgp_Array1OfPnt poles(1,4); poles.SetValue(1,gp_Pnt(0,0,0)); poles.SetValue(2,gp_Pnt(1,20,0)); poles.SetValue(3,gp_Pnt(2,-20,0)); poles.SetValue(4,gp_Pnt(3,0,0));
    Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles);
    const auto edge=BRepBuilderAPI_MakeEdge(curve).Edge();
    const auto reply=executeRequest(shapeRequest(edge)); QVERIFY2(reply.value("ok").toBool(),QJsonDocument(reply).toJson().constData());
    const auto polyline=reply.value("result").toObject().value("topology").toObject().value("edgePolylines").toArray()[0].toObject();
    const auto points=polyline.value("points").toArray(); QVERIFY(points.size()>2); QVERIFY(points.size()<=1025);
    const auto pt=[](const QJsonValue& value) { const auto a=value.toArray(); return gp_Pnt(a[0].toDouble(),a[1].toDouble(),a[2].toDouble()); };
    for (int sample=0;sample<=2000;++sample) {
      const gp_Pnt p=curve->Value(double(sample)/2000.0); double distance=1.0e30;
      for (int i=1;i<points.size();++i) { const gp_Pnt a=pt(points[i-1]),b=pt(points[i]); const gp_Vec chord(a,b),delta(a,p); const double u=std::clamp(delta.Dot(chord)/chord.SquareMagnitude(),0.0,1.0); distance=std::min(distance,p.Distance(a.Translated(chord*u))); }
      QVERIFY(distance<=polyline.value("chordTolerance").toDouble()+1e-8);
    }
    const auto sphere=BRepPrimAPI_MakeSphere(5).Shape(); bool tested=false;
    for (TopExp_Explorer it(sphere,TopAbs_EDGE);it.More();it.Next()) {
      const auto degenerate=TopoDS::Edge(it.Current()); if (!BRep_Tool::Degenerated(degenerate)) continue;
      const auto degenerateReply=executeRequest(shapeRequest(degenerate)); QVERIFY2(degenerateReply.value("ok").toBool(),QJsonDocument(degenerateReply).toJson().constData());
      const auto line=degenerateReply.value("result").toObject().value("topology").toObject().value("edgePolylines").toArray()[0].toObject();
      QVERIFY(line.value("degenerate").toBool()); QCOMPARE(line.value("points").toArray().size(),1); tested=true; break;
    }
    QVERIFY(tested);
  }
  void legacyMeshDoesNotApplyTopologyCountLimits() {
    const auto face=BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),0,1,0,1).Face();
    TopoDS_Compound compound; BRep_Builder builder; builder.MakeCompound(compound);
    for (int i=0;i<5001;++i) { gp_Trsf move; move.SetTranslation(gp_Vec(i*2,0,0)); builder.Add(compound,face.Moved(TopLoc_Location(move))); }
    const auto standard=executeRequest(shapeRequest(compound,false)); QVERIFY2(standard.value("ok").toBool(),QJsonDocument(standard.value("error").toObject()).toJson().constData());
    QVERIFY(!standard.value("result").toObject().contains("topology"));
    const auto topology=executeRequest(shapeRequest(compound)); QVERIFY(!topology.value("ok").toBool()); QCOMPARE(topology.value("error").toObject().value("message").toString(),"topology_too_large");
  }
  void closedCurveSubdividesAndPointBudgetFailsClosed() {
    const auto circle=BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(0,0,0),gp_Dir(0,0,1)),10)).Edge();
    const auto reply=executeRequest(shapeRequest(circle)); QVERIFY2(reply.value("ok").toBool(),QJsonDocument(reply).toJson().constData());
    const auto line=reply.value("result").toObject().value("topology").toObject().value("edgePolylines").toArray()[0].toObject();
    const auto points=line.value("points").toArray(); QVERIFY(points.size()>17); QVERIFY(points.size()<=1025);
    const auto endpointIds=line.value("vertexIds").toArray(); QCOMPARE(endpointIds[0],endpointIds[1]);
    const double tolerance=line.value("chordTolerance").toDouble();
    for (int i=1;i<points.size();++i) { const auto a=points[i-1].toArray(), b=points[i].toArray(); const double x=(a[0].toDouble()+b[0].toDouble())/2, y=(a[1].toDouble()+b[1].toDouble())/2; QVERIFY(10-std::hypot(x,y)<=tolerance+1e-8); }
    const auto hugeCircle=BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(0,0,0),gp_Dir(0,0,1)),1.0e7)).Edge();
    const auto rejected=executeRequest(shapeRequest(hugeCircle)); QVERIFY(!rejected.value("ok").toBool()); QVERIFY(!rejected.contains("result")); QCOMPARE(rejected.value("error").toObject().value("message").toString(),"topology_polyline_too_large");
  }
  void exactSerializedResponseBoundary() {
    constexpr qsizetype limit=8*1024*1024;
    auto response=executeRequest(topologyRequest(10,20,30)); QVERIFY(response.value("ok").toBool());
    auto result=response.value("result").toObject(); auto topology=result.value("topology").toObject(); topology.insert("boundaryFixture",QString()); result.insert("topology",topology); response.insert("result",result);
    const qsizetype base=QJsonDocument(response).toJson(QJsonDocument::Compact).size();
    topology.insert("boundaryFixture",QString(limit-base,QLatin1Char('x'))); result.insert("topology",topology); response.insert("result",result);
    QCOMPARE(QJsonDocument(response).toJson(QJsonDocument::Compact).size(),limit); QVERIFY(precision::geometry::boundResponse(response).value("ok").toBool());
    topology.insert("boundaryFixture",QString(limit-base+1,QLatin1Char('x'))); result.insert("topology",topology); response.insert("result",result);
    QCOMPARE(QJsonDocument(response).toJson(QJsonDocument::Compact).size(),limit+1);
    const auto rejected=precision::geometry::boundResponse(response); QVERIFY(!rejected.value("ok").toBool()); QVERIFY(!rejected.contains("result")); QCOMPARE(rejected.value("error").toObject().value("code").toString(),"response_too_large"); QCOMPARE(rejected.value("operationId"),response.value("operationId"));
    topology.insert("boundaryFixture",QString((limit-base)/2+1,QLatin1Char('"'))); result.insert("topology",topology); response.insert("result",result);
    QVERIFY(QJsonDocument(response).toJson(QJsonDocument::Compact).size()>limit); QVERIFY(!precision::geometry::boundResponse(response).value("ok").toBool());
  }
  void selectedFilletConsumesCompleteCachedMap() {
    const auto result=executeRequest(topologyRequest(10,20,30)).value("result").toObject(); const auto selected=selection(result);
    auto run=[&](const QJsonValue& value) { return executeRequest(request("fillet",{{"brep",result.value("brep")},{"radius",1.0},{"topologyInput",value}})); };
    const auto accepted=run(selected); QVERIFY2(accepted.value("ok").toBool(),QJsonDocument(accepted).toJson().constData());
    const auto legacy=executeRequest(request("fillet",{{"brep",result.value("brep")},{"radius",1.0}})); QVERIFY(legacy.value("ok").toBool());
    QVERIFY(accepted.value("result").toObject().value("volume").toDouble()>legacy.value("result").toObject().value("volume").toDouble());
    for (const QString field : {"topology","brepSha256","producerFeatureId","mode","edgeRefs"}) { auto bad=selected; bad.remove(field); QVERIFY(!run(bad).value("ok").toBool()); }
    for (const QJsonValue bad : {QJsonValue(),QJsonValue(false),QJsonValue(QJsonObject{})}) QVERIFY(!run(bad).value("ok").toBool());
    for (const QString field : {"producerFeatureId","producerOperation","brepSha256"}) { auto bad=selected; bad.insert(field,"wrong"); QVERIFY(!run(bad).value("ok").toBool()); }
    auto producerMismatch=selected; producerMismatch.insert("producerFeatureId","123e4567-e89b-42d3-a456-426614174001"); QVERIFY(!run(producerMismatch).value("ok").toBool());
    for (const QString field : {"featureId","entityKind","key"}) {
      auto invalidReference=selected; auto ref=edgeReference(result.value("topology").toObject());
      ref.insert(field,field=="featureId" ? "123e4567-e89b-42d3-a456-426614174001" : field=="entityKind" ? "face" : "absent");
      invalidReference.insert("edgeRefs",QJsonArray{ref}); QVERIFY(!run(invalidReference).value("ok").toBool());
    }
    auto bad=selected; auto refs=bad.value("edgeRefs").toArray(); refs.append(refs[0]); bad.insert("edgeRefs",refs); QVERIFY(!run(bad).value("ok").toBool());
    for (const QString field : {"entities","vertices","edgePolylines","triangleFaceIds"}) { bad=selected; auto map=bad.value("topology").toObject(); map.insert(field,QJsonArray{}); bad.insert("topology",map); QVERIFY(!run(bad).value("ok").toBool()); }
    bad=selected; auto map=bad.value("topology").toObject(); auto entities=map.value("entities").toArray(); auto entity=entities[8].toObject(); auto source=entity.value("source").toObject(); source.insert("key","box.edge.forged"); entity.insert("source",source); entities[8]=entity; map.insert("entities",entities); bad.insert("topology",map); QVERIFY(!run(bad).value("ok").toBool());
    bad=selected; map=bad.value("topology").toObject(); auto lines=map.value("edgePolylines").toArray(); auto line=lines[0].toObject(); auto points=line.value("points").toArray(); points[0]=QJsonArray{999,999,999}; line.insert("points",points); lines[0]=line; map.insert("edgePolylines",lines); bad.insert("topology",map); QVERIFY(!run(bad).value("ok").toBool());
    bad=selected; map=bad.value("topology").toObject(); entities=map.value("entities").toArray(); entity=entities[8].toObject(); source=entity.value("source").toObject(); source.insert("entityKind","face"); auto signature=source.value("signature").toObject(); signature.insert("kind","face"); source.insert("signature",signature); entity.insert("source",source); entities[8]=entity; map.insert("entities",entities); bad.insert("topology",map); QVERIFY(!run(bad).value("ok").toBool());
    const auto different=executeRequest(topologyRequest(11,20,30)).value("result").toObject();
    bad=selected; map=different.value("topology").toObject(); map.insert("brepSha256",selected.value("brepSha256")); bad.insert("topology",map); QVERIFY(!run(bad).value("ok").toBool());
    bad=selected; map=bad.value("topology").toObject(); auto owners=map.value("triangleFaceIds").toArray(); owners[0]=owners[0].toDouble()+1e-8; map.insert("triangleFaceIds",owners); bad.insert("topology",map); QVERIFY(!run(bad).value("ok").toBool());
    auto genericRequest=request("translate",{{"brep",result.value("brep")},{"vector",QJsonArray{2,3,4}}}); genericRequest.insert("topologyVersion",1); genericRequest.insert("producerFeatureId","123e4567-e89b-42d3-a456-426614174001");
    const auto genericResult=executeRequest(genericRequest).value("result").toObject(); const auto genericSelected=selection(genericResult);
    const auto genericReply=executeRequest(request("fillet",{{"brep",genericResult.value("brep")},{"radius",1.0},{"topologyInput",genericSelected}})); QVERIFY2(genericReply.value("ok").toBool(),QJsonDocument(genericReply).toJson().constData());
    for (const auto& item : genericResult.value("topology").toObject().value("entities").toArray()) QVERIFY(!item.toObject().value("source").toObject().contains("key"));
  }
  void rejectsUnknownAndInvalid() { QVERIFY(!executeRequest(request("teleport", {})).value("ok").toBool()); QVERIFY(!executeRequest(request("box", {{"dx", -1.0}, {"dy", 1.0}, {"dz", 1.0}})).value("ok").toBool()); auto incompatible = request("box", {{"dx", 1.0}, {"dy", 1.0}, {"dz", 1.0}}); incompatible.insert("protocolVersion", 1.5); QVERIFY(!executeRequest(incompatible).value("ok").toBool()); const auto box = executeRequest(request("box", {{"dx", 1.0}, {"dy", 1.0}, {"dz", 1.0}})).value("result").toObject().value("brep").toString(); QVERIFY(!executeRequest(request("validate", {{"brep", box + "!"}})).value("ok").toBool()); QVERIFY(executeRequest(request("translate", {{"brep", box}, {"vector", QJsonArray{0.0,0.0,0.0}}})).value("ok").toBool()); }
};
QTEST_MAIN(GeometryWorkerTests)
#include "GeometryWorkerTests.moc"
