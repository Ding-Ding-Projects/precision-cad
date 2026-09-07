#include "history/local_history_service.h"
#include "precision_document.h"

#include <QDir>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QLockFile>
#include <QUuid>
#include <QTemporaryDir>

namespace precision::history {
namespace {
constexpr qint64 kMaximumDocumentBytes = 16LL * 1024 * 1024;
constexpr int kMaximumProcessOutput = 8 * 1024 * 1024;

bool isNativeDocument(const QString &path) {
    const auto lower = path.toLower();
    return lower.endsWith(QStringLiteral(".pcad")) || lower.endsWith(QStringLiteral(".cad.json"));
}
QString cleanMessage(const QByteArray &value) {
    return QString::fromUtf8(value).trimmed().left(2048);
}
}

LocalHistoryService::LocalHistoryService(QObject *parent) : QObject(parent) {}

void LocalHistoryService::setError(HistoryError *target, QString code, QString message) {
    if (target) *target = {std::move(code), std::move(message)};
}

bool LocalHistoryService::initialize(const QString &projectDirectory, const QString &gitExecutable) {
    m_root.clear();
    m_git = gitExecutable.isEmpty() ? QStringLiteral("git") : gitExecutable;
    QFileInfo requested(projectDirectory);
    if (!requested.exists() || !requested.isDir() || requested.isSymLink()) return false;
    const QString candidate = requested.canonicalFilePath();
    if (candidate.isEmpty()) return false;
    const auto result = git({QStringLiteral("-C"), candidate, QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")});
    if (result.exitCode != 0 || result.timedOut) return false;
    QFileInfo reported(QString::fromUtf8(result.output).trimmed());
    if (!reported.exists() || reported.isSymLink() || reported.canonicalFilePath() != candidate) return false;
    m_root = candidate;
    return true;
}

bool LocalHistoryService::createProjectRepository(const QString &directory, const QString &executable, HistoryError *error) {
    m_root.clear(); m_git=executable.isEmpty() ? QStringLiteral("git") : executable;
    if (error) *error={};
    const QFileInfo requested(directory);
    if (!requested.exists() || !requested.isDir() || requested.isSymLink()) { setError(error,"invalid-project-directory","Choose an existing empty or native-model directory."); return false; }
    const QString candidate=requested.canonicalFilePath();
    auto existing=git({"-C",candidate,"rev-parse","--show-toplevel"});
    if (existing.exitCode==0 || existing.timedOut || existing.exitCode!=128) { setError(error,"repository-already-present","This directory already belongs to a repository, or its state cannot be verified."); return false; }
    for (const auto &entry : QDir(candidate).entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System)) {
        QFile file(entry.absoluteFilePath());
        if (!entry.isFile() || entry.isSymLink() || !isNativeDocument(entry.fileName()) || entry.size()>kMaximumDocumentBytes || !file.open(QIODevice::ReadOnly) || !validateDocument(file.read(kMaximumDocumentBytes+1),error)) { if(error && error->code.isEmpty()) setError(error,"unsupported-project-content","New history is limited to empty directories or valid native models."); return false; }
    }
    QTemporaryDir staged(QDir(candidate).filePath(".precision-history-init-XXXXXX"));
    if (!staged.isValid()) { setError(error,"repository-create-failed","Temporary project metadata could not be created."); return false; }
    auto created=git({"init","-b","main",staged.path()});
    if (created.exitCode!=0) { setError(error,"repository-create-failed",cleanMessage(created.error)); return false; }
    existing=git({"-C",candidate,"rev-parse","--show-toplevel"});
    if (existing.exitCode!=128 || !QDir().rename(QDir(staged.path()).filePath(".git"),QDir(candidate).filePath(".git"))) { setError(error,"repository-publication-refused","Project metadata changed while initializing history; existing content was retained."); return false; }
    if (!initialize(candidate,m_git)) { setError(error,"repository-created-attachment-failed","Local metadata was created, but attachment could not be verified."); return false; }
    return true;
}

QString LocalHistoryService::projectDirectory() const { return m_root; }

bool LocalHistoryService::initialized(HistoryError *error) const {
    if (m_root.isEmpty()) { setError(error, QStringLiteral("not-initialized"), QStringLiteral("Choose a Git project directory first.")); return false; }
    return true;
}

LocalHistoryService::ProcessResult LocalHistoryService::git(const QStringList &arguments, int timeoutMs, const QProcessEnvironment &extraEnvironment, const QByteArray &input) const {
    ProcessResult result;
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    env.insert(QStringLiteral("GCM_INTERACTIVE"), QStringLiteral("Never"));
    env.insert(QStringLiteral("GIT_ASKPASS"), QStringLiteral(""));
    for (const auto &key : env.keys()) if (key.startsWith("GIT_")) env.remove(key);
    env.insert("GIT_TERMINAL_PROMPT", "0");
    env.insert("GIT_OPTIONAL_LOCKS", "0");
    env.insert("GIT_LITERAL_PATHSPECS", "1");
    for (const QString &key : extraEnvironment.keys()) env.insert(key, extraEnvironment.value(key));
    process.setProcessEnvironment(env);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setProgram(m_git);
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(3000)) { result.error = process.errorString().toUtf8(); return result; }
    if (!input.isEmpty()) process.write(input);
    process.closeWriteChannel();
    QElapsedTimer timer; timer.start(); bool excessive = false;
    const auto drain = [&]() {
        const auto channel = [&](QProcess::ProcessChannel which, QByteArray &retained) {
            process.setReadChannel(which);
            while (process.bytesAvailable() > 0 && !excessive) {
                const qint64 remaining = kMaximumProcessOutput - retained.size();
                const auto chunk = process.read(qMin<qint64>(65536, remaining + 1));
                if (chunk.size() > remaining) { excessive = true; break; }
                retained += chunk;
            }
        };
        channel(QProcess::StandardOutput, result.output);
        channel(QProcess::StandardError, result.error);
    };
    while (process.state() != QProcess::NotRunning && timer.elapsed() < timeoutMs && !excessive) { process.waitForReadyRead(qMin(50, qMax(1, timeoutMs - int(timer.elapsed())))); drain(); }
    if (process.state() != QProcess::NotRunning) { result.timedOut = !excessive; process.kill(); process.waitForFinished(2000); }
    drain();
    if (excessive) { result.exitCode = -1; result.error = "Git output exceeded the local history limit."; return result; }
    if (result.timedOut) return result;
    result.exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    return result;
}

ProjectStatus LocalHistoryService::inspectStatus() const {
    ProjectStatus status; HistoryError error;
    if (!initialized(&error)) { status.error = error; return status; }
    const auto head = git({QStringLiteral("-C"), m_root, QStringLiteral("symbolic-ref"), QStringLiteral("--short"), QStringLiteral("HEAD")});
    status.repository = true; status.root = m_root; status.branch = head.exitCode == 0 ? QString::fromUtf8(head.output).trimmed() : QStringLiteral("(detached)");
    const auto state = git({QStringLiteral("-C"), m_root, QStringLiteral("status"), QStringLiteral("--porcelain=v1"), QStringLiteral("-z"), QStringLiteral("--untracked-files=all")});
    if (state.exitCode != 0 || state.timedOut) { status.error = {QStringLiteral("status-failed"), cleanMessage(state.error)}; return status; }
    const auto records = state.output.split('\0');
    for (const QByteArray &record : records) {
        if (record.size() < 4) continue;
        const QString path = QString::fromUtf8(record.mid(3));
        const QByteArray xy = record.left(2);
        if (xy == "??") status.untracked << path;
        else { if (xy[0] != ' ') status.staged << path; if (xy[1] != ' ') status.modified << path; }
    }
    status.clean = status.staged.isEmpty() && status.modified.isEmpty() && status.untracked.isEmpty();
    return status;
}

QVector<HistoryCommit> LocalHistoryService::listCommits(int limit) const {
    QVector<HistoryCommit> commits; if (!initialized() || limit < 1) return commits;
    limit = qMin(limit, 200);
    const auto r = git({QStringLiteral("-C"), m_root, QStringLiteral("log"), QStringLiteral("-n"), QString::number(limit), QStringLiteral("--format=%H%x1f%h%x1f%an%x1f%aI%x1f%s%x1e")});
    if (r.exitCode != 0 || r.timedOut) return commits;
    for (const auto &raw : r.output.split('\x1e')) {
        const auto parts = raw.split('\x1f'); if (parts.size() != 5) continue;
        commits.push_back({QString::fromUtf8(parts[0]), QString::fromUtf8(parts[1]), QString::fromUtf8(parts[4]).trimmed(), QString::fromUtf8(parts[2]), QDateTime::fromString(QString::fromUtf8(parts[3]), Qt::ISODate)});
    }
    return commits;
}

bool LocalHistoryService::validSelectedPath(const QString &relativePath, QString *absolutePath, HistoryError *error) const {
    if (relativePath.isEmpty() || relativePath.contains(':') || QDir::isAbsolutePath(relativePath) || relativePath.contains(QRegularExpression(QStringLiteral("(^|[\\/])([.][.]|[.]git)([\\/]|$)"), QRegularExpression::CaseInsensitiveOption))) {
        setError(error, QStringLiteral("unsafe-path"), QStringLiteral("The selected path is outside the allowed project files.")); return false;
    }
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(relativePath));
    QString ancestor = m_root;
    for (const auto &part : clean.split('/')) {
        ancestor = QDir(ancestor).filePath(part);
        if (QFileInfo(ancestor).isSymLink()) { setError(error,"unsafe-path","Symbolic links are not project history inputs."); return false; }
    }
    const QString full = QDir(m_root).absoluteFilePath(clean);
    QFileInfo file(full);
    const QString canonical = file.canonicalFilePath();
    const QString withinRoot = QDir(m_root).relativeFilePath(canonical);
    if (!isNativeDocument(clean) || file.isSymLink() || !file.exists() || !file.isFile() || file.size() > kMaximumDocumentBytes || canonical.isEmpty() || QDir::isAbsolutePath(withinRoot) || withinRoot == QStringLiteral("..") || withinRoot.startsWith(QStringLiteral("../")) || withinRoot.startsWith(QStringLiteral("..\\"))) {
        setError(error, QStringLiteral("unsupported-path"), QStringLiteral("Select an existing native model file inside this project.")); return false;
    }
    if (absolutePath) *absolutePath = canonical; return true;
}

bool LocalHistoryService::validateDocument(const QByteArray &bytes, HistoryError *error) const {
    if (bytes.isEmpty() || bytes.size() > kMaximumDocumentBytes) { setError(error, QStringLiteral("invalid-document-size"), QStringLiteral("The native document exceeds the local history size limit.")); return false; }
    precision::core::DocumentRecord record;
    const auto parsed = precision::core::Document::parse(bytes, &record);
    if (!parsed.ok) { setError(error, QStringLiteral("invalid-native-schema"), parsed.error); return false; }
    return true;
}

bool LocalHistoryService::commitSelected(const QStringList &relativePaths, const QString &message, const CommitAuthor &author, HistoryError *error) {
    if (!initialized(error)) return false;
    if (relativePaths.isEmpty() || message.trimmed().isEmpty() || author.name.trimmed().isEmpty() || !author.email.contains('@')) { setError(error, QStringLiteral("invalid-commit-input"), QStringLiteral("Choose native files, a message, and a local author identity.")); return false; }
    if (error) *error = {};
    QStringList selected;
    for (const auto &path : relativePaths) {
        QString absolute;
        if (!validSelectedPath(path, &absolute, error)) return false;
        QFile file(absolute);
        if (!file.open(QIODevice::ReadOnly) || !validateDocument(file.read(kMaximumDocumentBytes + 1), error)) return false;
        selected << QDir::cleanPath(QDir::fromNativeSeparators(path));
    }
    const auto resolve = [&](const QString &name) {
        auto r = git({"-C", m_root, "rev-parse", "--path-format=absolute", "--git-path", name});
        return r.exitCode == 0 ? QString::fromUtf8(r.output).trimmed() : QString();
    };
    const QString liveIndexPath = resolve("index"), headPath = resolve("HEAD");
    if (liveIndexPath.isEmpty() || headPath.isEmpty()) { setError(error,"index-path-failed","The project metadata paths could not be resolved."); return false; }
    // Use Git's own exclusive lock names, not an unrelated advisory lock.
    struct GitLock {
        QFile file;
        bool owned = false;
        explicit GitLock(const QString &path) : file(path) { owned = file.open(QIODevice::WriteOnly | QIODevice::NewOnly); }
        ~GitLock() { if (owned) { file.close(); file.remove(); } }
    } indexLock(liveIndexPath + ".lock");
    if (!indexLock.owned) { setError(error,"project-locked","Git index or HEAD is busy. No commit was published."); return false; }
    const auto branch = git({"-C",m_root,"symbolic-ref","HEAD"});
    if (branch.exitCode != 0) { setError(error,"detached-head","Select a local branch before committing."); return false; }
    const QString ref = QString::fromUtf8(branch.output).trimmed();
    const auto parentResult = git({"-C",m_root,"rev-parse","--verify","--quiet", "HEAD"});
    const QString parent = parentResult.exitCode == 0 ? QString::fromUtf8(parentResult.output).trimmed() : QString();
    if (parentResult.timedOut || (parentResult.exitCode != 0 && parentResult.exitCode != 1)) { setError(error,"head-read-failed","The current commit could not be resolved."); return false; }
    QTemporaryDir temporary;
    if (!temporary.isValid()) { setError(error,"index-create-failed","A private index could not be created."); return false; }
    const QString indexPath = QDir(temporary.path()).filePath("index");
    QProcessEnvironment isolated; isolated.insert("GIT_INDEX_FILE",indexPath);
    QFile original(liveIndexPath);
    if (original.exists()) {
        if (original.size() > kMaximumProcessOutput || !QFile::copy(liveIndexPath,indexPath)) { setError(error,"index-copy-failed","The current index could not be isolated within the size limit."); return false; }
    } else {
        const auto r = git({"-C",m_root,"read-tree",parent.isEmpty() ? "--empty" : parent},10000,isolated);
        if (r.exitCode != 0) { setError(error,"index-create-failed",cleanMessage(r.error)); return false; }
    }
    QStringList stagedArgs{"-C",m_root,"diff","--cached","--quiet"};
    if (!parent.isEmpty()) stagedArgs << parent;
    auto staged = git(stagedArgs,10000,isolated);
    if (staged.exitCode != 0) { setError(error,"unrelated-staged-state","Commit is refused while staged state exists or cannot be inspected."); return false; }
    QStringList args{"-C",m_root,"add","--"}; args += selected;
    auto add = git(args,10000,isolated);
    if (add.exitCode != 0) { setError(error,"stage-failed",cleanMessage(add.error)); return false; }
    for (const auto &path : selected) {
        auto bytes = git({"-C",m_root,"show",":"+path},10000,isolated);
        if (bytes.exitCode != 0 || !validateDocument(bytes.output,error)) { if (error && error->code.isEmpty()) setError(error,"stage-validation-failed","The staged native record is invalid."); return false; }
    }
    // Compare against the captured parent, never a moving HEAD.
    auto tree = git({"-C",m_root,"write-tree"},10000,isolated);
    if (tree.exitCode != 0) { setError(error,"tree-write-failed",cleanMessage(tree.error)); return false; }
    const QString treeId = QString::fromUtf8(tree.output).trimmed();
    if (!parent.isEmpty()) {
        auto oldTree = git({"-C",m_root,"rev-parse",parent+"^{tree}"});
        if (oldTree.exitCode != 0) { setError(error,"head-read-failed",cleanMessage(oldTree.error)); return false; }
        if (QString::fromUtf8(oldTree.output).trimmed() == treeId) return true;
    }
    QFile nextIndex(indexPath);
    if (!nextIndex.open(QIODevice::ReadOnly) || nextIndex.size() > kMaximumProcessOutput) { setError(error,"index-read-failed","The prepared index cannot be read."); return false; }
    const QByteArray indexBytes = nextIndex.read(kMaximumProcessOutput+1);
    nextIndex.close();
    if (indexBytes.size() > kMaximumProcessOutput) { setError(error,"index-read-failed","The prepared index exceeds the size limit."); return false; }
    QSaveFile publishIndex(liveIndexPath);
    if (!publishIndex.open(QIODevice::WriteOnly) || publishIndex.write(indexBytes) != indexBytes.size()) { setError(error,"index-write-failed","The prepared index cannot be written. No commit was published."); return false; }
    isolated.insert("GIT_AUTHOR_NAME",author.name); isolated.insert("GIT_AUTHOR_EMAIL",author.email);
    isolated.insert("GIT_COMMITTER_NAME",author.name); isolated.insert("GIT_COMMITTER_EMAIL",author.email);
    args = {"-C",m_root,"-c","commit.gpgSign=false","commit-tree",treeId};
    if (!parent.isEmpty()) args << "-p" << parent;
    args << "-m" << message;
    auto commit = git(args,10000,isolated);
    if (commit.exitCode != 0) { setError(error,"commit-failed",cleanMessage(commit.error)); return false; }
    const QString commitId = QString::fromUtf8(commit.output).trimmed();
    const auto stillSelected = git({"-C",m_root,"symbolic-ref","HEAD"});
    if (stillSelected.exitCode != 0 || QString::fromUtf8(stillSelected.output).trimmed()!=ref) { setError(error,"head-changed","The active branch changed before publication."); return false; }
    auto updated = git({"-C",m_root,"-c","core.hooksPath="+temporary.path(),"update-ref","--no-deref","-m","CAD selected-file commit",ref,commitId,parent.isEmpty() ? QString(commitId.size(),'0') : parent});
    if (updated.exitCode != 0) {
        // update-ref may have published before a timeout or an output limit.
        auto observed = git({"-C",m_root,"rev-parse","--verify",ref});
        if (observed.exitCode != 0 || QString::fromUtf8(observed.output).trimmed() != commitId) {
            setError(error,"commit-publication-unconfirmed","Reference publication was refused or cannot be confirmed; recoverable commit: " + commitId + ": " + cleanMessage(updated.error));
            if (error) error->recoverableCommit = commitId;
            return false;
        }
    }
    const auto activeBranch = git({"-C",m_root,"symbolic-ref","HEAD"});
    const auto activeHead = git({"-C",m_root,"rev-parse","HEAD"});
    if (activeBranch.exitCode != 0 || activeHead.exitCode != 0 || QString::fromUtf8(activeBranch.output).trimmed()!=ref || QString::fromUtf8(activeHead.output).trimmed()!=commitId) {
        setError(error,"commit-published-index-pending","Commit published, but HEAD changed before index reconciliation: " + commitId);
        if (error) error->recoverableCommit=commitId;
        return false;
    }
    if (!publishIndex.commit()) {
        setError(error,"commit-published-index-pending","Commit published, but the index could not be reconciled: " + commitId);
        if (error) error->recoverableCommit = commitId;
        return false;
    }
    return true;
}

QVector<BranchInfo> LocalHistoryService::listBranches(HistoryError *error) const {
    QVector<BranchInfo> branches; if (!initialized(error)) return branches;
    auto r = git({QStringLiteral("-C"), m_root, QStringLiteral("for-each-ref"), QStringLiteral("--format=%(refname:short)%1f%(objectname)%1f%(HEAD)%1e"), QStringLiteral("refs/heads")});
    if (r.exitCode != 0 || r.timedOut) { setError(error, QStringLiteral("branches-failed"), cleanMessage(r.error)); return branches; }
    for (const auto &item : r.output.split('\x1e')) { auto p = item.split('\x1f'); if (p.size() == 3) branches.push_back({QString::fromUtf8(p[0]), QString::fromUtf8(p[1]), p[2].trimmed() == "*"}); }
    return branches;
}

bool LocalHistoryService::createBranch(const QString &name, HistoryError *error) {
    if (!initialized(error)) return false;
    auto check = git({QStringLiteral("check-ref-format"), QStringLiteral("--branch"), name});
    if (check.exitCode != 0 || check.timedOut) { setError(error, QStringLiteral("invalid-ref"), QStringLiteral("The branch name is not valid.")); return false; }
    auto r = git({QStringLiteral("-C"), m_root, QStringLiteral("branch"), name});
    if (r.exitCode != 0 || r.timedOut) { setError(error, QStringLiteral("branch-create-failed"), cleanMessage(r.error)); return false; }
    return true;
}

SemanticDiff LocalHistoryService::diffNativeDocuments(const QString &leftPath, const QString &rightPath) const {
    SemanticDiff diff; QString left, right; HistoryError error;
    if (!validSelectedPath(leftPath, &left, &error) || !validSelectedPath(rightPath, &right, &error)) { diff.error = error; return diff; }
    QFile a(left), b(right); if (!a.open(QIODevice::ReadOnly) || !b.open(QIODevice::ReadOnly)) { diff.error = {QStringLiteral("read-failed"), QStringLiteral("The selected native document could not be read.")}; return diff; }
    const auto ab = a.read(kMaximumDocumentBytes+1), bb = b.read(kMaximumDocumentBytes+1);
    if (!validateDocument(ab, &error) || !validateDocument(bb, &error)) { diff.error = error; return diff; }
    const auto da = QJsonDocument::fromJson(ab), db = QJsonDocument::fromJson(bb);
    diff.comparable = true; const auto ao = da.object(), bo = db.object();
    for (const QString key : {QStringLiteral("schemaVersion"), QStringLiteral("documentId"), QStringLiteral("revision"), QStringLiteral("units")}) if (ao.value(key) != bo.value(key)) diff.changes.push_back({key, QStringLiteral("record"), QStringLiteral("Changed native record field")});
    QHash<QString,QJsonObject> af, bf; for (auto v: ao.value(QStringLiteral("features")).toArray()) if (v.isObject()) af.insert(v.toObject().value(QStringLiteral("id")).toString(), v.toObject()); for (auto v: bo.value(QStringLiteral("features")).toArray()) if (v.isObject()) bf.insert(v.toObject().value(QStringLiteral("id")).toString(), v.toObject());
    for (auto it=af.cbegin(); it!=af.cend(); ++it) { if (!bf.contains(it.key())) diff.changes.push_back({QStringLiteral("features/")+it.key(),QStringLiteral("removed"),QStringLiteral("Feature removed")}); else if (it.value()!=bf.value(it.key())) diff.changes.push_back({QStringLiteral("features/")+it.key(),QStringLiteral("changed"),QStringLiteral("Feature parameters, references, or state changed")}); }
    for (auto it=bf.cbegin(); it!=bf.cend(); ++it) if (!af.contains(it.key())) diff.changes.push_back({QStringLiteral("features/")+it.key(),QStringLiteral("added"),QStringLiteral("Feature added")});
    return diff;
}

RestorePreview LocalHistoryService::previewRestore(const QString &revision, const QString &relativePath) const {
    RestorePreview preview; HistoryError error; QString current;
    if (!initialized(&error) || !validSelectedPath(relativePath, &current, &error)) { preview.error = error; return preview; }
    auto verify = git({QStringLiteral("-C"), m_root, QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("--quiet"), revision + QStringLiteral("^{commit}")});
    if (verify.exitCode != 0 || verify.timedOut) { preview.error = {QStringLiteral("invalid-revision"), QStringLiteral("Choose an existing commit revision.")}; return preview; }
    QFile active(current); if (!active.open(QIODevice::ReadOnly)) { preview.error = {QStringLiteral("current-read-failed"), QStringLiteral("The current native document could not be read for restore protection.")}; return preview; }
    const QByteArray activeBytes = active.read(kMaximumDocumentBytes+1);
    if (!validateDocument(activeBytes, &error)) { preview.error = error; return preview; }
    auto historical = git({QStringLiteral("-C"), m_root, QStringLiteral("show"), QString::fromUtf8(verify.output).trimmed() + QStringLiteral(":") + relativePath});
    if (historical.exitCode != 0 || historical.timedOut || !validateDocument(historical.output, &error)) { preview.error = error.code.isEmpty() ? HistoryError{QStringLiteral("restore-read-failed"), cleanMessage(historical.error)} : error; return preview; }
    preview.valid=true; preview.sourceRevision=QString::fromUtf8(verify.output).trimmed(); preview.relativePath=relativePath; preview.bytes=historical.output.size(); preview.currentSha256=QCryptographicHash::hash(activeBytes,QCryptographicHash::Sha256); preview.historicalSha256=QCryptographicHash::hash(historical.output,QCryptographicHash::Sha256); preview.preservedCopy=current + QStringLiteral(".before-restore-") + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmsszzz")) + QStringLiteral("-") + QUuid::createUuid().toString(QUuid::WithoutBraces); return preview;
}

bool LocalHistoryService::applyRestore(const RestorePreview &preview, HistoryError *error) {
    if (!initialized(error) || !preview.valid) { setError(error, QStringLiteral("invalid-preview"), QStringLiteral("Create a valid restore preview before applying it.")); return false; }
    QString current; if (!validSelectedPath(preview.relativePath, &current, error)) return false;
    QLockFile lock(current + QStringLiteral(".precision-history.lock")); lock.setStaleLockTime(0);
    if (!lock.tryLock(1000)) { setError(error, QStringLiteral("restore-locked"), QStringLiteral("The native document is busy. Restore was not applied.")); return false; }
    QFile active(current); if (!active.open(QIODevice::ReadOnly)) { setError(error, QStringLiteral("current-read-failed"), QStringLiteral("The current native document could not be read for restore protection.")); return false; }
    const QByteArray activeBytes = active.read(kMaximumDocumentBytes+1);
    active.close();
    if (QCryptographicHash::hash(activeBytes,QCryptographicHash::Sha256) != preview.currentSha256) { setError(error, QStringLiteral("restore-preview-stale"), QStringLiteral("The native document changed after preview. Create a new restore preview.")); return false; }
    auto verify = git({QStringLiteral("-C"), m_root, QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("--quiet"), preview.sourceRevision + QStringLiteral("^{commit}")});
    if (verify.exitCode != 0 || verify.timedOut || QString::fromUtf8(verify.output).trimmed() != preview.sourceRevision) { setError(error, QStringLiteral("invalid-revision"), QStringLiteral("The restore preview revision is no longer valid.")); return false; }
    auto historical = git({QStringLiteral("-C"), m_root, QStringLiteral("show"), preview.sourceRevision + QStringLiteral(":") + preview.relativePath});
    if (historical.exitCode != 0 || historical.timedOut || !validateDocument(historical.output, error)) { if (error && error->code.isEmpty()) setError(error, QStringLiteral("restore-read-failed"), cleanMessage(historical.error)); return false; }
    if (QCryptographicHash::hash(historical.output,QCryptographicHash::Sha256) != preview.historicalSha256) { setError(error, QStringLiteral("historical-content-changed"), QStringLiteral("The historical restore content changed after preview.")); return false; }
    if (!validateDocument(activeBytes, error)) return false;
    QJsonDocument currentDoc = QJsonDocument::fromJson(activeBytes), restoredDoc = QJsonDocument::fromJson(historical.output);
    if (currentDoc.object().value(QStringLiteral("documentId")) != restoredDoc.object().value(QStringLiteral("documentId"))) { setError(error, QStringLiteral("document-id-mismatch"), QStringLiteral("The historical document does not match the active document identity.")); return false; }
    if (currentDoc.object().value("revision").toDouble() >= double(precision::core::kMaxDocumentRevision)) { setError(error,"revision-limit","The active document revision cannot be advanced."); return false; }
    QJsonObject restoredObject = restoredDoc.object(); restoredObject.insert(QStringLiteral("revision"), currentDoc.object().value(QStringLiteral("revision")).toDouble() + 1.0);
    const QByteArray restoredBytes = QJsonDocument(restoredObject).toJson(QJsonDocument::Compact);
    if (!validateDocument(restoredBytes,error)) return false;
    const QString expectedPrefix = current + ".before-restore-";
    if (!preview.preservedCopy.startsWith(expectedPrefix) || QFileInfo(preview.preservedCopy).absolutePath() != QFileInfo(current).absolutePath()) { setError(error,"invalid-preservation-path","The preservation path does not belong to this model."); return false; }
    QFile backup(preview.preservedCopy); if (!backup.open(QIODevice::WriteOnly | QIODevice::NewOnly) || backup.write(activeBytes) != activeBytes.size()) { setError(error, QStringLiteral("preserve-failed"), QStringLiteral("The current model could not be preserved before restoration.")); return false; }
    backup.close();
    QSaveFile out(current); if (!out.open(QIODevice::WriteOnly) || out.write(restoredBytes) != restoredBytes.size() || !out.commit()) { out.cancelWriting(); setError(error, QStringLiteral("restore-write-failed"), QStringLiteral("The current model remains preserved, but restoration could not be replaced atomically.")); return false; }
    return true;
}
} // namespace precision::history
