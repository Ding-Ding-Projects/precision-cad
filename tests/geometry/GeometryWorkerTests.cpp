#include "GeometryWorker.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest/QTest>

using precision::geometry::executeRequest;
class GeometryWorkerTests final : public QObject {
  Q_OBJECT
  QJsonObject request(const QString& operation, QJsonObject parameters) { return {{"protocolVersion", 1}, {"operationId", "op-1"}, {"documentId", "doc-1"}, {"revision", 7}, {"operation", operation}, {"parameters", parameters}}; }
private slots:
  void primitiveVolumesAndTessellation() { const auto reply = executeRequest(request("box", {{"dx", 10.0}, {"dy", 20.0}, {"dz", 30.0}})); QVERIFY(reply.value("ok").toBool()); const auto result = reply.value("result").toObject(); QCOMPARE(result.value("volume").toDouble(), 6000.0); QVERIFY(!result.value("brep").toString().isEmpty()); QVERIFY(result.value("mesh").toObject().value("indices").toArray().size() >= 36); }
  void drilledBracketBoolean() { const auto box = executeRequest(request("box", {{"dx", 20.0}, {"dy", 20.0}, {"dz", 10.0}})).value("result").toObject().value("brep").toString(); const auto hole = executeRequest(request("cylinder", {{"radius", 4.0}, {"height", 10.0}, {"origin", QJsonArray{10.0, 10.0, 0.0}}})).value("result").toObject().value("brep").toString(); const auto result = executeRequest(request("cut", {{"leftBrep", box}, {"rightBrep", hole}})); QVERIFY(result.value("ok").toBool()); QVERIFY(result.value("result").toObject().value("volume").toDouble() < 4000.0); }
  void serializationRoundTrip() { const auto first = executeRequest(request("cylinder", {{"radius", 2.0}, {"height", 10.0}})); const auto encoded = first.value("result").toObject().value("brep").toString(); const auto second = executeRequest(request("validate", {{"brep", encoded}})); QVERIFY(second.value("ok").toBool()); QCOMPARE(second.value("result").toObject().value("volume").toDouble(), first.value("result").toObject().value("volume").toDouble()); }
  void rejectsUnknownAndInvalid() { QVERIFY(!executeRequest(request("teleport", {})).value("ok").toBool()); QVERIFY(!executeRequest(request("box", {{"dx", -1.0}, {"dy", 1.0}, {"dz", 1.0}})).value("ok").toBool()); }
};
QTEST_MAIN(GeometryWorkerTests)
#include "GeometryWorkerTests.moc"
