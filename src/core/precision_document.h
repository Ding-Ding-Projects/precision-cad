#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace precision::core {

inline constexpr int kDocumentSchemaVersion = 1;

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
    static Result save(const QString &path, const DocumentRecord &record);
    static Result load(const QString &path, DocumentRecord *out);
    static Result recover(const QString &path, DocumentRecord *out, bool *usedBackup = nullptr);
};

} // namespace precision::core
