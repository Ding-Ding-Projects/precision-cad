#include <QtTest>
#include "workspace_controller.h"

class WorkspaceControllerTest final : public QObject {
  Q_OBJECT
private slots:
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
};
QTEST_GUILESS_MAIN(WorkspaceControllerTest)
#include "tst_workspace_controller.moc"
