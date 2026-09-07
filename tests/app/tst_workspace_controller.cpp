#include <QtTest>
#include "workspace_controller.h"

class WorkspaceControllerTest final : public QObject {
  Q_OBJECT
private slots:
  void init() { qputenv("PRECISION_GEOMETRY_WORKER", QByteArrayLiteral(FAKE_WORKER_PATH)); }
  void unavailableWorkerPreservesCommittedDocument() {
    qputenv("PRECISION_GEOMETRY_WORKER", QByteArrayLiteral("Z:/definitely-not-a-worker.exe"));
    WorkspaceController controller;
    QSignalSpy changed(&controller, &WorkspaceController::documentChanged);
    controller.addBox(10, 10, 10);
    QTRY_COMPARE(changed.count(), 0);
    QCOMPARE(controller.features().size(), 0);
    QCOMPARE(controller.operationState(), QStringLiteral("Failed"));
    QVERIFY(!controller.errorMessage().isEmpty());
  }
  void invalidDimensionsPreserveCommittedDocument() {
    WorkspaceController controller;
    QSignalSpy changed(&controller, &WorkspaceController::documentChanged);
    controller.addBox(-1, 10, 10);
    QTRY_COMPARE(changed.count(), 0);
    QCOMPARE(controller.features().size(), 0);
    QCOMPARE(controller.operationState(), QStringLiteral("Failed"));
  }
  void fakeFailuresPreserveCommittedDocument_data() { QTest::addColumn<double>("dx"); QTest::newRow("malformed")<<11.0; QTest::newRow("identity")<<12.0; QTest::newRow("oversized")<<13.0; }
  void fakeFailuresPreserveCommittedDocument() { QFETCH(double,dx); WorkspaceController controller; QSignalSpy changed(&controller,&WorkspaceController::documentChanged); controller.addBox(dx,10,10); QTRY_COMPARE_WITH_TIMEOUT(changed.count(),0,2000); QCOMPARE(controller.features().size(),0); QVERIFY(controller.operationState()=="Failed"); }
  void cancellationPreservesCommittedDocument() { WorkspaceController controller; controller.addBox(14,10,10); QTRY_VERIFY_WITH_TIMEOUT(controller.operationState().startsWith("Regenerating"),1000); controller.cancel(); QCOMPARE(controller.features().size(),0); QCOMPARE(controller.operationState(),QStringLiteral("Cancelled")); }
  void timeoutPreservesCommittedDocument() { qputenv("PRECISION_WORKER_TIMEOUT_MS","30"); WorkspaceController controller; controller.addBox(14,10,10); QTRY_COMPARE_WITH_TIMEOUT(controller.operationState(),QStringLiteral("Failed"),1000); QCOMPARE(controller.features().size(),0); qunsetenv("PRECISION_WORKER_TIMEOUT_MS"); }
};
QTEST_GUILESS_MAIN(WorkspaceControllerTest)
#include "tst_workspace_controller.moc"
