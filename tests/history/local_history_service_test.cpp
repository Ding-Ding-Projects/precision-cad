#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <cstdio>
#include "precision_document.h"
#include "history/local_history_service.h"

using namespace precision::history;
class LocalHistoryServiceTest final : public QObject {
    Q_OBJECT
    QString root;
    static QByteArray read(const QString &path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
    static QByteArray output(const QStringList &args,const QString &at) { QProcess p; p.start("git",QStringList{"-C",at}+args); if (!p.waitForFinished(10000) || p.exitCode()!=0) return {}; return p.readAllStandardOutput().trimmed(); }
    void mode(const QByteArray &name) { write(root+"/.git/history-fixture-mode",name); }
    bool commit(LocalHistoryService &s, HistoryError *e) { return s.commitSelected({"model.pcad"},"selected",{"Fixture","fixture@example.invalid"},e); }
    static bool run(const QStringList &args, const QString &at) { QProcess p; p.setProgram("git"); p.setArguments(QStringList{"-C", at} + args); p.start(); return p.waitForFinished(10000) && p.exitCode() == 0; }
    static QByteArray document(int revision, QString label = "Boss") { return QJsonDocument(QJsonObject{{"schemaVersion",1},{"documentId","doc-1"},{"revision",revision},{"units","mm"},{"features",QJsonArray{QJsonObject{{"id","f1"},{"type","box"},{"label",label},{"inputRefs",QJsonArray{}},{"parameters",QJsonObject{}},{"suppressed",false}}}}}).toJson(QJsonDocument::Compact); }
    static void write(const QString &path, const QByteArray &bytes) { QFile f(path); QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path)); QCOMPARE(f.write(bytes), bytes.size()); }
private slots:
    void init() { root = QDir::temp().filePath("precision-history-" + QUuid::createUuid().toString(QUuid::WithoutBraces)); QVERIFY(QDir().mkpath(root)); QVERIFY(run({"init"},root)); QVERIFY(run({"config","user.name","Fixture"},root)); QVERIFY(run({"config","user.email","fixture@example.invalid"},root)); write(root+"/model.pcad",document(1)); QVERIFY(run({"add","model.pcad"},root)); QVERIFY(run({"commit","-m","initial"},root)); }
    void cleanup() { QDir(root).removeRecursively(); }
    void commitsAndSemanticDiff() { LocalHistoryService service; QVERIFY(service.initialize(root)); const auto commits=service.listCommits(); QCOMPARE(commits.size(),1); write(root+"/model2.pcad",document(2,"Changed")); SemanticDiff diff=service.diffNativeDocuments("model.pcad","model2.pcad"); QVERIFY2(diff.comparable, qPrintable(diff.error.code + ": " + diff.error.message)); QVERIFY(!diff.changes.isEmpty()); }
    void selectedCommitAndRefValidation() { LocalHistoryService service; QVERIFY(service.initialize(root)); write(root+"/model.pcad",document(2)); HistoryError error; QVERIFY2(service.commitSelected({"model.pcad"},"second",{"Fixture","fixture@example.invalid"},&error),qPrintable(error.code + ": " + error.message)); QVERIFY(error.code.isEmpty()); QVERIFY(service.listCommits().size() >= 2); QVERIFY(!service.createBranch("bad ref name",&error)); QCOMPARE(error.code,QString("invalid-ref")); QVERIFY(service.createBranch("feature/local",&error)); }
    void rejectsUnsafePathAndExistingStage() { LocalHistoryService service; QVERIFY(service.initialize(root)); HistoryError error; QVERIFY(!service.commitSelected({"../outside.pcad"},"nope",{"Fixture","fixture@example.invalid"},&error)); QCOMPARE(error.code,QString("unsafe-path")); write(root+"/other.pcad",document(3)); QVERIFY(run({"add","other.pcad"},root)); QVERIFY(!service.commitSelected({"model.pcad"},"nope",{"Fixture","fixture@example.invalid"},&error)); QCOMPARE(error.code,QString("unrelated-staged-state")); }
    void restorePreservesCurrentBytes() { LocalHistoryService service; QVERIFY(service.initialize(root)); write(root+"/model.pcad",document(2,"new")); HistoryError error; QVERIFY(service.commitSelected({"model.pcad"},"second",{"Fixture","fixture@example.invalid"},&error)); write(root+"/model.pcad",document(3,"working")); auto preview=service.previewRestore("HEAD~1","model.pcad"); QVERIFY2(preview.valid,qPrintable(preview.error.code + ": " + preview.error.message)); QVERIFY2(service.applyRestore(preview,&error),qPrintable(error.code + ": " + error.message)); QFile restored(root+"/model.pcad"); QVERIFY(restored.open(QIODevice::ReadOnly)); const auto restoredRecord=QJsonDocument::fromJson(restored.readAll()).object(); QCOMPARE(restoredRecord.value("documentId").toString(),QString("doc-1")); QCOMPARE(restoredRecord.value("revision").toInt(),4); QFile backup(preview.preservedCopy); QVERIFY(backup.open(QIODevice::ReadOnly)); QCOMPARE(backup.readAll(),document(3,"working")); }
    void staleRestorePreviewRefusesOverwrite() { LocalHistoryService service; QVERIFY(service.initialize(root)); auto preview=service.previewRestore("HEAD","model.pcad"); QVERIFY(preview.valid); write(root+"/model.pcad",document(2,"edited after preview")); HistoryError error; QVERIFY(!service.applyRestore(preview,&error)); QCOMPARE(error.code,QString("restore-preview-stale")); }
    void coherentFirstAndSubsequentCommit() {
        QTemporaryDir empty; QVERIFY(run({"init"},empty.path()));
        LocalHistoryService s; QVERIFY(s.initialize(empty.path()));
        write(empty.path()+"/model.pcad",document(1)); HistoryError e;
        QVERIFY2(commit(s,&e),qPrintable(e.code+e.message));
        QCOMPARE(output({"status","--porcelain"},empty.path()),QByteArray());
        write(empty.path()+"/model.pcad",document(2)); QVERIFY2(commit(s,&e),qPrintable(e.code+e.message));
        QCOMPARE(output({"status","--porcelain"},empty.path()),QByteArray());
        const auto id=output({"rev-parse","HEAD"},empty.path());
        QVERIFY(commit(s,&e)); QCOMPARE(output({"rev-parse","HEAD"},empty.path()),id);
    }
    void failurePreservesIndexAndWorkingBytes() {
        LocalHistoryService s; QVERIFY(s.initialize(root,QCoreApplication::applicationFilePath()));
        write(root+"/model.pcad",document(2)); const auto index=read(root+"/.git/index"), bytes=read(root+"/model.pcad"), head=output({"rev-parse","HEAD"},root);
        mode("fail-commit"); HistoryError e; QVERIFY(!commit(s,&e)); QCOMPARE(e.code,QString("commit-failed"));
        QCOMPARE(read(root+"/.git/index"),index); QCOMPARE(read(root+"/model.pcad"),bytes); QCOMPARE(output({"rev-parse","HEAD"},root),head);
        QVERIFY(!QFile::exists(root+"/.git/index.lock")); QVERIFY(!QFile::exists(root+"/.git/HEAD.lock"));
    }
    void concurrentStageCannotEnterOrOverwriteTransaction() {
        LocalHistoryService s; QVERIFY(s.initialize(root,QCoreApplication::applicationFilePath()));
        write(root+"/model.pcad",document(2)); write(root+"/other.pcad",document(3)); mode("stage-during"); HistoryError e;
        QVERIFY2(commit(s,&e),qPrintable(e.code+e.message)); QCOMPARE(read(root+"/.git/concurrent-stage-exit"),QByteArray("128"));
        QCOMPARE(output({"ls-tree","--name-only","HEAD"},root),QByteArray("model.pcad")); QCOMPARE(read(root+"/other.pcad"),document(3));
        QVERIFY(run({"add","other.pcad"},root)); const auto staged=read(root+"/.git/index");
        QVERIFY(!commit(s,&e)); QCOMPARE(e.code,QString("unrelated-staged-state")); QCOMPARE(read(root+"/.git/index"),staged);
    }
    void concurrentHeadIsPreserved() {
        LocalHistoryService s; QVERIFY(s.initialize(root,QCoreApplication::applicationFilePath()));
        write(root+"/model.pcad",document(2)); const auto original=read(root+"/.git/index"); mode("advance-head"); HistoryError e;
        QVERIFY(!commit(s,&e)); QCOMPARE(e.code,QString("commit-publication-unconfirmed")); QVERIFY(!e.recoverableCommit.isEmpty());
        QCOMPARE(output({"rev-parse","HEAD"},root),read(root+"/.git/concurrent-head")); QCOMPARE(read(root+"/.git/index"),original);
        QCOMPARE(output({"log","-1","--format=%s"},root),QByteArray("concurrent"));
    }
    void primaryLockContentionPreservesBytes() {
        LocalHistoryService s; QVERIFY(s.initialize(root)); write(root+"/model.pcad",document(2));
        const auto index=read(root+"/.git/index"); write(root+"/.git/index.lock","owned elsewhere"); HistoryError e;
        QVERIFY(!commit(s,&e)); QCOMPARE(e.code,QString("project-locked")); QCOMPARE(read(root+"/.git/index"),index); QCOMPARE(read(root+"/.git/index.lock"),QByteArray("owned elsewhere"));
    }
    void configuredHooksAreNotExecuted() {
        write(root+"/.git/hooks/pre-commit","#!/bin/sh\nexit 1\n"); write(root+"/.git/hooks/reference-transaction","#!/bin/sh\nexit 1\n");
        LocalHistoryService s; QVERIFY(s.initialize(root)); write(root+"/model.pcad",document(2)); HistoryError e;
        QVERIFY2(commit(s,&e),qPrintable(e.code+e.message)); QCOMPARE(output({"status","--porcelain"},root),QByteArray());
    }
    void rawInvalidRecordsRejectedEverywhere() {
        const QList<QByteArray> bad{document(2).replace("\"revision\":2","\"revision\":2,\"revision\":3"), document(2).replace("\"revision\":2","\"revision\":9007199254740992"),document(2)+" trailing"};
        LocalHistoryService s; QVERIFY(s.initialize(root));
        for (const auto &bytes : bad) {
            write(root+"/invalid.pcad",bytes); QVERIFY(!s.diffNativeDocuments("model.pcad","invalid.pcad").comparable);
            QVERIFY(run({"add","invalid.pcad"},root)); QVERIFY(run({"commit","-m","invalid historical fixture"},root));
            write(root+"/invalid.pcad",document(3)); QVERIFY(!s.previewRestore("HEAD","invalid.pcad").valid);
            write(root+"/invalid.pcad",bytes); HistoryError e; QVERIFY(!s.commitSelected({"invalid.pcad"},"invalid",{"Fixture","fixture@example.invalid"},&e));
        }
    }
    void revisionLimitAndForgedPreservationRefused() {
        LocalHistoryService s; QVERIFY(s.initialize(root));
        auto max=QJsonDocument::fromJson(document(2)).object(); max.insert("revision",double(precision::core::kMaxDocumentRevision)); write(root+"/model.pcad",QJsonDocument(max).toJson());
        auto preview=s.previewRestore("HEAD","model.pcad"); QVERIFY(preview.valid); const auto bytes=read(root+"/model.pcad"); HistoryError e;
        QVERIFY(!s.applyRestore(preview,&e)); QCOMPARE(e.code,QString("revision-limit")); QCOMPARE(read(root+"/model.pcad"),bytes); QVERIFY(!QFile::exists(preview.preservedCopy));
        write(root+"/model.pcad",document(3)); preview=s.previewRestore("HEAD","model.pcad"); preview.preservedCopy=root+"/unrelated";
        QVERIFY(!s.applyRestore(preview,&e)); QCOMPARE(e.code,QString("invalid-preservation-path")); QVERIFY(!QFile::exists(preview.preservedCopy));
    }
    void createOnlyExplicitNativeProject() {
        LocalHistoryService s; QTemporaryDir empty; HistoryError e;
        write(empty.path()+"/new.pcad",document(1)); QVERIFY2(s.createProjectRepository(empty.path(),{},&e),qPrintable(e.code+e.message));
        QVERIFY(s.inspectStatus().repository); QCOMPARE(read(empty.path()+"/new.pcad"),document(1));
        QVERIFY(!s.createProjectRepository(empty.path(),{},&e));
        QVERIFY(QDir(root).mkdir("nested")); QVERIFY(!s.createProjectRepository(root+"/nested",{},&e)); QVERIFY(!QFile::exists(root+"/nested/.git"));
        QTemporaryDir unrelated; write(unrelated.path()+"/private.txt","unrelated"); QVERIFY(!s.createProjectRepository(unrelated.path(),{},&e)); QVERIFY(!QFile::exists(unrelated.path()+"/.git"));
    }
    void publishedCommitReportsIndexReconciliationFailure() {
        LocalHistoryService s; QVERIFY(s.initialize(root,QCoreApplication::applicationFilePath()));
        write(root+"/model.pcad",document(2)); mode("break-index-publication"); HistoryError e; QVERIFY(!commit(s,&e));
        QCOMPARE(e.code,QString("commit-published-index-pending")); QCOMPARE(output({"rev-parse","HEAD"},root),e.recoverableCommit.toUtf8()); QCOMPARE(read(root+"/model.pcad"),document(2));
    }
    void boundedProcessFlood() {
        for (const QByteArray channel : {QByteArray("flood-out"),QByteArray("flood-err")}) {
            mode(channel); LocalHistoryService s; QElapsedTimer timer; timer.start(); QVERIFY(!s.initialize(root,QCoreApplication::applicationFilePath())); QVERIFY(timer.elapsed()<10000);
        }
    }
};

// A disposable executable adapter gives deterministic interleavings without modifying production code.
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); const auto args=app.arguments().mid(1);
    if (args.contains("-C")) {
        const QString root=args.value(args.indexOf("-C")+1);
        QFile modeFile(root+"/.git/history-fixture-mode"); QByteArray mode; if (modeFile.open(QIODevice::ReadOnly)) mode=modeFile.readAll(); modeFile.close();
        const auto save=[&](const QString &name,const QByteArray &bytes) { QFile f(root+"/.git/"+name); if(f.open(QIODevice::WriteOnly)) f.write(bytes); };
        const auto real=[&](QStringList a) { QProcess p; auto env=QProcessEnvironment::systemEnvironment(); env.remove("GIT_INDEX_FILE"); p.setProcessEnvironment(env); p.start("git",QStringList{"-C",root}+a); p.waitForFinished(10000); return qMakePair(p.exitCode(),p.readAllStandardOutput().trimmed()); };
        if(mode.startsWith("flood-")) { QByteArray bytes(65536,'x'); for(int i=0;i<2000;++i) { auto f=mode=="flood-out" ? stdout : stderr; std::fwrite(bytes.data(),1,size_t(bytes.size()),f); std::fflush(f); } return 0; }
        if(args.contains("commit-tree") && mode=="fail-commit") return 42;
        if(args.contains("write-tree") && mode=="stage-during") { save("history-fixture-mode",""); save("concurrent-stage-exit",QByteArray::number(real({"add","other.pcad"}).first)); }
        if(args.contains("commit-tree") && mode=="advance-head") {
            save("history-fixture-mode",""); const auto head=real({"rev-parse","HEAD"}).second; const auto tree=real({"rev-parse","HEAD^{tree}"}).second;
            const auto newer=real({"commit-tree",QString::fromUtf8(tree),"-p",QString::fromUtf8(head),"-m","concurrent"}).second;
            const auto ref=real({"symbolic-ref","HEAD"}).second;
            if(real({"update-ref",QString::fromUtf8(ref),QString::fromUtf8(newer),QString::fromUtf8(head)}).first!=0) return 43;
            save("concurrent-head",newer);
        }
        QProcess p; p.start("git",args);
        if(args.contains("--stdin")) { QFile in; in.open(stdin,QIODevice::ReadOnly); p.write(in.readAll()); }
        p.closeWriteChannel(); if(!p.waitForFinished(10000)) return 44;
        if(args.contains("update-ref") && mode=="break-index-publication" && p.exitCode()==0) { QFile::remove(root+"/.git/index"); QDir().mkdir(root+"/.git/index"); }
        const auto out=p.readAllStandardOutput(),err=p.readAllStandardError(); std::fwrite(out.data(),1,size_t(out.size()),stdout); std::fwrite(err.data(),1,size_t(err.size()),stderr); return p.exitCode();
    }
    LocalHistoryServiceTest test; return QTest::qExec(&test,argc,argv);
}
#include "local_history_service_test.moc"
