#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <stdexcept>
#include <optional>

namespace precision::core {

inline constexpr int kDocumentSchemaVersion = 1;
inline constexpr quint64 kMaxDocumentRevision = 9007199254740991ULL;

struct Feature {
    QString id;
    QString type;
    QString label;
    QVector<QString> inputRefs;
    QJsonObject parameters;
    bool suppressed = false;
};

struct DocumentRecord {
    int schemaVersion = kDocumentSchemaVersion;
    QString documentId;
    quint64 revision = 0;
    QString units = QStringLiteral("mm");
    QVector<Feature> features;
};

struct Result {
    bool ok = false;
    QString error;
    static Result success() { return {true, {}}; }
    static Result failure(QString message) { return {false, std::move(message)}; }
};

class Document final {
public:
    // Throws std::invalid_argument when the initial record violates the native record contract.
    explicit Document(DocumentRecord initial);

    const DocumentRecord &record() const noexcept;
    Result addFeature(const Feature &feature, quint64 expectedRevision);
    Result updateFeature(const Feature &feature, quint64 expectedRevision);
    Result removeFeature(const QString &id, quint64 expectedRevision);
    Result suppressFeature(const QString &id, bool suppressed, quint64 expectedRevision);
    Result undo(quint64 expectedRevision);
    Result redo(quint64 expectedRevision);

    static Result validate(const DocumentRecord &record);
    static Result parse(const QByteArray &json, DocumentRecord *out);
    static QByteArray serialize(const DocumentRecord &record);

private:
    Result transact(DocumentRecord candidate, quint64 expectedRevision);
    DocumentRecord m_record;
    QVector<DocumentRecord> m_undo;
    QVector<DocumentRecord> m_redo;
};

class DocumentStorage final {
public:
    // `expectedPersistedRevision` is null only for a new file. Existing files must match it.
    static Result save(const QString &path, const DocumentRecord &record, std::optional<quint64> expectedPersistedRevision);
    static Result load(const QString &path, DocumentRecord *out);
    static Result recover(const QString &path, DocumentRecord *out, bool *usedBackup = nullptr);
};

} // namespace precision::core
