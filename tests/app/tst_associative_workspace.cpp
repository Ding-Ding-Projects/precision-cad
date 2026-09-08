#include <QtTest>
#include <QTemporaryDir>
#include "workspace_controller.h"
#include "guided_sketch.h"
#include "model_evaluator.h"
#include <cmath>
using namespace precision::core;
class AssociativeWorkspaceTests:public QObject {
 Q_OBJECT
 QString sketchId,regionId,padId;
 void makeSketch(WorkspaceController &c) { c.addSketch("xy",80,40,8,40,20); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),15000); QCOMPARE(c.operationState(),QString("Ready")); sketchId=c.features().first().toMap().value("id").toString(); auto d=c.sketchDetails(sketchId); QVERIFY(d.value("available").toBool()); QCOMPARE(d.value("dof").toInt(),0); regionId=d.value("regions").toList().first().toMap().value("stableId").toString(); }
 void makePad(WorkspaceController &c) { makeSketch(c); c.addPad(sketchId,regionId,12); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),15000); QCOMPARE(c.operationState(),QString("Ready")); padId=c.features().last().toMap().value("id").toString(); }
 double volume(const WorkspaceController &c) { return c.volume().section(' ',0,0).toDouble(); }
private slots:
 void init() { qputenv("PRECISION_GEOMETRY_WORKER",QByteArrayLiteral(REAL_SKETCH_WORKER_PATH)); qunsetenv("PRECISION_WORKER_TIMEOUT_MS"); qunsetenv("PRECISION_WORKER_CACHE_BYTES"); }
 void editUndoSaveReopen() {
   QTemporaryDir dir; WorkspaceController c; makePad(c); QVERIFY(std::abs(volume(c)-(3200-std::acos(-1.)*64)*12)<1e-5);
   c.updateSketch(sketchId,"xy",100,40,8,40,20); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),15000); QCOMPARE(c.operationState(),QString("Ready")); QCOMPARE(c.features().first().toMap().value("id").toString(),sketchId); QCOMPARE(c.features().last().toMap().value("id").toString(),padId); QCOMPARE(c.selectedBody(),padId); QVERIFY(std::abs(volume(c)-(4000-std::acos(-1.)*64)*12)<1e-5);
   c.undo(); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),15000); QCOMPARE(c.editableSketch(sketchId).value("width").toDouble(),80.); c.redo(); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),15000); QCOMPARE(c.editableSketch(sketchId).value("width").toDouble(),100.);
   c.save(dir.filePath("model.pcad")); QVERIFY(!c.dirty()); WorkspaceController reopened; reopened.open(dir.filePath("model.pcad")); QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy(),15000); QCOMPARE(reopened.operationState(),QString("Ready")); QCOMPARE(reopened.features(),c.features()); QCOMPARE(reopened.volume(),c.volume()); QVERIFY(!reopened.dirty());
   reopened.selectBody(sketchId); QCOMPARE(reopened.volume(),QString("Unavailable")); QVERIFY(reopened.meshParts().isEmpty()); QVERIFY(reopened.sketchDetails(sketchId).value("available").toBool()); reopened.selectBody(padId); QVERIFY(!reopened.meshParts().isEmpty());
 }
 void invalidEditRollbackCancelAndCache() {
   WorkspaceController c; makePad(c); const auto mesh=c.meshVertices(),features=c.features(); const auto volumeBefore=c.volume();
   c.updateSketch(sketchId,"xy",80,40,20,40,20); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),15000); QCOMPARE(c.operationState(),QString("Failed")); QCOMPARE(c.meshVertices(),mesh); QCOMPARE(c.volume(),volumeBefore); QCOMPARE(c.features(),features); QCOMPARE(c.editableSketch(sketchId).value("holeRadius").toDouble(),8.);
   c.updateSketch(sketchId,"xy",100,40,8,40,20); c.cancel(); QCOMPARE(c.operationState(),QString("Cancelled")); QCOMPARE(c.meshVertices(),mesh); QTest::qWait(150); QCOMPARE(c.features(),features);
   qputenv("PRECISION_WORKER_CACHE_BYTES","1"); c.updateSketch(sketchId,"xy",100,40,8,40,20); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),15000); QCOMPARE(c.operationState(),QString("Failed")); QCOMPARE(c.meshVertices(),mesh); QCOMPARE(c.features(),features);
 }
 void typeBindingsAndOldSchema() {
   const auto model=rectangleHoleModel("model","xy",80,40,8,40,20);
   DocumentRecord record{1,"document",0,"mm",{{"sketch","sketch","Sketch",{},{{"model",model}},false},{"pad","pad","Pad",{"sketch"},{{"regionId","region"},{"length",12}},false}}};
   QVERIFY(ModelEvaluator::evaluate(record).result.ok); record.features[1].type="tessellate"; record.features[1].parameters={}; QVERIFY(!ModelEvaluator::evaluate(record).result.ok);
   record.features={{"box","box","Box",{},{{"dx",1},{"dy",2},{"dz",3}},false}}; DocumentRecord parsed; QVERIFY(Document::parse(Document::serialize(record),&parsed).ok); QVERIFY(ModelEvaluator::evaluate(parsed).result.ok);
 }
 void overconstrainedOpenRollsBack() {
   QTemporaryDir dir; WorkspaceController c; makePad(c); const auto previous=c.meshVertices(); auto model=rectangleHoleModel("model","xy",80,40,8,40,20); auto constraints=model.value("constraints").toArray(); auto conflict=constraints[1].toObject(); conflict.insert("id","99"); conflict.insert("value",90); constraints.append(conflict); model.insert("constraints",constraints);
   DocumentRecord invalid{1,"invalid-document",0,"mm",{{"invalid-sketch","sketch","Sketch",{},{{"model",model}},false}}}; QVERIFY(DocumentStorage::save(dir.filePath("invalid.pcad"),invalid,std::nullopt).ok); c.open(dir.filePath("invalid.pcad")); QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),15000); QCOMPARE(c.operationState(),QString("Failed")); QVERIFY(c.errorMessage().contains("constraint")); QCOMPARE(c.meshVertices(),previous);
 }
};
QTEST_GUILESS_MAIN(AssociativeWorkspaceTests)
#include "tst_associative_workspace.moc"
