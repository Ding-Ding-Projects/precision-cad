#include "history/local_history_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>

namespace precision::history {
namespace {
constexpr qint64 kMaximumDocumentBytes = 32LL * 1024 * 1024;
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

QString LocalHistoryService::projectDirectory() const { return m_root; }

bool LocalHistoryService::initialized(HistoryError *error) const {
    if (m_root.isEmpty()) { setError(error, QStringLiteral("not-initialized"), QStringLiteral("Choose a Git project directory first.")); return false; }
    return true;
}

LocalHistoryService::ProcessResult LocalHistoryService::git(const QStringList &arguments, int timeoutMs) const {
    ProcessResult result;
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    env.insert(QStringLiteral("GCM_INTERACTIVE"), QStringLiteral("Never"));
    env.insert(QStringLiteral("GIT_ASKPASS"), QStringLiteral(""));
    process.setProcessEnvironment(env);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setProgram(m_git);
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(3000)) { result.error = process.errorString().toUtf8(); return result; }
    if (!process.waitForFinished(timeoutMs)) { result.timedOut = true; process.kill(); process.waitForFinished(2000); return result; }
    result.exitCode = process.exitCode();
    result.output = process.readAllStandardOutput();
    result.error = process.readAllStandardError();
    if (result.output.size() > kMaximumProcessOutput || result.error.size() > kMaximumProcessOutput) {
        result.exitCode = -1; result.error = "Git output exceeded the local history limit.";
    }
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
    if (relativePath.isEmpty() || QDir::isAbsolutePath(relativePath) || relativePath.contains(QRegularExpression(QStringLiteral("(^|[\\/])([.][.]|[.]git)([\\/]|$)")))) {
        setError(error, QStringLiteral("unsafe-path"), QStringLiteral("The selected path is outside the allowed project files.")); return false;
    }
    const QString clean = QDir::cleanPath(relativePath);
    const QString full = QDir(m_root).absoluteFilePath(clean);
    QFileInfo file(full);
    if (!isNativeDocument(clean) || file.isSymLink() || !file.exists() || !file.isFile() || file.size() > kMaximumDocumentBytes || QFileInfo(file.canonicalFilePath()).dir().canonicalPath().isEmpty() || !file.canonicalFilePath().startsWith(m_root + QDir::separator())) {
        setError(error, QStringLiteral("unsupported-path"), QStringLiteral("Select an existing native model file inside this project.")); return false;
    }
    if (absolutePath) *absolutePath = file.canonicalFilePath(); return true;
}

bool LocalHistoryService::validateDocument(const QByteArray &bytes, HistoryError *error) const {
    if (bytes.isEmpty() || bytes.size() > kMaximumDocumentBytes) { setError(error, QStringLiteral("invalid-document-size"), QStringLiteral("The native document exceeds the local history size limit.")); return false; }
    QJsonParseError parse; const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) { setError(error, QStringLiteral("invalid-document-json"), QStringLiteral("The historical native document is not valid JSON.")); return false; }
    const QJsonObject object = doc.object();
    const auto validString = [&object](const char *key) { return object.value(QLatin1String(key)).isString() && !object.value(QLatin1String(key)).toString().isEmpty(); };
    if (!object.value(QStringLiteral("schemaVersion")).isDouble() || !validString("documentId") || !object.value(QStringLiteral("revision")).isDouble() || !validString("units") || !object.value(QStringLiteral("features")).isArray()) {
        setError(error, QStringLiteral("invalid-native-schema"), QStringLiteral("The historical file does not satisfy the native document record boundary.")); return false;
    }
    return true;
}

bool LocalHistoryService::commitSelected(const QStringList &relativePaths, const QString &message, const CommitAuthor &author, HistoryError *error) {
    if (!initialized(error)) return false;
    if (relativePaths.isEmpty() || message.trimmed().isEmpty() || author.name.trimmed().isEmpty() || !author.email.contains('@')) { setError(error, QStringLiteral("invalid-commit-input"), QStringLiteral("Choose native files, a message, and a local author identity.")); return false; }
    const ProjectStatus status = inspectStatus();
    if (!status.error.code.isEmpty()) { if (error) *error = status.error; return false; }
    if (!status.staged.isEmpty()) { setError(error, QStringLiteral("unrelated-staged-state"), QStringLiteral("Commit is refused while any staged project state exists.")); return false; }
    QStringList args{QStringLiteral("-C"), m_root, QStringLiteral("add"), QStringLiteral("--")};
    for (const auto &path : relativePaths) { if (!validSelectedPath(path, nullptr, error)) return false; args << path; }
    auto add = git(args); if (add.exitCode != 0 || add.timedOut) { setError(error, QStringLiteral("stage-failed"), cleanMessage(add.error)); return false; }
    auto diff = git({QStringLiteral("-C"), m_root, QStringLiteral("diff"), QStringLiteral("--cached"), QStringLiteral("--quiet")});
    if (diff.exitCode == 0) return true;
    if (diff.exitCode != 1) { setError(error, QStringLiteral("commit-check-failed"), cleanMessage(diff.error)); return false; }
    auto commit = git({QStringLiteral("-C"), m_root, QStringLiteral("-c"), QStringLiteral("user.name=") + author.name, QStringLiteral("-c"), QStringLiteral("user.email=") + author.email, QStringLiteral("commit"), QStringLiteral("-m"), message});
    if (commit.exitCode != 0 || commit.timedOut) { setError(error, QStringLiteral("commit-failed"), cleanMessage(commit.error)); return false; }
    return true;
}

QVector<BranchInfo> LocalHistoryService::listBranches(HistoryError *error) const {
    QVector<BranchInfo> branches; if (!initialized(error)) return branches;
    auto r = git({QStringLiteral("-C"), m_root, QStringLiteral("for-each-ref"), QStringLiteral("--format=%(refname:short)%x1f%(objectname)%x1f%(HEAD)%x1e"), QStringLiteral("refs/heads")});
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
    QJsonDocument da = QJsonDocument::fromJson(a.readAll()), db = QJsonDocument::fromJson(b.readAll());
    if (!validateDocument(da.toJson(QJsonDocument::Compact), &error) || !validateDocument(db.toJson(QJsonDocument::Compact), &error)) { diff.error = error; return diff; }
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
    auto historical = git({QStringLiteral("-C"), m_root, QStringLiteral("show"), revision + QStringLiteral(":") + relativePath});
    if (historical.exitCode != 0 || historical.timedOut || !validateDocument(historical.output, &error)) { preview.error = error.code.isEmpty() ? HistoryError{QStringLiteral("restore-read-failed"), cleanMessage(historical.error)} : error; return preview; }
    preview.valid=true; preview.sourceRevision=QString::fromUtf8(verify.output).trimmed(); preview.relativePath=relativePath; preview.bytes=historical.output.size(); preview.preservedCopy=current + QStringLiteral(".before-restore-") + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmsszzz")); return preview;
}

bool LocalHistoryService::applyRestore(const RestorePreview &preview, HistoryError *error) {
    if (!initialized(error) || !preview.valid) { setError(error, QStringLiteral("invalid-preview"), QStringLiteral("Create a valid restore preview before applying it.")); return false; }
    QString current; if (!validSelectedPath(preview.relativePath, &current, error)) return false;
    auto verify = git({QStringLiteral("-C"), m_root, QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("--quiet"), preview.sourceRevision + QStringLiteral("^{commit}")});
    if (verify.exitCode != 0 || verify.timedOut || QString::fromUtf8(verify.output).trimmed() != preview.sourceRevision) { setError(error, QStringLiteral("invalid-revision"), QStringLiteral("The restore preview revision is no longer valid.")); return false; }
    auto historical = git({QStringLiteral("-C"), m_root, QStringLiteral("show"), preview.sourceRevision + QStringLiteral(":") + preview.relativePath});
    if (historical.exitCode != 0 || historical.timedOut || !validateDocument(historical.output, error)) { if (error && error->code.isEmpty()) setError(error, QStringLiteral("restore-read-failed"), cleanMessage(historical.error)); return false; }
    if (!QFile::copy(current, preview.preservedCopy)) { setError(error, QStringLiteral("preserve-failed"), QStringLiteral("The current model could not be preserved before restoration.")); return false; }
    QSaveFile out(current); if (!out.open(QIODevice::WriteOnly) || out.write(historical.output) != historical.output.size() || !out.commit()) { out.cancelWriting(); setError(error, QStringLiteral("restore-write-failed"), QStringLiteral("The current model remains preserved, but restoration could not be replaced atomically.")); return false; }
    return true;
}
} // namespace precision::history
