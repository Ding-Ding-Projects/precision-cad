#pragma once

#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
#include <QVector>

namespace precision::history {

struct HistoryError { QString code; QString message; };
struct ProjectStatus { bool repository = false; bool clean = false; QString root; QString branch; QStringList modified; QStringList staged; QStringList untracked; HistoryError error; };
struct HistoryCommit { QString id; QString shortId; QString subject; QString author; QDateTime timestamp; };
struct BranchInfo { QString name; QString id; bool current = false; };
struct CommitAuthor { QString name; QString email; };
struct SemanticChange { QString path; QString kind; QString detail; };
struct SemanticDiff { bool comparable = false; QVector<SemanticChange> changes; HistoryError error; };
struct RestorePreview { bool valid = false; QString sourceRevision; QString relativePath; QString preservedCopy; QByteArray currentSha256; QByteArray historicalSha256; qint64 bytes = 0; HistoryError error; };

class LocalHistoryService final : public QObject {
    Q_OBJECT
public:
    explicit LocalHistoryService(QObject *parent = nullptr);
    bool initialize(const QString &projectDirectory, const QString &gitExecutable = QString());
    [[nodiscard]] QString projectDirectory() const;
    [[nodiscard]] ProjectStatus inspectStatus() const;
    [[nodiscard]] QVector<HistoryCommit> listCommits(int limit = 100) const;
    bool commitSelected(const QStringList &relativePaths, const QString &message, const CommitAuthor &author, HistoryError *error = nullptr);
    [[nodiscard]] QVector<BranchInfo> listBranches(HistoryError *error = nullptr) const;
    bool createBranch(const QString &name, HistoryError *error = nullptr);
    [[nodiscard]] SemanticDiff diffNativeDocuments(const QString &leftPath, const QString &rightPath) const;
    [[nodiscard]] RestorePreview previewRestore(const QString &revision, const QString &relativePath) const;
    bool applyRestore(const RestorePreview &preview, HistoryError *error = nullptr);
private:
    struct ProcessResult { int exitCode = -1; bool timedOut = false; QByteArray output; QByteArray error; };
    [[nodiscard]] ProcessResult git(const QStringList &arguments, int timeoutMs = 10000) const;
    [[nodiscard]] bool validSelectedPath(const QString &relativePath, QString *absolutePath = nullptr, HistoryError *error = nullptr) const;
    [[nodiscard]] bool validateDocument(const QByteArray &bytes, HistoryError *error = nullptr) const;
    [[nodiscard]] bool initialized(HistoryError *error = nullptr) const;
    static void setError(HistoryError *target, QString code, QString message);
    QString m_root;
    QString m_git;
};
} // namespace precision::history
