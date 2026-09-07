#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#endif
#include "native_diagnostics.h"
using precision::diagnostics::NativeDiagnosticsCollector;
namespace {
const QString source = QStringLiteral("0123456789abcdef0123456789abcdef01234567");
std::atomic<int> priorCalls{0};
std::atomic<int> laterCalls{0};
QtMessageHandler forwardingTarget = nullptr;
std::atomic<bool> reenterOnce{false};
void priorHandler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
  ++priorCalls;
  if (reenterOnce.exchange(false) && forwardingTarget) forwardingTarget(type, context, message);
}
void laterHandler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
  ++laterCalls;
  if (forwardingTarget) forwardingTarget(type, context, message);
}
QJsonObject readRecord(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return QJsonDocument::fromJson(file.readAll()).object();
}
bool writeFile(const QString &path, const QByteArray &bytes) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
#ifdef Q_OS_WIN
struct LockedFile {
  HANDLE handle = INVALID_HANDLE_VALUE;
  explicit LockedFile(const QString &path) {
    handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ,
                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  }
  ~LockedFile() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
#endif
}
class NativeDiagnosticsTest final : public QObject {
  Q_OBJECT
private slots:
  void cleanup() { qunsetenv("PRECISION_LAYOUT_AUDIT"); }
  void disabledAuditWritesNothing();
  void rejectsMalformedProfileAndSource();
  void recordsTailAndQmlWithoutMessageText();
  void preventsSecondCollectorWithoutReplacingDescriptor();
  void activeHeartbeatNeverSeals();
  void oldReceiptCannotBecomeCurrent();
  void finalWriteFailureLeavesUnsealed();
  void fatalChildCannotSeal();
  void concurrentCallbacksAndForwardingSurviveDestruction();
  void reentrantHandlerAndConcurrentFinalization();
  void exactJsonIntegerBoundary();
  void rejectsJunctionProfile();
};
void NativeDiagnosticsTest::disabledAuditWritesNothing() {
  QTemporaryDir profile;
  QVERIFY(profile.isValid());
  qunsetenv("PRECISION_LAYOUT_AUDIT");
  QVERIFY(!NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
  QVERIFY(QDir(profile.path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty());
}
void NativeDiagnosticsTest::rejectsMalformedProfileAndSource() {
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  QVERIFY(!precision::diagnostics::hasValidatedAuditProfile(QStringLiteral("relative-profile")));
  for (const QFileInfo &root : QDir::drives())
    QVERIFY(!precision::diagnostics::hasValidatedAuditProfile(root.absoluteFilePath()));
  QVERIFY(!precision::diagnostics::hasValidatedAuditProfile(QStringLiteral("//localhost/share/profile")));
  QTemporaryDir profile;
  QVERIFY(!NativeDiagnosticsCollector::startIfRequested(profile.path(), QStringLiteral("unavailable")));
  QVERIFY(!NativeDiagnosticsCollector::startIfRequested(profile.path(), QString(40, QLatin1Char('z'))));
}
void NativeDiagnosticsTest::recordsTailAndQmlWithoutMessageText() {
  QTemporaryDir profile;
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  QString path;
  {
    std::unique_ptr<NativeDiagnosticsCollector> collector(NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
    QVERIFY(collector);
    path = collector->receiptPath();
    {
      QQmlEngine engine;
      collector->watch(&engine);
      QList<QQmlError> errors{QQmlError()};
      QVERIFY(QMetaObject::invokeMethod(&engine, "warnings", Qt::DirectConnection, Q_ARG(QList<QQmlError>, errors)));
      QObject::connect(&engine, &QObject::destroyed, [] { qWarning("synthetic-engine-destruction-text"); });
      qCritical("synthetic-sensitive-text");
    }
    // No event-loop turn between this warning and collector destruction.
    qWarning("synthetic-final-tail-text");
  }
  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  QVERIFY(!bytes.contains("synthetic-"));
  const auto record = QJsonDocument::fromJson(bytes).object();
  QCOMPARE(record.value("qtWarnings").toInteger(), 2);
  QCOMPARE(record.value("qtCriticals").toInteger(), 1);
  QCOMPARE(record.value("qmlWarnings").toInteger(), 1);
  QCOMPARE(record.value("sourceCommit").toString(), source);
  QVERIFY(record.value("sealed").toBool());
  QVERIFY(record.value("countsComplete").toBool());
}
void NativeDiagnosticsTest::preventsSecondCollectorWithoutReplacingDescriptor() {
  QTemporaryDir profile;
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  std::unique_ptr<NativeDiagnosticsCollector> first(NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
  QVERIFY(first);
  const auto expected = readRecord(profile.filePath("native-diagnostics-run.json"));
  QVERIFY(!NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
  QCOMPARE(readRecord(profile.filePath("native-diagnostics-run.json")), expected);
}
void NativeDiagnosticsTest::activeHeartbeatNeverSeals() {
  QTemporaryDir profile;
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  std::unique_ptr<NativeDiagnosticsCollector> collector(NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
  QVERIFY(collector);
  const auto initial = readRecord(collector->receiptPath());
  QVERIFY(!initial.value("countsComplete").toBool(true));
  QVERIFY(!initial.value("sealed").toBool(true));
  qWarning("heartbeat-count");
  QTRY_VERIFY_WITH_TIMEOUT(readRecord(collector->receiptPath()).value("heartbeat").toInteger() > initial.value("heartbeat").toInteger(), 3000);
  const auto active = readRecord(collector->receiptPath());
  QVERIFY(!active.value("sealed").toBool(true));
  QCOMPARE(active.value("qtWarnings").toInteger(), 1);
  QVERIFY(collector->finalize());
  QVERIFY(collector->finalize());
  const auto sealed = readRecord(collector->receiptPath());
  QTest::qWait(1100);
  QCOMPARE(readRecord(collector->receiptPath()), sealed);
}
void NativeDiagnosticsTest::oldReceiptCannotBecomeCurrent() {
  QTemporaryDir profile;
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  QVERIFY(writeFile(profile.filePath("native-diagnostics.json"), R"({"countsComplete":true,"sealed":true,"runId":"old"})"));
  const QString descriptor = profile.filePath("native-diagnostics-run.json");
  QVERIFY(writeFile(descriptor, R"({"runId":"old"})"));
#ifdef Q_OS_WIN
  {
    LockedFile locked(descriptor);
    QVERIFY(locked.handle != INVALID_HANDLE_VALUE);
    QVERIFY(!NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
    QCOMPARE(readRecord(descriptor).value("runId").toString(), QStringLiteral("old"));
    const auto runs = QDir(profile.path()).entryList({"native-diagnostics-*"}, QDir::Dirs);
    QCOMPARE(runs.size(), 1);
    QVERIFY(!readRecord(profile.filePath(runs.first() + "/native-diagnostics.json")).value("sealed").toBool(true));
  }
#endif
  std::unique_ptr<NativeDiagnosticsCollector> collector(NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
  QVERIFY(collector);
  const auto expected = readRecord(descriptor);
  QCOMPARE(expected.value("runId").toString(), collector->runId());
  QVERIFY(expected.value("runId").toString() != QStringLiteral("old"));
  QCOMPARE(expected.value("processId").toInteger(), QCoreApplication::applicationPid());
  QCOMPARE(readRecord(collector->receiptPath()).value("startedAtUtc"), expected.value("startedAtUtc"));
  QVERIFY(!readRecord(collector->receiptPath()).value("sealed").toBool(true));
}
void NativeDiagnosticsTest::finalWriteFailureLeavesUnsealed() {
#ifdef Q_OS_WIN
  QTemporaryDir profile;
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  std::unique_ptr<NativeDiagnosticsCollector> collector(NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
  QVERIFY(collector);
  const QString path = collector->receiptPath();
  {
    LockedFile locked(path);
    QVERIFY(locked.handle != INVALID_HANDLE_VALUE);
    QVERIFY(!collector->finalize());
    QVERIFY(!collector->isValid());
  }
  collector.reset();
  QVERIFY(!readRecord(path).value("sealed").toBool(true));
  QVERIFY(!readRecord(path).value("countsComplete").toBool(true));
#else
  QSKIP("Windows file sharing regression.");
#endif
}
void NativeDiagnosticsTest::fatalChildCannotSeal() {
  QTemporaryDir profile;
  QProcess child;
  child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--fatal-child"), profile.path()});
  QVERIFY(child.waitForFinished(10000));
  QVERIFY(child.exitStatus() == QProcess::CrashExit || child.exitCode() != 0);
  const auto expected = readRecord(profile.filePath("native-diagnostics-run.json"));
  QVERIFY(!expected.isEmpty());
  const auto receipt = readRecord(profile.filePath(expected.value("receiptRelativePath").toString()));
  QVERIFY(!receipt.isEmpty());
  QVERIFY(!receipt.value("sealed").toBool(true));
  QVERIFY(!receipt.value("countsComplete").toBool(true));
}
void NativeDiagnosticsTest::concurrentCallbacksAndForwardingSurviveDestruction() {
  QTemporaryDir profile;
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  std::unique_ptr<NativeDiagnosticsCollector> collector(NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
  QVERIFY(collector);
  forwardingTarget = qInstallMessageHandler(&laterHandler);
  QVERIFY(forwardingTarget);
  const int before = priorCalls.load();
  std::vector<std::thread> threads;
  for (int index = 0; index < 4; ++index)
    threads.emplace_back([] { for (int n = 0; n < 250; ++n) qWarning("concurrent-event"); });
  for (auto &thread : threads) thread.join();
  const QString path = collector->receiptPath();
  collector.reset();
  QCOMPARE(readRecord(path).value("qtWarnings").toInteger(), 1000);
  QCOMPARE(priorCalls.load() - before, 1000);
  qWarning("after-destruction");
  QCOMPARE(priorCalls.load() - before, 1001);
  {
    std::unique_ptr<NativeDiagnosticsCollector> next(NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
    QVERIFY(next);
    qWarning("later-chain-next-run");
    QVERIFY(next->finalize());
    QCOMPARE(readRecord(next->receiptPath()).value("qtWarnings").toInteger(), 1);
  }
  QCOMPARE(priorCalls.load() - before, 1002);
  qInstallMessageHandler(forwardingTarget);
}
void NativeDiagnosticsTest::exactJsonIntegerBoundary() {
  QTemporaryDir profile;
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  std::unique_ptr<NativeDiagnosticsCollector> collector(NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
  QVERIFY(collector);
  collector->seedWarningCountForTest(9007199254740990ULL);
  qWarning("last-exact-integer");
  QVERIFY(collector->finalize());
  QCOMPARE(readRecord(collector->receiptPath()).value("qtWarnings").toInteger(), qint64(9007199254740991LL));
  collector.reset();
  collector.reset(NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
  QVERIFY(collector);
  collector->seedWarningCountForTest(9007199254740991ULL);
  qWarning("overflow");
  QVERIFY(!collector->finalize());
  const auto record = readRecord(collector->receiptPath());
  QVERIFY(record.value("droppedEvents").toBool());
  QVERIFY(!record.value("countsComplete").toBool(true));
  QCOMPARE(record.value("qtWarnings").toInteger(), qint64(9007199254740991LL));
}
void NativeDiagnosticsTest::reentrantHandlerAndConcurrentFinalization() {
  QTemporaryDir profile;
  qputenv("PRECISION_LAYOUT_AUDIT", "1");
  std::unique_ptr<NativeDiagnosticsCollector> collector(NativeDiagnosticsCollector::startIfRequested(profile.path(), source));
  QVERIFY(collector);
  forwardingTarget = qInstallMessageHandler(&laterHandler);
  const int before = priorCalls.load();
  reenterOnce = true;
  qWarning("reentrant-event");
  QCOMPARE(priorCalls.load() - before, 1);
  std::atomic<bool> running{true};
  std::atomic<bool> began{false};
  std::thread worker([&] {
    began = true;
    while (running.load()) qWarning("teardown-race-event");
  });
  while (!began.load()) std::this_thread::yield();
  const QString path = collector->receiptPath();
  const bool finalized = collector->finalize();
  collector.reset();
  running = false;
  worker.join();
  qInstallMessageHandler(forwardingTarget);
  QVERIFY(finalized);
  const auto record = readRecord(path);
  QVERIFY(record.value("sealed").toBool());
  QVERIFY(record.value("qtWarnings").toInteger() >= 2);
}
void NativeDiagnosticsTest::rejectsJunctionProfile() {
#ifdef Q_OS_WIN
  QTemporaryDir parent;
  QTemporaryDir destination;
  const QString junction = parent.filePath("junction");
  QProcess process;
  process.start(QStringLiteral("cmd.exe"), {QStringLiteral("/d"), QStringLiteral("/c"), QStringLiteral("mklink"),
      QStringLiteral("/J"), QDir::toNativeSeparators(junction), QDir::toNativeSeparators(destination.path())});
  QVERIFY(process.waitForFinished(5000));
  QCOMPARE(process.exitCode(), 0);
  QVERIFY(!precision::diagnostics::hasValidatedAuditProfile(junction));
  QVERIFY(!precision::diagnostics::hasValidatedAuditProfile(junction + "/child"));
  QVERIFY(RemoveDirectoryW(reinterpret_cast<LPCWSTR>(junction.utf16())));
#else
  QSKIP("Windows junction regression.");
#endif
}
int main(int argc, char **argv) {
#ifdef Q_OS_WIN
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
  QCoreApplication application(argc, argv);
  if (application.arguments().contains(QStringLiteral("--fatal-child"))) {
    qputenv("PRECISION_LAYOUT_AUDIT", "1");
    std::unique_ptr<NativeDiagnosticsCollector> collector(
        NativeDiagnosticsCollector::startIfRequested(application.arguments().last(), source));
    if (!collector) return 2;
    qFatal("synthetic-fatal-event");
  }
  qInstallMessageHandler(&priorHandler);
  NativeDiagnosticsTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "tst_native_diagnostics.moc"

