#include "precision_document.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace precision::core {
namespace {
constexpr int kMaxFeatures = 10000;
constexpr int kMaxRefsPerFeature = 256;
constexpr int kMaxStringLength = 512;
constexpr int kMaxJsonDepth = 16;
constexpr int kMaxJsonItems = 10000;
constexpr qsizetype kMaxDocumentBytes = 16 * 1024 * 1024;

Result checkString(const QString &value, const char *name, bool allowEmpty = false) {
    if ((!allowEmpty && value.isEmpty()) || value.size() > kMaxStringLength)
        return Result::failure(QStringLiteral("Invalid %1").arg(QLatin1String(name)));
    return Result::success();
}

Result validateValue(const QJsonValue &value, int depth) {
    if (depth > kMaxJsonDepth) return Result::failure(QStringLiteral("Parameter nesting exceeds limit"));
    if (value.isDouble() && !std::isfinite(value.toDouble())) return Result::failure(QStringLiteral("Parameters contain a non-finite number"));
    if (value.isString() && value.toString().size() > kMaxStringLength) return Result::failure(QStringLiteral("Parameter string exceeds limit"));
    if (value.isArray()) {
        const auto array = value.toArray();
        if (array.size() > kMaxJsonItems) return Result::failure(QStringLiteral("Parameter array exceeds limit"));
        for (const auto &entry : array) { auto r = validateValue(entry, depth + 1); if (!r.ok) return r; }
    }
    if (value.isObject()) {
        const auto object = value.toObject();
        if (object.size() > kMaxJsonItems) return Result::failure(QStringLiteral("Parameter object exceeds limit"));
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key().size() > kMaxStringLength) return Result::failure(QStringLiteral("Parameter key exceeds limit"));
            auto r = validateValue(it.value(), depth + 1); if (!r.ok) return r;
        }
    }
    return Result::success();
}

QString jsonString(const QString &value) {
    return QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
}

QByteArray valueJson(const QJsonValue &value) {
    if (value.isObject()) {
        const auto object = value.toObject(); QStringList keys = object.keys(); std::sort(keys.begin(), keys.end());
        QByteArray result("{"); bool first = true;
        for (const auto &key : keys) { if (!first) result += ','; first = false; result += jsonString(key).toUtf8(); result += ':'; result += valueJson(object.value(key)); }
        return result + '}';
    }
    if (value.isArray()) { QByteArray result("["); bool first = true; for (const auto &entry : value.toArray()) { if (!first) result += ','; first = false; result += valueJson(entry); } return result + ']'; }
    return QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact).mid(1).chopped(1);
}

int featureIndex(const QVector<Feature> &features, const QString &id) {
    for (int i = 0; i < features.size(); ++i) if (features[i].id == id) return i;
    return -1;
}

Result readBounded(QFile &file, QByteArray *out) {
    const QFileInfo info(file);
    if (info.size() > kMaxDocumentBytes) return Result::failure(QStringLiteral("Document byte size exceeds limit"));
    const auto bytes = file.read(kMaxDocumentBytes + 1);
    if (bytes.size() > kMaxDocumentBytes || !file.atEnd()) return Result::failure(QStringLiteral("Document byte size exceeds limit"));
    if (file.error() != QFile::NoError) return Result::failure(QStringLiteral("Could not read document"));
    *out = bytes;
    return Result::success();
}

// QJsonDocument intentionally normalizes duplicate keys. Reject them before parsing so a later
// field cannot silently replace an earlier document field.
bool duplicateObjectKey(const QByteArray &json) {
    int pos = 0;
    auto whitespace = [&] { while (pos < json.size() && QByteArray(" \t\r\n").contains(json[pos])) ++pos; };
    std::function<bool(int)> value;
    std::function<bool(int)> object = [&](int depth) {
        if (depth > kMaxJsonDepth) return true;
        if (pos >= json.size() || json[pos++] != '{') return false; whitespace(); QSet<QString> keys;
        if (pos < json.size() && json[pos] == '}') { ++pos; return false; }
        while (pos < json.size()) {
            whitespace(); if (pos >= json.size() || json[pos] != '"') return false; const int start = pos++;
            while (pos < json.size() && json[pos] != '"') { if (json[pos] == '\\' && ++pos >= json.size()) return false; ++pos; }
            if (pos >= json.size()) return false; const QByteArray rawToken = json.mid(start, ++pos - start); QJsonParseError keyError; const auto keyDoc = QJsonDocument::fromJson(QByteArray("[") + rawToken + ']', &keyError); if (keyError.error != QJsonParseError::NoError || !keyDoc.isArray() || keyDoc.array().size() != 1 || !keyDoc.array().first().isString()) return false; const QString key = keyDoc.array().first().toString(); if (keys.contains(key)) return true; keys.insert(key); whitespace();
            if (pos >= json.size() || json[pos++] != ':') return false; if (value(depth + 1)) return true; whitespace();
            if (pos >= json.size()) return false; if (json[pos] == '}') { ++pos; return false; } if (json[pos++] != ',') return false;
        }
        return false;
    };
    std::function<bool(int)> array = [&](int depth) { if (depth > kMaxJsonDepth) return true; if (pos >= json.size() || json[pos++] != '[') return false; whitespace(); if (pos < json.size() && json[pos] == ']') { ++pos; return false; } while (pos < json.size()) { if (value(depth + 1)) return true; whitespace(); if (pos >= json.size()) return false; if (json[pos] == ']') { ++pos; return false; } if (json[pos++] != ',') return false; } return false; };
    value = [&](int depth) { whitespace(); if (pos >= json.size()) return false; if (json[pos] == '{') return object(depth); if (json[pos] == '[') return array(depth); if (json[pos] == '"') { ++pos; while (pos < json.size() && json[pos] != '"') { if (json[pos] == '\\' && ++pos >= json.size()) return false; ++pos; } if (pos >= json.size()) return false; ++pos; return false; } while (pos < json.size() && !QByteArray(" \t\r\n,]}").contains(json[pos])) ++pos; return false; };
    return value(0);
}
}

Document::Document(DocumentRecord initial) : m_record(std::move(initial)) {
    const auto result = validate(m_record);
    if (!result.ok) throw std::invalid_argument(result.error.toStdString());
}
const DocumentRecord &Document::record() const noexcept { return m_record; }

Result Document::validate(const DocumentRecord &record) {
    if (record.schemaVersion != kDocumentSchemaVersion) return Result::failure(QStringLiteral("Unsupported schema version"));
    if (record.revision > kMaxDocumentRevision) return Result::failure(QStringLiteral("Revision exceeds deterministic JSON range"));
    if (auto r = checkString(record.documentId, "documentId"); !r.ok) return r;
    if (record.units != QStringLiteral("mm") && record.units != QStringLiteral("in")) return Result::failure(QStringLiteral("Unsupported units"));
    if (record.features.size() > kMaxFeatures) return Result::failure(QStringLiteral("Feature count exceeds limit"));
    QHash<QString, int> indices;
    for (const auto &feature : record.features) {
        if (auto r = checkString(feature.id, "feature id"); !r.ok) return r;
        if (auto r = checkString(feature.type, "feature type"); !r.ok) return r;
        if (auto r = checkString(feature.label, "feature label", true); !r.ok) return r;
        if (indices.contains(feature.id)) return Result::failure(QStringLiteral("Duplicate feature id: %1").arg(feature.id));
        indices.insert(feature.id, indices.size());
        if (feature.inputRefs.size() > kMaxRefsPerFeature) return Result::failure(QStringLiteral("Too many feature references"));
        for (const auto &ref : feature.inputRefs) if (auto r = checkString(ref, "input reference"); !r.ok) return r;
        if (auto r = validateValue(feature.parameters, 0); !r.ok) return r;
    }
    for (const auto &feature : record.features) for (const auto &ref : feature.inputRefs) if (!indices.contains(ref)) return Result::failure(QStringLiteral("Unknown feature reference: %1").arg(ref));
    QVector<int> colors(record.features.size(), 0);
    struct Frame { int index; int nextRef; };
    for (int root = 0; root < record.features.size(); ++root) {
        if (colors[root] != 0) continue;
        QVector<Frame> stack{{root, 0}}; colors[root] = 1;
        while (!stack.isEmpty()) {
            auto &frame = stack.last(); const auto &refs = record.features[frame.index].inputRefs;
            if (frame.nextRef == refs.size()) { colors[frame.index] = 2; stack.removeLast(); continue; }
            const int child = indices.value(refs[frame.nextRef++]);
            if (colors[child] == 1) return Result::failure(QStringLiteral("Cyclic feature dependency at: %1").arg(record.features[child].id));
            if (colors[child] == 0) { colors[child] = 1; stack.push_back({child, 0}); }
        }
    }
    return Result::success();
}

Result Document::transact(DocumentRecord candidate, quint64 expectedRevision) {
    if (expectedRevision != m_record.revision) return Result::failure(QStringLiteral("Stale document revision"));
    if (m_record.revision == kMaxDocumentRevision) return Result::failure(QStringLiteral("Revision limit reached"));
    candidate.revision = m_record.revision + 1;
    if (auto r = validate(candidate); !r.ok) return r;
    m_undo.push_back(m_record); m_redo.clear(); m_record = std::move(candidate); return Result::success();
}
Result Document::addFeature(const Feature &feature, quint64 expectedRevision) { auto c = m_record; c.features.push_back(feature); return transact(std::move(c), expectedRevision); }
Result Document::updateFeature(const Feature &feature, quint64 expectedRevision) { auto c = m_record; const int i = featureIndex(c.features, feature.id); if (i < 0) return Result::failure(QStringLiteral("Feature does not exist")); c.features[i] = feature; return transact(std::move(c), expectedRevision); }
Result Document::removeFeature(const QString &id, quint64 expectedRevision) { auto c = m_record; const int i = featureIndex(c.features, id); if (i < 0) return Result::failure(QStringLiteral("Feature does not exist")); c.features.removeAt(i); return transact(std::move(c), expectedRevision); }
Result Document::suppressFeature(const QString &id, bool suppressed, quint64 expectedRevision) { auto c = m_record; const int i = featureIndex(c.features, id); if (i < 0) return Result::failure(QStringLiteral("Feature does not exist")); c.features[i].suppressed = suppressed; return transact(std::move(c), expectedRevision); }
Result Document::undo(quint64 expectedRevision) { if (expectedRevision != m_record.revision) return Result::failure(QStringLiteral("Stale document revision")); if (m_record.revision == kMaxDocumentRevision) return Result::failure(QStringLiteral("Revision limit reached")); if (m_undo.isEmpty()) return Result::failure(QStringLiteral("Nothing to undo")); m_redo.push_back(m_record); auto next = m_undo.takeLast(); next.revision = m_record.revision + 1; m_record = std::move(next); return Result::success(); }
Result Document::redo(quint64 expectedRevision) { if (expectedRevision != m_record.revision) return Result::failure(QStringLiteral("Stale document revision")); if (m_record.revision == kMaxDocumentRevision) return Result::failure(QStringLiteral("Revision limit reached")); if (m_redo.isEmpty()) return Result::failure(QStringLiteral("Nothing to redo")); m_undo.push_back(m_record); auto next = m_redo.takeLast(); next.revision = m_record.revision + 1; m_record = std::move(next); return Result::success(); }

QByteArray Document::serialize(const DocumentRecord &record) {
    QByteArray result("{\"documentId\":"); result += jsonString(record.documentId).toUtf8(); result += ",\"features\":[";
    for (int i = 0; i < record.features.size(); ++i) { if (i) result += ','; const auto &f = record.features[i]; result += "{\"id\":" + jsonString(f.id).toUtf8() + ",\"inputRefs\":["; for (int j = 0; j < f.inputRefs.size(); ++j) { if (j) result += ','; result += jsonString(f.inputRefs[j]).toUtf8(); } result += "],\"label\":" + jsonString(f.label).toUtf8() + ",\"parameters\":" + valueJson(f.parameters) + ",\"suppressed\":" + (f.suppressed ? "true" : "false") + ",\"type\":" + jsonString(f.type).toUtf8() + '}'; }
    result += "],\"revision\":" + QByteArray::number(record.revision) + ",\"schemaVersion\":" + QByteArray::number(record.schemaVersion) + ",\"units\":" + jsonString(record.units).toUtf8() + '}'; return result;
}
Result Document::parse(const QByteArray &json, DocumentRecord *out) {
    if (!out || json.isEmpty() || json.size() > kMaxDocumentBytes) return Result::failure(QStringLiteral("Invalid document byte size"));
    if (duplicateObjectKey(json)) return Result::failure(QStringLiteral("Duplicate JSON object field"));
    QJsonParseError error; const auto root = QJsonDocument::fromJson(json, &error); if (error.error != QJsonParseError::NoError || !root.isObject()) return Result::failure(QStringLiteral("Malformed document JSON"));
    const auto o = root.object(); const QSet<QString> allowed{"schemaVersion", "documentId", "revision", "units", "features"}; if (o.size() != allowed.size()) return Result::failure(QStringLiteral("Unknown or missing document fields")); for (const auto &key : o.keys()) if (!allowed.contains(key)) return Result::failure(QStringLiteral("Unknown document field"));
    if (!o.value("schemaVersion").isDouble() || !o.value("documentId").isString() || !o.value("revision").isDouble() || !o.value("units").isString() || !o.value("features").isArray()) return Result::failure(QStringLiteral("Document field type mismatch"));
    const double schema = o.value("schemaVersion").toDouble(); const double revision = o.value("revision").toDouble(); if (!std::isfinite(schema) || std::floor(schema) != schema || schema != kDocumentSchemaVersion || !std::isfinite(revision) || revision < 0 || revision > static_cast<double>(kMaxDocumentRevision) || std::floor(revision) != revision) return Result::failure(QStringLiteral("Invalid document version or revision"));
    DocumentRecord parsed; parsed.schemaVersion = static_cast<int>(schema); parsed.documentId = o.value("documentId").toString(); parsed.revision = static_cast<quint64>(revision); parsed.units = o.value("units").toString();
    for (const auto &entry : o.value("features").toArray()) { if (!entry.isObject()) return Result::failure(QStringLiteral("Feature is not an object")); const auto f = entry.toObject(); const QSet<QString> fields{"id","type","label","inputRefs","parameters","suppressed"}; if (f.size() != fields.size()) return Result::failure(QStringLiteral("Unknown or missing feature fields")); for (const auto &key : f.keys()) if (!fields.contains(key)) return Result::failure(QStringLiteral("Unknown feature field")); if (!f.value("id").isString() || !f.value("type").isString() || !f.value("label").isString() || !f.value("inputRefs").isArray() || !f.value("parameters").isObject() || !f.value("suppressed").isBool()) return Result::failure(QStringLiteral("Feature field type mismatch")); Feature feature{f.value("id").toString(), f.value("type").toString(), f.value("label").toString(), {}, f.value("parameters").toObject(), f.value("suppressed").toBool()}; for (const auto &ref : f.value("inputRefs").toArray()) { if (!ref.isString()) return Result::failure(QStringLiteral("Feature reference is not a string")); feature.inputRefs.push_back(ref.toString()); } parsed.features.push_back(std::move(feature)); }
    if (auto r = validate(parsed); !r.ok) return r; *out = std::move(parsed); return Result::success();
}

Result DocumentStorage::save(const QString &path, const DocumentRecord &record, std::optional<quint64> expectedPersistedRevision) {
    if (auto r = Document::validate(record); !r.ok) return r;
    QLockFile lock(path + QStringLiteral(".lock")); lock.setStaleLockTime(0); if (!lock.tryLock(0)) return Result::failure(QStringLiteral("Document is locked by another writer"));
    QFile current(path); if (current.exists()) { if (!current.open(QIODevice::ReadOnly)) return Result::failure(QStringLiteral("Could not read existing document")); QByteArray old; if (auto r = readBounded(current, &old); !r.ok) return r; current.close(); DocumentRecord parsed; if (auto r = Document::parse(old, &parsed); !r.ok) return Result::failure(QStringLiteral("Refusing to overwrite invalid existing document: %1").arg(r.error)); if (!expectedPersistedRevision || parsed.revision != *expectedPersistedRevision) return Result::failure(QStringLiteral("Persisted document revision changed")); if (parsed.documentId != record.documentId) return Result::failure(QStringLiteral("Persisted document identity changed")); const QByteArray next = Document::serialize(record); if (record.revision < parsed.revision) return Result::failure(QStringLiteral("Document revision must advance")); if (record.revision == parsed.revision) return next == old ? Result::success() : Result::failure(QStringLiteral("Same revision has different document bytes")); QSaveFile backup(path + QStringLiteral(".bak")); if (!backup.open(QIODevice::WriteOnly) || backup.write(old) != old.size() || !backup.commit()) return Result::failure(QStringLiteral("Could not preserve valid backup")); }
    else if (expectedPersistedRevision) return Result::failure(QStringLiteral("Expected persisted document does not exist"));
    QSaveFile file(path); const auto bytes = Document::serialize(record); if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return Result::failure(QStringLiteral("Atomic save failed: %1").arg(file.errorString()));
    return Result::success();
}
Result DocumentStorage::load(const QString &path, DocumentRecord *out) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return Result::failure(QStringLiteral("Could not open document")); QByteArray bytes; if (auto r = readBounded(f, &bytes); !r.ok) return r; return Document::parse(bytes, out); }
Result DocumentStorage::recover(const QString &path, DocumentRecord *out, bool *usedBackup) { if (usedBackup) *usedBackup = false; auto r = load(path, out); if (r.ok) return r; r = load(path + QStringLiteral(".bak"), out); if (r.ok && usedBackup) *usedBackup = true; return r.ok ? r : Result::failure(QStringLiteral("Neither document nor backup is valid")); }
} // namespace precision::core
