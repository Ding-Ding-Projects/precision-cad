#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include "history/local_history_service.h"

using namespace precision::history;
class LocalHistoryServiceTest final : public QObject {
    Q_OBJECT
    QString root;
    static bool run(const QStringList &args, const QString &at) { QProcess p; p.setProgram("git"); p.setArguments(QStringList{"-C", at} + args); p.start(); return p.waitForFinished(10000) && p.exitCode() == 0; }
    static QByteArray document(int revision, QString label = "Boss") { return QJsonDocument(QJsonObject{{"schemaVersion",1},{"documentId","doc-1"},{"revision",revision},{"units","mm"},{"features",QJsonArray{QJsonObject{{"id","f1"},{"type","box"},{"label",label}}}}}).toJson(QJsonDocument::Compact); }
    static void write(const QString &path, const QByteArray &bytes) { QFile f(path); QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path)); QCOMPARE(f.write(bytes), bytes.size()); }
private slots:
    void init() { root = QDir::temp().filePath("precision-history-" + QUuid::createUuid().toString(QUuid::WithoutBraces)); QVERIFY(QDir().mkpath(root)); QVERIFY(run({"init"},root)); QVERIFY(run({"config","user.name","Fixture"},root)); QVERIFY(run({"config","user.email","fixture@example.invalid"},root)); write(root+"/model.pcad",document(1)); QVERIFY(run({"add","model.pcad"},root)); QVERIFY(run({"commit","-m","initial"},root)); }
    void cleanup() { QDir(root).removeRecursively(); }
    void commitsAndSemanticDiff() { LocalHistoryService service; QVERIFY(service.initialize(root)); const auto commits=service.listCommits(); QCOMPARE(commits.size(),1); write(root+"/model2.pcad",document(2,"Changed")); SemanticDiff diff=service.diffNativeDocuments("model.pcad","model2.pcad"); QVERIFY(diff.comparable); QVERIFY(!diff.changes.isEmpty()); }
    void selectedCommitAndRefValidation() { LocalHistoryService service; QVERIFY(service.initialize(root)); write(root+"/model.pcad",document(2)); HistoryError error; QVERIFY(service.commitSelected({"model.pcad"},"second",{"Fixture","fixture@example.invalid"},&error)); QVERIFY(error.code.isEmpty()); QVERIFY(!service.listCommits().isEmpty()); QVERIFY(!service.createBranch("bad ref name",&error)); QCOMPARE(error.code,QString("invalid-ref")); QVERIFY(service.createBranch("feature/local",&error)); }
    void rejectsUnsafePathAndExistingStage() { LocalHistoryService service; QVERIFY(service.initialize(root)); HistoryError error; QVERIFY(!service.commitSelected({"../outside.pcad"},"nope",{"Fixture","fixture@example.invalid"},&error)); QCOMPARE(error.code,QString("unsafe-path")); write(root+"/other.pcad",document(3)); QVERIFY(run({"add","other.pcad"},root)); QVERIFY(!service.commitSelected({"model.pcad"},"nope",{"Fixture","fixture@example.invalid"},&error)); QCOMPARE(error.code,QString("unrelated-staged-state")); }
    void restorePreservesCurrentBytes() { LocalHistoryService service; QVERIFY(service.initialize(root)); write(root+"/model.pcad",document(2,"new")); HistoryError error; QVERIFY(service.commitSelected({"model.pcad"},"second",{"Fixture","fixture@example.invalid"},&error)); const auto old = service.listCommits().last().id; write(root+"/model.pcad",document(3,"working")); auto preview=service.previewRestore(old,"model.pcad"); QVERIFY(preview.valid); QVERIFY(service.applyRestore(preview,&error)); QFile restored(root+"/model.pcad"); QVERIFY(restored.open(QIODevice::ReadOnly)); QCOMPARE(restored.readAll(),document(1)); QFile backup(preview.preservedCopy); QVERIFY(backup.open(QIODevice::ReadOnly)); QCOMPARE(backup.readAll(),document(3,"working")); }
};
QTEST_MAIN(LocalHistoryServiceTest)
#include "local_history_service_test.moc"
