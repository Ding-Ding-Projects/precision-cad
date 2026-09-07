#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTemporaryDir>

#include "native_diagnostics.h"

using precision::diagnostics::NativeDiagnosticsCollector;

class NativeDiagnosticsTest final : public QObject {
  Q_OBJECT
private slots:
  void disabledAuditWritesNothing();
  void rejectsMalformedProfile();
  void recordsQtAndQmlCountsWithoutMessageText();
  void preventsSecondCollector();
};

void NativeDiagnosticsTest::disabledAuditWritesNothing() {
  QTemporaryDir profile;
  QVERIFY(profile.isValid());
  qunsetenv("PRECISION_LAYOUT_AUDIT");
  QVERIFY(NativeDiagnosticsCollector::startIfRequested(profile.path(), QStringLiteral("test")) == nullptr);
  QVERIFY(!QFile::exists(profile.filePath(QStringLiteral("native-diagnostics.json"))));
}

void NativeDiagnosticsTest::rejectsMalformedProfile() {
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  QVERIFY(!precision::diagnostics::hasValidatedAuditProfile(QStringLiteral("relative-profile")));
  QVERIFY(NativeDiagnosticsCollector::startIfRequested(QStringLiteral("relative-profile"), QStringLiteral("test")) == nullptr);
  qunsetenv("PRECISION_LAYOUT_AUDIT");
}

void NativeDiagnosticsTest::recordsQtAndQmlCountsWithoutMessageText() {
  QTemporaryDir profile;
  QVERIFY(profile.isValid());
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  const QString syntheticSecret = QStringLiteral("password=not-for-diagnostics");
  qsizetype expectedQmlWarnings = 0;
  {
    std::unique_ptr<NativeDiagnosticsCollector> collector(
        NativeDiagnosticsCollector::startIfRequested(profile.path(), QStringLiteral("test-source")));
    QVERIFY(collector);
    QQmlEngine engine;
    collector->watch(&engine);
    qWarning().noquote() << syntheticSecret;
    qCritical().noquote() << syntheticSecret;
    QQmlComponent component(&engine);
    component.setData("import QtQml 2.15\nNoSuchQmlType {}", QUrl(QStringLiteral("memory:invalid.qml")));
    QTRY_VERIFY(component.isError());
    const QList<QQmlError> invalidQmlErrors = component.errors();
    QVERIFY(!invalidQmlErrors.isEmpty());
    expectedQmlWarnings = invalidQmlErrors.size();
    QVERIFY(QMetaObject::invokeMethod(&engine, "warnings", Qt::DirectConnection,
                                      Q_ARG(QList<QQmlError>, invalidQmlErrors)));
    QTRY_VERIFY(QFile::exists(profile.filePath(QStringLiteral("native-diagnostics.json"))));
    QTRY_VERIFY([&] {
      QFile current(profile.filePath(QStringLiteral("native-diagnostics.json")));
      if (!current.open(QIODevice::ReadOnly)) return false;
      return QJsonDocument::fromJson(current.readAll()).object().value(QStringLiteral("qtCriticals")).toInteger() == 1;
    }());
  }
  qunsetenv("PRECISION_LAYOUT_AUDIT");
  QFile file(profile.filePath(QStringLiteral("native-diagnostics.json")));
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QByteArray contents = file.readAll();
  QVERIFY(!contents.contains(syntheticSecret.toUtf8()));
  const QJsonObject record = QJsonDocument::fromJson(contents).object();
  QCOMPARE(record.value(QStringLiteral("sourceCommit")).toString(), QStringLiteral("test-source"));
  QCOMPARE(record.value(QStringLiteral("qtWarnings")).toInteger(), qint64(1));
  QCOMPARE(record.value(QStringLiteral("qtCriticals")).toInteger(), qint64(1));
  QCOMPARE(record.value(QStringLiteral("qmlWarnings")).toInteger(), qint64(expectedQmlWarnings));
  QVERIFY(record.value(QStringLiteral("startedAtUtc")).toString().endsWith(QLatin1Char('Z')));
  QVERIFY(record.value(QStringLiteral("updatedAtUtc")).toString().endsWith(QLatin1Char('Z')));
  QVERIFY(record.value(QStringLiteral("countsComplete")).toBool());
}

void NativeDiagnosticsTest::preventsSecondCollector() {
  QTemporaryDir profile;
  QVERIFY(profile.isValid());
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  std::unique_ptr<NativeDiagnosticsCollector> first(NativeDiagnosticsCollector::startIfRequested(profile.path(), QStringLiteral("test")));
  QVERIFY(first);
  QVERIFY(NativeDiagnosticsCollector::startIfRequested(profile.path(), QStringLiteral("test")) == nullptr);
  qunsetenv("PRECISION_LAYOUT_AUDIT");
}

QTEST_MAIN(NativeDiagnosticsTest)
#include "tst_native_diagnostics.moc"
