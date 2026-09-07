#include "preferences_store.h"
#include "personal_vocabulary_store.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QTemporaryDir>
#include <QtTest>

using precision::preferences::PreferencesStore;
class PreferencesStoreTests final : public QObject {
    Q_OBJECT
private slots:
 void persistsValidState(); void rejectsInvalidWithoutChange(); void corruptionRetainsDefaults(); void writerLockPreservesMemory(); void exportOmitsVoiceIds(); void staleWriterRejected(); void vocabularyLoadsAndClears();
};
QString path(const QTemporaryDir &dir) { return dir.filePath("preferences.json"); }
void PreferencesStoreTests::persistsValidState() { QTemporaryDir dir; PreferencesStore a(path(dir)); QVERIFY(a.setLanguageMode("both")); QVERIFY(a.setEnglishTone(1)); QVERIFY(a.setCantoneseTone(3)); QVERIFY(a.setTheme("dark")); QVERIFY(a.setFontScale(1.25)); QVERIFY(a.setAccentColor("#224466")); QVERIFY(a.setNarrationEnabled(true)); QVERIFY(a.setNarrationLanguage("yue")); QVERIFY(a.setEnglishVoiceId("voice-en")); QVERIFY(a.setCantoneseVoiceId("voice-yue")); QVERIFY(a.setNarrationRate(1.5)); PreferencesStore b(path(dir)); QCOMPARE(b.languageMode(),QString("both")); QCOMPARE(b.englishTone(),1); QCOMPARE(b.cantoneseTone(),3); QCOMPARE(b.theme(),QString("dark")); QCOMPARE(b.fontScale(),1.25); QCOMPARE(b.accentColor(),QString("#224466")); QVERIFY(b.narrationEnabled()); QCOMPARE(b.cantoneseVoiceId(),QString("voice-yue")); QCOMPARE(b.narrationRate(),1.5); }
void PreferencesStoreTests::rejectsInvalidWithoutChange() { QTemporaryDir dir; PreferencesStore store(path(dir)); const auto before=store.theme(); QSignalSpy errors(&store,&PreferencesStore::errorOccurred); QVERIFY(!store.setTheme("neon")); QCOMPARE(store.theme(),before); QVERIFY(!store.setFontScale(9.0)); QCOMPARE(store.fontScale(),1.0); QVERIFY(!store.setAccentColor("transparent")); QCOMPARE(store.accentColor(),QString("#6750A4")); QCOMPARE(errors.count(),3); }
void PreferencesStoreTests::corruptionRetainsDefaults() { QTemporaryDir dir; QFile file(path(dir)); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{broken"); file.close(); PreferencesStore store(path(dir)); QCOMPARE(store.languageMode(),QString("en")); QCOMPARE(store.englishTone(),5); QVERIFY(store.lastError().contains("corrupt")); }
void PreferencesStoreTests::writerLockPreservesMemory() { QTemporaryDir dir; PreferencesStore store(path(dir)); QLockFile lock(path(dir)+".lock"); lock.setStaleLockTime(0); QVERIFY(lock.tryLock()); QVERIFY(!store.setTheme("dark")); QCOMPARE(store.theme(),QString("system")); QVERIFY(store.lastError().contains("busy")); }
void PreferencesStoreTests::exportOmitsVoiceIds() { QTemporaryDir dir; PreferencesStore store(path(dir)); QVERIFY(store.setEnglishVoiceId("private-en")); QVERIFY(store.setCantoneseVoiceId("private-yue")); const auto object=QJsonDocument::fromJson(store.exportPublicPreferences()).object(); QVERIFY(!object.contains("englishVoiceId")); QVERIFY(!object.contains("cantoneseVoiceId")); }
void PreferencesStoreTests::staleWriterRejected() { QTemporaryDir dir; PreferencesStore first(path(dir)); PreferencesStore second(path(dir)); QVERIFY(first.setTheme("dark")); QVERIFY(!second.setFontScale(1.5)); QCOMPARE(second.fontScale(), 1.0); QVERIFY(second.lastError().contains("changed by another writer")); }
void PreferencesStoreTests::vocabularyLoadsAndClears() { QTemporaryDir dir; const QString input=dir.filePath("input.json"); QFile file(input); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{\"schemaVersion\":1,\"entries\":{\"Widget\":\"Fixture\"}}"); file.close(); precision::preferences::PersonalVocabularyStore store(dir.filePath("cache.json")); QVERIFY(store.loadFile(input)); QVERIFY(store.loaded()); QCOMPARE(store.applyPrivateUiText("Widget id"),QString("Fixture id")); QVERIFY(store.clear()); QVERIFY(!store.loaded()); QVERIFY(!QFile::exists(dir.filePath("cache.json"))); }
QTEST_APPLESS_MAIN(PreferencesStoreTests)
#include "preferences_store_tests.moc"
