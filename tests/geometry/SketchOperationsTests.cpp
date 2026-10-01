#include <QtTest>
#include "SketchOperations.h"
#include "GeometryWorker.h"
#include "guided_sketch.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
using namespace precision::geometry;
using namespace precision::core;
class SketchOperationsTests:public QObject {
 Q_OBJECT
 QJsonObject request(const QString &operation,const QJsonObject &params,int revision=3) { return {{"protocolVersion",2},{"operationId","operation-1"},{"documentId","document-1"},{"revision",revision},{"operation",operation},{"parameters",params}}; }
 QJsonObject sketch(const QString &plane="xy",double width=80) { return executeSketchRequest(request("sketch",{{"model",rectangleHoleModel("model-1",plane,width,40,8,40,20)},{"producerFeatureId","feature-sketch"}})); }
 QJsonObject pad(const QJsonObject &source,int revision=3) { const auto region=source.value("profiles").toObject().value("regions").toArray().first().toObject().value("stableId"); return executeSketchRequest(request("pad",{{"length",12},{"regionId",region},{"sketchId","feature-sketch"},{"sketch",source}},revision)); }
private slots:
 void normalizedDatumNormals_data() {
   QTest::addColumn<double>("x"); QTest::addColumn<double>("y"); QTest::addColumn<double>("z");
   QTest::newRow("negative")<<0.<<0.<<-1.; QTest::newRow("oblique")<<1.<<2.<<3.;
   QTest::newRow("extreme-large")<<1e300<<2e300<<3e300; QTest::newRow("extreme-small")<<1e-300<<2e-300<<3e-300;
 }
 void normalizedDatumNormals() {
   QFETCH(double,x); QFETCH(double,y); QFETCH(double,z); auto model=rectangleHoleModel("model","xy",80,40,8,40,20); auto plane=model.value("plane").toObject(); plane.insert("normalX",x); plane.insert("normalY",y); plane.insert("normalZ",z); model.insert("plane",plane);
   const auto s=executeSketchRequest(request("sketch",{{"model",model},{"producerFeatureId","feature-sketch"}})); QVERIFY(s.value("ok").toBool());
   const auto p=pad(s.value("result").toObject()); QVERIFY2(p.value("ok").toBool(),qPrintable(QString::fromUtf8(QJsonDocument(p).toJson())));
   const auto solid=p.value("result").toObject(); QVERIFY(std::abs(solid.value("volume").toDouble()-(3200-std::acos(-1.)*64)*12)<1e-6);
   if(x==0 && y==0 && z<0) { const auto b=solid.value("bounds").toArray(); QVERIFY(std::abs(b[2].toDouble()+12)<1e-5); QVERIFY(std::abs(b[5].toDouble())<1e-5); }
 }
 void volumeAndPlanes_data() { QTest::addColumn<QString>("plane"); for(auto p:{"xy","yz","zx"}) QTest::newRow(p)<<QString(p); }
 void volumeAndPlanes() {
   QFETCH(QString,plane); const auto s=sketch(plane); QVERIFY2(s.value("ok").toBool(),qPrintable(QString::fromUtf8(QJsonDocument(s).toJson())));
   const auto source=s.value("result").toObject(); QCOMPARE(source.value("kind"),QJsonValue("sketch")); QVERIFY(!source.contains("brep")); QCOMPARE(source.value("producerFeatureId"),QJsonValue("feature-sketch"));
   QCOMPARE(source.value("solve").toObject().value("dof"),QJsonValue(0));
   const auto p=pad(source); QVERIFY2(p.value("ok").toBool(),qPrintable(QString::fromUtf8(QJsonDocument(p).toJson())));
   const auto solid=p.value("result").toObject(); QCOMPARE(solid.size(),5); QVERIFY(std::abs(solid.value("volume").toDouble()-(80*40-std::acos(-1.)*64)*12)<1e-6);
   const auto b=solid.value("bounds").toArray(); const int axis=plane=="xy"?2:plane=="yz"?0:1; QVERIFY(std::abs(b[axis+3].toDouble()-b[axis].toDouble()-12)<1e-5);
   QVERIFY(!solid.value("mesh").toObject().value("vertices").toArray().isEmpty());
 }
 void stableRegionsAfterEdit() { auto first=sketch().value("result").toObject(),changed=sketch("xy",100).value("result").toObject(); QCOMPARE(first.value("profiles").toObject().value("regions"),changed.value("profiles").toObject().value("regions")); auto p=pad(changed); QVERIFY(p.value("ok").toBool()); QVERIFY(std::abs(p.value("result").toObject().value("volume").toDouble()-(100*40-std::acos(-1.)*64)*12)<1e-6); }
 void staleAndTamperedSourceRejected() {
   auto source=sketch().value("result").toObject(); QVERIFY(!pad(source,4).value("ok").toBool());
   source.insert("producerFeatureId","different"); QVERIFY(!pad(source).value("ok").toBool());
   source=sketch().value("result").toObject(); source.insert("documentId","different"); QVERIFY(!pad(source).value("ok").toBool());
   source=sketch().value("result").toObject(); auto preview=source.value("preview").toObject(); preview.insert("segments",QJsonArray{}); source.insert("preview",preview); QVERIFY(!pad(source).value("ok").toBool());
 }
 void invalidProfileAndSolverConflict() {
   auto model=rectangleHoleModel("model","xy",80,40,8,40,20);
   auto touchingEntities=model.value("entities").toArray(), touchingConstraints=model.value("constraints").toArray();
   auto circle=touchingEntities[5].toObject(), radiusConstraint=touchingConstraints[8].toObject();
   circle.insert("radius",20); radiusConstraint.insert("value",20); touchingEntities[5]=circle; touchingConstraints[8]=radiusConstraint;
   model.insert("entities",touchingEntities); model.insert("constraints",touchingConstraints);
   QVERIFY(!executeSketchRequest(request("sketch",{{"model",model},{"producerFeatureId","feature"}})).value("ok").toBool());
   model=rectangleHoleModel("model","xy",80,40,8,40,20); auto constraints=model.value("constraints").toArray(); auto conflict=constraints[1].toObject(); conflict.insert("id","99"); conflict.insert("value",90); constraints.append(conflict); model.insert("constraints",constraints);
   auto failed=executeSketchRequest(request("sketch",{{"model",model},{"producerFeatureId","feature"}})); QVERIFY(!failed.value("ok").toBool()); QVERIFY(failed.value("error").toObject().value("message").toString().contains("constraint")); QVERIFY(!failed.contains("result"));
 }
 void envelopeBoundsAndUnknownRegion() {
   auto r=request("sketch",{{"model",rectangleHoleModel("m","xy",80,40,8,40,20)},{"producerFeatureId","feature"}}); r.insert("extra",1); QVERIFY(!executeSketchRequest(r).value("ok").toBool());
   r=request("sketch",{}); r.insert("operationId",QString(4*1024*1024,'x')); QCOMPARE(executeSketchRequest(r).value("error").toObject().value("code"),QJsonValue("request_too_large"));
   auto source=sketch().value("result").toObject(); QVERIFY(!executeSketchRequest(request("pad",{{"length",12},{"regionId","unknown"},{"sketchId","feature-sketch"},{"sketch",source}})).value("ok").toBool());
 }
 void guidedMetadataCannotMisrepresentModel() { auto model=rectangleHoleModel("m","xy",80,40,8,40,20); QVERIFY(rectangleHoleDimensions(model).value("editable").toBool()); auto constraints=model.value("constraints").toArray(); auto c=constraints[1].toObject(); c.insert("first","4"); constraints[1]=c; model.insert("constraints",constraints); QVERIFY(!rectangleHoleDimensions(model).value("editable").toBool()); }
};
QTEST_GUILESS_MAIN(SketchOperationsTests)
#include "SketchOperationsTests.moc"
