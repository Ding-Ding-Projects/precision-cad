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
#include <algorithm>

namespace precision::preferences {
namespace {
constexpr qint64 kMaxBytes = 1024 * 1024;
constexpr int kMaxEntries = 10000;
bool unsafe(const QString &key) { return key.isEmpty() || key == "__proto__" || key == "prototype" || key == "constructor"; }
QString defaultCachePath() { return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("private-vocabulary-cache.json"); }
class JsonShapeScanner {
public:
    explicit JsonShapeScanner(const QByteArray &bytes) : b(bytes) {}
    bool invalid() { ws(); return !value(0) || (ws(), p != b.size()); }
private:
    const QByteArray &b; qsizetype p=0;
    void ws(){ while(p<b.size() && QByteArray(" \t\r\n").contains(b[p])) ++p; }
    bool token(QByteArray *raw=nullptr) { ws(); if(p>=b.size()||b[p]!='\"') return false; const auto start=p++; bool esc=false; while(p<b.size()){ const char c=b[p++]; if(esc){ esc=false; continue; } if(c=='\\') {esc=true; continue;} if(c=='\"'){if(raw)*raw=b.mid(start,p-start); return true;} } return false; }
    bool value(int depth) { ws(); if(p>=b.size()) return false; if(b[p]=='{') return object(depth+1); if(b[p]=='[') return array(depth+1); if(b[p]=='\"') return token(); const auto start=p; while(p<b.size()&&!QByteArray(" \t\r\n,]}").contains(b[p])) ++p; return p>start; }
    bool object(int depth) { if(depth>4 || b[p++]!='{') return false; QSet<QString> seen; ws(); if(p<b.size()&&b[p]=='}'){++p;return true;} while(true){ QByteArray raw; if(!token(&raw))return false; QJsonParseError e; const auto d=QJsonDocument::fromJson("["+raw+"]",&e); if(e.error!=QJsonParseError::NoError||!d.isArray())return false; const auto key=d.array().first().toString(); if(seen.contains(key))return false; seen.insert(key); ws(); if(p>=b.size()||b[p++]!=':')return false; if(!value(depth))return false; ws(); if(p>=b.size())return false; if(b[p]=='}'){++p;return true;} if(b[p++]!=',')return false; }
    }
    bool array(int depth) { if(depth>4||b[p++]!='[')return false; ws(); if(p<b.size()&&b[p]==']'){++p;return true;} while(true){if(!value(depth))return false;ws();if(p>=b.size())return false;if(b[p]==']'){++p;return true;}if(b[p++]!=',')return false;} }
};
}
PersonalVocabularyStore::PersonalVocabularyStore(QString cachePath, QObject *parent) : QObject(parent), m_cachePath(cachePath.isEmpty() ? defaultCachePath() : std::move(cachePath)) {
    QFile cache(m_cachePath); if (cache.exists() && cache.open(QIODevice::ReadOnly)) { const auto bytes=cache.read(kMaxBytes+1); if(bytes.size()<=kMaxBytes) loadBytes(bytes, false); }
}
bool PersonalVocabularyStore::loaded() const { return !m_entries.isEmpty(); }
bool PersonalVocabularyStore::loadFile(const QString &userChosenPath) {
    QFile input(userChosenPath);
    if (!input.open(QIODevice::ReadOnly)) { fail("personal vocabulary file was rejected"); return false; }
    const auto bytes=input.read(kMaxBytes+1); if(bytes.size()>kMaxBytes) { fail("personal vocabulary file was rejected"); return false; }
    return loadBytes(bytes, true);
}
bool PersonalVocabularyStore::loadBytes(const QByteArray &bytes, bool persist) {
    if (JsonShapeScanner(bytes).invalid()) { fail("personal vocabulary shape was rejected"); return false; }
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
bool PersonalVocabularyStore::clear() { const bool was=loaded(); if (QFile::exists(m_cachePath) && !QFile::remove(m_cachePath)) { fail("personal vocabulary cache could not be cleared"); return false; } m_entries.clear(); if(was) emit loadedChanged(); return true; }
QString PersonalVocabularyStore::applyPrivateUiText(const QString &text) const {
    QList<QString> keys=m_entries.keys(); std::sort(keys.begin(),keys.end(),[](const QString&a,const QString&b){ return a.size()==b.size()?a<b:a.size()>b.size(); });
    QString output; output.reserve(text.size());
    const QStringView source(text);
    for (qsizetype i=0;i<text.size();) { const QString *match=nullptr; for(const auto &key:keys) if(source.mid(i,key.size())==key) { match=&key; break; } if(match) { output+=m_entries.value(*match); i+=match->size(); } else output+=text.at(i++); }
    return output;
}
void PersonalVocabularyStore::fail(const QString &message) { emit errorOccurred(message); }
} // namespace precision::preferences
