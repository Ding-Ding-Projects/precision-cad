#include <QtTest>
#include <QTemporaryDir>
#include "workspace_controller.h"
using namespace precision::core;
class WorkspaceControllerTest final : public QObject {
 Q_OBJECT
private:
 void seed(WorkspaceController &c) { c.addBox(10,10,10); QTRY_COMPARE(c.operationState(),QString("Ready")); QCOMPARE(c.features().size(),1); QVERIFY(!c.meshVertices().isEmpty()); }
private slots:
 void init() { qputenv("PRECISION_GEOMETRY_WORKER",QByteArrayLiteral(FAKE_WORKER_PATH)); qunsetenv("PRECISION_WORKER_TIMEOUT_MS"); }
 void unavailableWorkerClearsBusy() { WorkspaceController c; seed(c); auto old=c.features(); qputenv("PRECISION_GEOMETRY_WORKER","Z:/missing-worker.exe"); c.addBox(1,1,1); QTRY_COMPARE(c.operationState(),QString("Failed")); QVERIFY(!c.busy()); QCOMPARE(c.features(),old); }
 void invalidDimensions() { WorkspaceController c; seed(c); auto old=c.features(); c.addBox(-1,1,1); QTRY_COMPARE(c.operationState(),QString("Failed")); QCOMPARE(c.features(),old); QVERIFY(!c.busy()); }
 void invalidResults_data() { QTest::addColumn<double>("dx"); for(int i:{11,12,13,15,17,18,19,20,21,22,23,24,25}) QTest::newRow(qPrintable(QString::number(i)))<<double(i); }
 void invalidResults() { QFETCH(double,dx); WorkspaceController c; seed(c); auto old=c.features(),mesh=c.meshVertices(); c.addBox(dx,1,1); QTRY_COMPARE_WITH_TIMEOUT(c.operationState(),QString("Failed"),4000); QVERIFY(!c.busy()); QCOMPARE(c.features(),old); QCOMPARE(c.meshVertices(),mesh); }
 void duplicateBusyRequest() { WorkspaceController c; seed(c); c.addBox(16,1,1); c.addBox(-1,1,1); QVERIFY(c.busy()); QTRY_COMPARE(c.operationState(),QString("Ready")); QCOMPARE(c.features().size(),2); }
 void cancelAndRestart() { WorkspaceController c; seed(c); c.addBox(14,1,1); c.cancel(); QCOMPARE(c.operationState(),QString("Cancelled")); QCOMPARE(c.features().size(),1); c.addBox(2,2,2); QTRY_COMPARE(c.operationState(),QString("Ready")); QCOMPARE(c.features().size(),2); }
 void timeout() { WorkspaceController c; seed(c); qputenv("PRECISION_WORKER_TIMEOUT_MS","100"); c.addBox(14,1,1); QTRY_COMPARE_WITH_TIMEOUT(c.operationState(),QString("Failed"),2000); QVERIFY(!c.busy()); QCOMPARE(c.features().size(),1); }
 void saveAsAndMultipleEdits() { QTemporaryDir dir; WorkspaceController c; seed(c); c.save(dir.filePath("a.pcad")); QVERIFY(!c.dirty()); c.addBox(2,2,2); QTRY_COMPARE(c.operationState(),QString("Ready")); c.addBox(3,3,3); QTRY_COMPARE(c.operationState(),QString("Ready")); c.save(dir.filePath("a.pcad")); QVERIFY(!c.dirty()); c.save(dir.filePath("b.pcad")); QVERIFY(QFile::exists(dir.filePath("b.pcad"))); QCOMPARE(c.documentPath(),dir.filePath("b.pcad")); }
 void failedOpenPreservesAll() { QTemporaryDir dir; WorkspaceController c; seed(c); c.save(dir.filePath("good.pcad")); c.addBox(2,2,2); QTRY_COMPARE(c.operationState(),QString("Ready")); auto old=c.features(),mesh=c.meshVertices(); DocumentRecord bad{1,"other",1,"mm",{{"bad","box","Bad",{},{{"dx",11},{"dy",1},{"dz",1}},false}}}; QVERIFY(DocumentStorage::save(dir.filePath("bad.pcad"),bad,std::nullopt).ok); c.open(dir.filePath("bad.pcad")); QTRY_COMPARE(c.operationState(),QString("Failed")); QCOMPARE(c.features(),old); QCOMPARE(c.meshVertices(),mesh); QCOMPARE(c.documentPath(),dir.filePath("good.pcad")); QVERIFY(c.dirty()); c.open(dir.filePath("good.pcad")); QTRY_COMPARE(c.operationState(),QString("Ready")); QVERIFY(!c.dirty()); QCOMPARE(c.features().size(),1); }
 void imperialOpenIsRejected() { QTemporaryDir dir; WorkspaceController c; seed(c); const auto previous=c.features(); DocumentRecord record{1,"imperial",1,"in",{}}; QVERIFY(DocumentStorage::save(dir.filePath("in.pcad"),record,std::nullopt).ok); c.open(dir.filePath("in.pcad")); QCOMPARE(c.operationState(),QString("Failed")); QCOMPARE(c.features(),previous); QVERIFY(c.dirty()); QVERIFY(c.documentPath().isEmpty()); }
 void selectBodyAndEmptyMesh() { WorkspaceController c; seed(c); const auto id=c.features()[0].toMap()["id"].toString(); c.selectBody(id); QCOMPARE(c.selectedBody(),id); c.suppressFeature(id,true); QTRY_COMPARE(c.operationState(),QString("Ready")); QVERIFY(c.meshVertices().isEmpty()); }
};
QTEST_GUILESS_MAIN(WorkspaceControllerTest)
#include "tst_workspace_controller.moc"
