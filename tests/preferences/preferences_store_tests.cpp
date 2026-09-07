#include "preferences_store.h"
#include "personal_vocabulary_store.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QTemporaryDir>
#include <QtTest>
#include "json_shape_scanner.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif

using precision::preferences::PreferencesStore;
class PreferencesStoreTests final : public QObject {
    Q_OBJECT
private slots:
 void emptyVocabularyTransitions(); void emptyVocabularyLockedClear();
 void vocabularyWriteRefusal(); void vocabularyStartupRejectsDepth(); void fractionalTone_data(); void fractionalTone(); void settingsDuplicates_data(); void settingsDuplicates(); void deepUnknownFields(); void scannerScopeAndDepth(); void oversizedVocabularyStartup(); void vocabularyReloadPreservesBytes_data(); void vocabularyReloadPreservesBytes(); void vocabularyLockedClear(); void publicExportAllowlist(); void exactBytesPersisted(); void preferencesReadLimits(); void resetStaleAndEvidenceSafety();
 void persistsValidState(); void rejectsInvalidWithoutChange(); void corruptionRetainsDefaults(); void corruptStateRequiresReset(); void resetPreservesCorruptEvidence(); void writerLockPreservesMemory(); void exportOmitsVoiceIds(); void staleWriterRejected(); void vocabularyLoadsAndClears(); void vocabularyRejectsDuplicateDecodedKey(); void vocabularyAllowsSameKeyAcrossObjects(); void vocabularyUsesOriginalTextAndLongestKey(); void boundedAndFractionalInputs();
};
QString path(const QTemporaryDir &dir) { return dir.filePath("preferences.json"); }
void PreferencesStoreTests::persistsValidState() { QTemporaryDir dir; PreferencesStore a(path(dir)); QVERIFY(a.setLanguageMode("both")); QVERIFY2(a.setEnglishTone(1), qPrintable(a.lastError())); QVERIFY(a.setCantoneseTone(3)); QVERIFY(a.setTheme("dark")); QVERIFY(a.setFontScale(1.25)); QVERIFY(a.setAccentColor("#224466")); QVERIFY(a.setNarrationEnabled(true)); QVERIFY(a.setNarrationLanguage("yue")); QVERIFY(a.setEnglishVoiceId("voice-en")); QVERIFY(a.setCantoneseVoiceId("voice-yue")); QVERIFY(a.setNarrationRate(1.5)); PreferencesStore b(path(dir)); QCOMPARE(b.languageMode(),QString("both")); QCOMPARE(b.englishTone(),1); QCOMPARE(b.cantoneseTone(),3); QCOMPARE(b.theme(),QString("dark")); QCOMPARE(b.fontScale(),1.25); QCOMPARE(b.accentColor(),QString("#224466")); QVERIFY(b.narrationEnabled()); QCOMPARE(b.cantoneseVoiceId(),QString("voice-yue")); QCOMPARE(b.narrationRate(),1.5); }
void PreferencesStoreTests::rejectsInvalidWithoutChange() { QTemporaryDir dir; PreferencesStore store(path(dir)); const auto before=store.theme(); QSignalSpy errors(&store,&PreferencesStore::errorOccurred); QVERIFY(!store.setTheme("neon")); QCOMPARE(store.theme(),before); QVERIFY(!store.setFontScale(9.0)); QCOMPARE(store.fontScale(),1.0); QVERIFY(!store.setAccentColor("transparent")); QCOMPARE(store.accentColor(),QString("#6750A4")); QCOMPARE(errors.count(),3); }
void PreferencesStoreTests::corruptionRetainsDefaults() { QTemporaryDir dir; QFile file(path(dir)); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{broken"); file.close(); PreferencesStore store(path(dir)); QCOMPARE(store.languageMode(),QString("en")); QCOMPARE(store.englishTone(),5); QVERIFY(store.lastError().contains("corrupt")); }
void PreferencesStoreTests::corruptStateRequiresReset() { QTemporaryDir dir; QFile file(path(dir)); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{broken"); file.close(); PreferencesStore store(path(dir)); QVERIFY(!store.setTheme("dark")); QVERIFY(store.lastError().contains("explicit reset")); QVERIFY(store.reset()); QVERIFY(store.setTheme("dark")); }
void PreferencesStoreTests::resetPreservesCorruptEvidence() { QTemporaryDir dir; QFile file(path(dir)); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{broken"); file.close(); PreferencesStore store(path(dir)); QVERIFY(store.reset()); QVERIFY(QFile::exists(path(dir)+".corrupt")); QVERIFY(QFile::exists(path(dir))); }
void PreferencesStoreTests::writerLockPreservesMemory() { QTemporaryDir dir; PreferencesStore store(path(dir)); QLockFile lock(path(dir)+".lock"); lock.setStaleLockTime(0); QVERIFY(lock.tryLock()); QVERIFY(!store.setTheme("dark")); QCOMPARE(store.theme(),QString("system")); QVERIFY(store.lastError().contains("busy")); }
void PreferencesStoreTests::exportOmitsVoiceIds() { QTemporaryDir dir; PreferencesStore store(path(dir)); QVERIFY(store.setEnglishVoiceId("private-en")); QVERIFY(store.setCantoneseVoiceId("private-yue")); const auto object=QJsonDocument::fromJson(store.exportPublicPreferences()).object(); QVERIFY(!object.contains("englishVoiceId")); QVERIFY(!object.contains("cantoneseVoiceId")); }
void PreferencesStoreTests::staleWriterRejected() { QTemporaryDir dir; PreferencesStore first(path(dir)); PreferencesStore second(path(dir)); QVERIFY(first.setTheme("dark")); QVERIFY(!second.setFontScale(1.5)); QCOMPARE(second.fontScale(), 1.0); QVERIFY(second.lastError().contains("changed by another writer")); }
void PreferencesStoreTests::vocabularyLoadsAndClears() { QTemporaryDir dir; const QString input=dir.filePath("input.json"); QFile file(input); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{\"schemaVersion\":1,\"entries\":{\"Widget\":\"Fixture\"}}"); file.close(); precision::preferences::PersonalVocabularyStore store(dir.filePath("cache.json")); QVERIFY(store.loadFile(input)); QVERIFY(store.loaded()); QCOMPARE(store.applyPrivateUiText("Widget id"),QString("Fixture id")); QVERIFY(store.clear()); QVERIFY(!store.loaded()); QVERIFY(!QFile::exists(dir.filePath("cache.json"))); }
void PreferencesStoreTests::vocabularyRejectsDuplicateDecodedKey() { QTemporaryDir dir; const QString input=dir.filePath("duplicate.json"); QFile file(input); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{\"schemaVersion\":1,\"entries\":{\"Widget\":\"Fixture\",\"W\\u0069dget\":\"Other\"}}"); file.close(); precision::preferences::PersonalVocabularyStore store(dir.filePath("cache.json")); QVERIFY(!store.loadFile(input)); QVERIFY(!store.loaded()); }
void PreferencesStoreTests::vocabularyAllowsSameKeyAcrossObjects() { QTemporaryDir dir; const QString input=dir.filePath("legal.json"); QFile file(input); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{\"schemaVersion\":1,\"entries\":{\"schemaVersion\":\"quoted: \\\"value\\\"\"}}"); file.close(); precision::preferences::PersonalVocabularyStore store(dir.filePath("cache.json")); QVERIFY(store.loadFile(input)); }
void PreferencesStoreTests::vocabularyUsesOriginalTextAndLongestKey() { QTemporaryDir dir; const QString input=dir.filePath("ordered.json"); QFile file(input); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{\"schemaVersion\":1,\"entries\":{\"A\":\"B\",\"B\":\"C\",\"AB\":\"Z\"}}"); file.close(); precision::preferences::PersonalVocabularyStore store(dir.filePath("cache.json")); QVERIFY(store.loadFile(input)); QCOMPARE(store.applyPrivateUiText("A AB B"),QString("B Z C")); }
void PreferencesStoreTests::boundedAndFractionalInputs() { QTemporaryDir dir; QFile pref(path(dir)); QVERIFY(pref.open(QIODevice::WriteOnly)); pref.write(QByteArray(65537, 'x')); pref.close(); PreferencesStore oversized(path(dir)); QVERIFY(oversized.lastError().contains("large")); const auto vfile=dir.filePath("v.json"); QFile vocab(vfile); QVERIFY(vocab.open(QIODevice::WriteOnly)); vocab.write(QByteArray(1024*1024+1,'x')); vocab.close(); precision::preferences::PersonalVocabularyStore store(dir.filePath("cache.json")); QVERIFY(!store.loadFile(vfile)); QFile fraction(path(dir)); QVERIFY(fraction.open(QIODevice::WriteOnly)); fraction.write("{\"schemaVersion\":1.5,\"languageMode\":\"en\",\"englishTone\":5,\"cantoneseTone\":5,\"dialogEmojis\":true,\"theme\":\"system\",\"fontScale\":1,\"accentColor\":\"#6750A4\",\"reducedMotion\":false,\"adhdMode\":false,\"narrationEnabled\":false,\"narrationLanguage\":\"en\",\"englishVoiceId\":\"\",\"cantoneseVoiceId\":\"\",\"narrationRate\":1,\"narrationPitch\":1}"); fraction.close(); PreferencesStore fractional(path(dir)); QVERIFY(fractional.lastError().contains("unsupported") || fractional.lastError().contains("invalid")); }
namespace {
bool writeBytes(const QString &name, const QByteArray &bytes) {
 QFile file(name); return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.flush();
}
QByteArray readBytes(const QString &name) { QFile file(name); if(!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
QByteArray validPreferences(const QString &name) { PreferencesStore store(name); if(!store.reset()) return {}; return readBytes(name); }
const QByteArray validVocabulary=R"({"schemaVersion":1,"entries":{"Widget":"Fixture","schemaVersion":"quoted \"Widget\": value","literal\\u0061":"literal match"}})";
}
void PreferencesStoreTests::fractionalTone_data() {
 QTest::addColumn<QString>("field"); QTest::addColumn<double>("number");
 for(const auto *field:{"englishTone","cantoneseTone","schemaVersion"})
  for(double n:{1.5,4.5}) QTest::newRow(qPrintable(QString(field)+QString::number(n)))<<QString(field)<<n;
}
void PreferencesStoreTests::fractionalTone() {
 QFETCH(QString,field); QFETCH(double,number); QTemporaryDir dir;
 auto object=QJsonDocument::fromJson(validPreferences(path(dir))).object(); object[field]=number;
 const auto raw=QJsonDocument(object).toJson(); QVERIFY(writeBytes(path(dir),raw));
 PreferencesStore store(path(dir)); QVERIFY(!store.lastError().isEmpty()); QVERIFY(!store.setTheme("dark")); QCOMPARE(readBytes(path(dir)),raw);
}
void PreferencesStoreTests::settingsDuplicates_data() {
 QTest::addColumn<QByteArray>("extra");
 QTest::newRow("literal")<<QByteArray(R"("theme":"light",)");
 QTest::newRow("decoded")<<QByteArray(R"("th\u0065me":"light",)");
 QTest::newRow("schemaDecoded")<<QByteArray(R"("schema\u0056ersion":1,)");
}
void PreferencesStoreTests::settingsDuplicates() {
 QFETCH(QByteArray,extra); QTemporaryDir dir; auto raw=validPreferences(path(dir)); raw.insert(1,extra);
 QVERIFY(writeBytes(path(dir),raw)); PreferencesStore store(path(dir)); QVERIFY(!store.lastError().isEmpty());
 QVERIFY(!store.setTheme("dark")); QCOMPARE(readBytes(path(dir)),raw);
}
void PreferencesStoreTests::deepUnknownFields() {
 QTemporaryDir dir; auto raw=validPreferences(path(dir));
 raw.insert(1,QByteArray("\"unknown\":")+QByteArray(20000,'[')+"0"+QByteArray(20000,']')+",");
 QVERIFY(writeBytes(path(dir),raw)); PreferencesStore store(path(dir)); QVERIFY(store.lastError().contains("shape"));
 const auto input=dir.filePath("input.json"); const auto cache=dir.filePath("cache.json");
 QVERIFY(writeBytes(input,validVocabulary)); precision::preferences::PersonalVocabularyStore vocabulary(cache); QVERIFY(vocabulary.loadFile(input));
 QVERIFY(writeBytes(input,QByteArray("{\"unknown\":")+QByteArray(20000,'[')+"0"+QByteArray(20000,']')+"}"));
 QVERIFY(!vocabulary.loadFile(input)); QCOMPARE(readBytes(cache),validVocabulary);
}
void PreferencesStoreTests::scannerScopeAndDepth() {
 using precision::preferences::JsonShapeScanner;
 QVERIFY(!JsonShapeScanner(validVocabulary).invalid());
 QVERIFY(!JsonShapeScanner(QByteArray(R"({"a":{"a":{"a":{"a":1}}}})")).invalid());
 QVERIFY(JsonShapeScanner(QByteArray(R"({"a":{"a":{"a":{"a":{"a":1}}}}})")).invalid());
 QVERIFY(JsonShapeScanner(QByteArray(R"({"a":1,"\u0061":2})")).invalid());
 QVERIFY(!JsonShapeScanner(QByteArray(R"({"a":1,"\\u0061":2})")).invalid());
 QVERIFY(JsonShapeScanner(QByteArray(R"({"\\u0061":1,"\u005cu0061":2})")).invalid());
 const QByteArray quotedValue=R"({"a":"\"a\":1"})";
 QVERIFY(!JsonShapeScanner(quotedValue).invalid());
}
void PreferencesStoreTests::oversizedVocabularyStartup() {
 QTemporaryDir dir; const auto cache=dir.filePath("cache.json"); QByteArray bytes=validVocabulary; bytes.append(1024*1024+1-bytes.size(),' ');
 QVERIFY(writeBytes(cache,bytes)); precision::preferences::PersonalVocabularyStore store(cache);
 QVERIFY(!store.loaded()); QCOMPARE(store.applyPrivateUiText("Widget"),QString("Widget")); QCOMPARE(readBytes(cache),bytes);
 bytes.chop(1); QVERIFY(writeBytes(cache,bytes)); precision::preferences::PersonalVocabularyStore atLimit(cache); QVERIFY(atLimit.loaded());
}
void PreferencesStoreTests::vocabularyReloadPreservesBytes_data() {
 QTest::addColumn<QByteArray>("bad");
 QTest::newRow("unknownRoot")<<QByteArray(R"({"schemaVersion":1,"entries":{},"extra":true})");
 QTest::newRow("unsafeKey")<<QByteArray(R"({"schemaVersion":1,"entries":{"__proto__":"x"}})");
 QTest::newRow("longKey")<<(QByteArray("{\"schemaVersion\":1,\"entries\":{\"")+QByteArray(257,'a')+"\":\"x\"}}");
 QTest::newRow("longValue")<<(QByteArray("{\"schemaVersion\":1,\"entries\":{\"a\":\"")+QByteArray(513,'x')+"\"}}");
 QJsonObject many; for(int i=0;i<10001;++i) many.insert(QString::number(i),"x");
 QTest::newRow("manyEntries")<<QJsonDocument(QJsonObject{{"schemaVersion",1},{"entries",many}}).toJson();
 QTest::newRow("syntax")<<QByteArray("{broken");
 QTest::newRow("decodedDuplicate")<<QByteArray(R"({"schemaVersion":1,"entries":{"Widget":"x","W\u0069dget":"y"}})");
 QTest::newRow("literalBackslashDuplicate")<<QByteArray(R"({"schemaVersion":1,"entries":{"\\u0061":"x","\u005cu0061":"y"}})");
 QTest::newRow("fractionalSchema")<<QByteArray(R"({"schemaVersion":1.5,"entries":{"Widget":"x"}})");
 QTest::newRow("nonString")<<QByteArray(R"({"schemaVersion":1,"entries":{"Widget":1}})");
 QTest::newRow("oversized")<<QByteArray(1024*1024+1,'x');
}
void PreferencesStoreTests::vocabularyReloadPreservesBytes() {
 QFETCH(QByteArray,bad); QTemporaryDir dir; const auto input=dir.filePath("input.json"); const auto cache=dir.filePath("cache.json");
 QVERIFY(writeBytes(input,validVocabulary)); precision::preferences::PersonalVocabularyStore store(cache); QVERIFY(store.loadFile(input));
 QCOMPARE(store.applyPrivateUiText("schemaVersion"),QString("quoted \"Widget\": value"));
 QCOMPARE(store.applyPrivateUiText("literal\\u0061"),QString("literal match"));
 QSignalSpy changed(&store,&precision::preferences::PersonalVocabularyStore::loadedChanged);
 QVERIFY(writeBytes(input,bad)); QVERIFY(!store.loadFile(input)); QVERIFY(store.loaded());
 QCOMPARE(store.applyPrivateUiText("Widget"),QString("Fixture")); QCOMPARE(readBytes(cache),validVocabulary); QCOMPARE(changed.count(),0);
}
void PreferencesStoreTests::vocabularyLockedClear() {
 QTemporaryDir dir; const auto input=dir.filePath("input.json"); const auto cache=dir.filePath("cache.json");
 QVERIFY(writeBytes(input,validVocabulary)); precision::preferences::PersonalVocabularyStore store(cache); QVERIFY(store.loadFile(input));
#ifdef Q_OS_WIN
 HANDLE handle=CreateFileW(reinterpret_cast<LPCWSTR>(cache.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 QVERIFY(handle!=INVALID_HANDLE_VALUE);
 const bool cleared=store.clear(); CloseHandle(handle);
 QVERIFY(!cleared); QVERIFY(store.loaded()); QCOMPARE(readBytes(cache),validVocabulary); QCOMPARE(store.applyPrivateUiText("Widget"),QString("Fixture"));
 QVERIFY(store.clear()); QVERIFY(!store.loaded()); QVERIFY(!QFile::exists(cache));
#else
 QSKIP("This case exercises Windows file sharing denial.");
#endif
}
void PreferencesStoreTests::publicExportAllowlist() {
 QTemporaryDir dir; PreferencesStore store(path(dir)); QVERIFY(store.setEnglishVoiceId("private-source-path"));
 const auto output=store.exportPublicPreferences(); const auto object=QJsonDocument::fromJson(output).object();
 const QSet<QString> expected={"schemaVersion","languageMode","englishTone","cantoneseTone","dialogEmojis","theme","fontScale","accentColor","reducedMotion","adhdMode","narrationEnabled","narrationLanguage","narrationRate","narrationPitch"};
 const auto keys=object.keys(); QCOMPARE(QSet<QString>(keys.begin(),keys.end()),expected);
 QVERIFY(!output.contains(path(dir).toUtf8())); QVERIFY(!output.contains("private-source-path")); QVERIFY(!output.contains("cache")); QVERIFY(!output.contains("entries"));
}
void PreferencesStoreTests::exactBytesPersisted() {
 QTemporaryDir dir; const auto input=dir.filePath("input.json"); const auto cache=dir.filePath("cache.json");
 const auto bytes=QByteArray(" \n")+validVocabulary+"\r\n "; QVERIFY(writeBytes(input,bytes));
 precision::preferences::PersonalVocabularyStore vocabulary(cache); QVERIFY(vocabulary.loadFile(input)); QCOMPARE(readBytes(cache),bytes);
 PreferencesStore store(path(dir)); QVERIFY(store.setEnglishVoiceId(QString::fromUtf8("聲音")));
 const auto raw=readBytes(path(dir)); QCOMPARE(QJsonDocument::fromJson(raw).toJson(QJsonDocument::Compact),raw);
 PreferencesStore restored(path(dir)); QCOMPARE(restored.englishVoiceId(),QString::fromUtf8("聲音"));
}
void PreferencesStoreTests::preferencesReadLimits() {
 QTemporaryDir dir; auto raw=validPreferences(path(dir)); raw.append(65536-raw.size(),' '); QVERIFY(writeBytes(path(dir),raw));
 PreferencesStore atLimit(path(dir)); QVERIFY(atLimit.lastError().isEmpty());
 raw.append(' '); QVERIFY(writeBytes(path(dir),raw)); QVERIFY(!atLimit.setTheme("dark")); QCOMPARE(readBytes(path(dir)),raw);
 PreferencesStore overLimit(path(dir)); QVERIFY(!overLimit.lastError().isEmpty()); QVERIFY(!overLimit.reset()); QCOMPARE(readBytes(path(dir)),raw); QVERIFY(!QFile::exists(path(dir)+".corrupt"));
 QVERIFY(writeBytes(path(dir),"{broken")); PreferencesStore corrupt(path(dir)); QVERIFY(writeBytes(path(dir),raw)); QVERIFY(!corrupt.reset()); QCOMPARE(readBytes(path(dir)),raw); QVERIFY(!QFile::exists(path(dir)+".corrupt"));
}
void PreferencesStoreTests::resetStaleAndEvidenceSafety() {
 QTemporaryDir dir; QVERIFY(writeBytes(path(dir),"{first")); PreferencesStore stale(path(dir));
 QVERIFY(writeBytes(path(dir),"{second")); QVERIFY(!stale.reset()); QCOMPARE(readBytes(path(dir)),QByteArray("{second")); QVERIFY(!QFile::exists(path(dir)+".corrupt"));
 PreferencesStore current(path(dir)); QLockFile lock(path(dir)+".lock"); QVERIFY(lock.tryLock()); QVERIFY(!current.reset()); QVERIFY(!QFile::exists(path(dir)+".corrupt")); lock.unlock();
 QVERIFY(writeBytes(path(dir)+".corrupt","earlier evidence")); QVERIFY(!current.reset()); QCOMPARE(readBytes(path(dir)+".corrupt"),QByteArray("earlier evidence"));
 QVERIFY(QFile::remove(path(dir)+".corrupt")); QVERIFY(current.reset()); QCOMPARE(readBytes(path(dir)+".corrupt"),QByteArray("{second"));
 QVERIFY(current.setTheme("dark")); PreferencesStore restored(path(dir)); QCOMPARE(restored.theme(),QString("dark"));
}
void PreferencesStoreTests::vocabularyWriteRefusal() {
 QTemporaryDir dir; const auto input=dir.filePath("input.json"); const auto cache=dir.filePath("cache.json");
 QVERIFY(writeBytes(input,validVocabulary)); precision::preferences::PersonalVocabularyStore store(cache); QVERIFY(store.loadFile(input));
 QVERIFY(writeBytes(input,R"({"schemaVersion":1,"entries":{"Widget":"Replacement"}})"));
#ifdef Q_OS_WIN
 HANDLE handle=CreateFileW(reinterpret_cast<LPCWSTR>(cache.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 QVERIFY(handle!=INVALID_HANDLE_VALUE); const bool replaced=store.loadFile(input); CloseHandle(handle);
 QVERIFY(!replaced); QCOMPARE(readBytes(cache),validVocabulary); QCOMPARE(store.applyPrivateUiText("Widget"),QString("Fixture"));
#else
 QSKIP("This case exercises Windows file sharing denial.");
#endif
}
void PreferencesStoreTests::vocabularyStartupRejectsDepth() {
 QTemporaryDir dir; const auto cache=dir.filePath("cache.json");
 const auto bytes=QByteArray("{\"unknown\":")+QByteArray(20000,'[')+"0"+QByteArray(20000,']')+"}";
 QVERIFY(writeBytes(cache,bytes)); precision::preferences::PersonalVocabularyStore store(cache);
 QVERIFY(!store.loaded()); QCOMPARE(readBytes(cache),bytes);
}
void PreferencesStoreTests::emptyVocabularyTransitions() {
 QTemporaryDir dir; const auto input=dir.filePath("input.json"); const auto cache=dir.filePath("cache.json");
 const QByteArray empty=R"({"schemaVersion":1,"entries":{}})";
 QVERIFY(writeBytes(input,empty)); precision::preferences::PersonalVocabularyStore store(cache);
 QSignalSpy changes(&store,&precision::preferences::PersonalVocabularyStore::loadedChanged);
 QVERIFY(!store.loaded()); QVERIFY(store.loadFile(input)); QVERIFY(store.loaded()); QCOMPARE(changes.count(),1);
 QCOMPARE(store.applyPrivateUiText("Widget"),QString("Widget")); QCOMPARE(readBytes(cache),empty);
 precision::preferences::PersonalVocabularyStore restored(cache); QVERIFY(restored.loaded());
 QVERIFY(store.loadFile(input)); QCOMPARE(changes.count(),1);
 QVERIFY(writeBytes(input,validVocabulary)); QVERIFY(store.loadFile(input)); QVERIFY(store.loaded()); QCOMPARE(changes.count(),2);
 QCOMPARE(store.applyPrivateUiText("Widget"),QString("Fixture"));
 QVERIFY(writeBytes(input,empty)); QVERIFY(store.loadFile(input)); QVERIFY(store.loaded()); QCOMPARE(changes.count(),3);
 QCOMPARE(store.applyPrivateUiText("Widget"),QString("Widget"));
 QVERIFY(store.clear()); QVERIFY(!store.loaded()); QCOMPARE(changes.count(),4); QVERIFY(!QFile::exists(cache));
 QVERIFY(store.clear()); QCOMPARE(changes.count(),4);
 precision::preferences::PersonalVocabularyStore afterClear(cache); QVERIFY(!afterClear.loaded());
}
void PreferencesStoreTests::emptyVocabularyLockedClear() {
 QTemporaryDir dir; const auto input=dir.filePath("input.json"); const auto cache=dir.filePath("cache.json");
 const QByteArray empty=R"({"schemaVersion":1,"entries":{}})";
 QVERIFY(writeBytes(input,empty)); precision::preferences::PersonalVocabularyStore store(cache); QVERIFY(store.loadFile(input));
 QSignalSpy changes(&store,&precision::preferences::PersonalVocabularyStore::loadedChanged);
#ifdef Q_OS_WIN
 HANDLE handle=CreateFileW(reinterpret_cast<LPCWSTR>(cache.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 QVERIFY(handle!=INVALID_HANDLE_VALUE); const bool cleared=store.clear(); CloseHandle(handle);
 QVERIFY(!cleared); QVERIFY(store.loaded()); QCOMPARE(changes.count(),0); QCOMPARE(readBytes(cache),empty);
 QVERIFY(store.clear()); QVERIFY(!store.loaded()); QCOMPARE(changes.count(),1);
#else
 QSKIP("This case exercises Windows file sharing denial.");
#endif
}
QTEST_APPLESS_MAIN(PreferencesStoreTests)
#include "preferences_store_tests.moc"
