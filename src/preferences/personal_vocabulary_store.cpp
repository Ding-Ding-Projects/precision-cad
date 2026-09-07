#include "personal_vocabulary_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSet>
#include <QRegularExpression>

namespace precision::preferences {
namespace {
constexpr qint64 kMaxBytes = 1024 * 1024;
constexpr int kMaxEntries = 10000;
bool unsafe(const QString &key) { return key.isEmpty() || key == "__proto__" || key == "prototype" || key == "constructor"; }
QString defaultCachePath() { return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("private-vocabulary-cache.json"); }
bool duplicateDecodedKeys(const QByteArray &bytes) {
    static const QRegularExpression key(QStringLiteral("\"((?:\\\\.|[^\"\\\\])*)\"\\s*:"));
    QSet<QString> seen; const auto matches=key.globalMatch(QString::fromUtf8(bytes));
    auto iterator=matches;
    while(iterator.hasNext()) { const auto capture=iterator.next().captured(0); QJsonParseError error; const auto decoded=QJsonDocument::fromJson(("["+capture.left(capture.lastIndexOf(':')).trimmed()+"]").toUtf8(),&error); if(error.error!=QJsonParseError::NoError || !decoded.isArray()) return true; const auto value=decoded.array().first().toString(); if(seen.contains(value)) return true; seen.insert(value); }
    return false;
}
}
PersonalVocabularyStore::PersonalVocabularyStore(QString cachePath, QObject *parent) : QObject(parent), m_cachePath(cachePath.isEmpty() ? defaultCachePath() : std::move(cachePath)) {
    QFile cache(m_cachePath); if (cache.exists() && cache.open(QIODevice::ReadOnly) && cache.size() <= kMaxBytes) loadBytes(cache.readAll(), false);
}
bool PersonalVocabularyStore::loaded() const { return !m_entries.isEmpty(); }
bool PersonalVocabularyStore::loadFile(const QString &userChosenPath) {
    QFile input(userChosenPath);
    if (!input.open(QIODevice::ReadOnly) || input.size() > kMaxBytes) { fail("personal vocabulary file was rejected"); return false; }
    return loadBytes(input.readAll(), true);
}
bool PersonalVocabularyStore::loadBytes(const QByteArray &bytes, bool persist) {
    if (duplicateDecodedKeys(bytes)) { fail("personal vocabulary duplicate keys were rejected"); return false; }
    QJsonParseError error; const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) { fail("personal vocabulary data was rejected"); return false; }
    const auto root=document.object(); const QSet<QString> allowed{"schemaVersion", "entries"};
    const auto keys=root.keys(); if (QSet<QString>(keys.cbegin(),keys.cend()) != allowed || !root.value("schemaVersion").isDouble() || root.value("schemaVersion").toInt() != 1 || !root.value("entries").isObject()) { fail("personal vocabulary schema was rejected"); return false; }
    const auto entries=root.value("entries").toObject(); if (entries.size() > kMaxEntries) { fail("personal vocabulary has too many entries"); return false; }
    QHash<QString,QString> candidate;
    for (auto it=entries.begin(); it!=entries.end(); ++it) { if (unsafe(it.key()) || it.key().size()>256 || !it.value().isString() || it.value().toString().size()>512) { fail("personal vocabulary entry was rejected"); return false; } candidate.insert(it.key(),it.value().toString()); }
    if (persist) { QDir().mkpath(QFileInfo(m_cachePath).absolutePath()); QSaveFile output(m_cachePath); if (!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit()) { fail("personal vocabulary cache write failed"); return false; } }
    const bool changed = m_entries != candidate; m_entries=std::move(candidate); if(changed) emit loadedChanged(); return true;
}
bool PersonalVocabularyStore::clear() { const bool was=loaded(); m_entries.clear(); QFile::remove(m_cachePath); if(was) emit loadedChanged(); return true; }
QString PersonalVocabularyStore::applyPrivateUiText(const QString &text) const { QString result=text; for(auto it=m_entries.cbegin();it!=m_entries.cend();++it) result.replace(it.key(),it.value()); return result; }
void PersonalVocabularyStore::fail(const QString &message) { emit errorOccurred(message); }
} // namespace precision::preferences
