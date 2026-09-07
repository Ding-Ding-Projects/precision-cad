#include "precision_document.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using namespace precision::core;
#define REQUIRE(condition) do { if (!(condition)) { std::fprintf(stderr, "failed: %s at %d\n", #condition, __LINE__); return 1; } } while (false)

static DocumentRecord initial() { return {kDocumentSchemaVersion, QStringLiteral("doc-0001"), 7, QStringLiteral("mm"), {}}; }
static Feature feature(QString id, QVector<QString> refs = {}) { return {std::move(id), QStringLiteral("Sketch"), QStringLiteral("Base profile"), std::move(refs), QJsonObject{{QStringLiteral("radius"), 12.5}}, false}; }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    Document d(initial()); REQUIRE(Document::validate(d.record()).ok); REQUIRE(d.addFeature(feature("a"), 7).ok); const auto afterAdd = d.record().revision;
    REQUIRE(!d.addFeature(feature("a"), afterAdd).ok); REQUIRE(d.record().features.size() == 1); REQUIRE(!d.addFeature(feature("b", {"missing"}), afterAdd).ok); REQUIRE(d.record().features.size() == 1);
    REQUIRE(d.addFeature(feature("b", {"a"}), afterAdd).ok); const auto afterB = d.record().revision; REQUIRE(!d.updateFeature(feature("a", {"b"}), afterB).ok); REQUIRE(d.record().revision == afterB);
    REQUIRE(!d.suppressFeature("a", true, afterB - 1).ok); REQUIRE(d.undo(afterB).ok); REQUIRE(d.record().revision == afterB + 1 && d.record().features.size() == 1); REQUIRE(d.redo(d.record().revision).ok && d.record().features.size() == 2);
    const auto one = Document::serialize(d.record()); DocumentRecord roundTrip; const auto parsedOne = Document::parse(one, &roundTrip); if (!parsedOne.ok) std::fprintf(stderr, "parse error: %s\n", qPrintable(parsedOne.error)); REQUIRE(parsedOne.ok); REQUIRE(one == Document::serialize(roundTrip)); REQUIRE(!Document::parse("{\"schemaVersion\":2}", &roundTrip).ok); REQUIRE(!Document::parse("{\"schemaVersion\":1.5,\"documentId\":\"d\",\"revision\":0,\"units\":\"mm\",\"features\":[]}", &roundTrip).ok); REQUIRE(!Document::parse("{\"schemaVersion\":1,\"documentId\":\"d\",\"revision\":0,\"revision\":1,\"units\":\"mm\",\"features\":[]}", &roundTrip).ok); REQUIRE(!Document::parse("{\"schemaVersion\":1,\"documentId\":\"d\",\"revision\":0,\"\u0072evision\":1,\"units\":\"mm\",\"features\":[]}", &roundTrip).ok); REQUIRE(!Document::parse("{\"schemaVersion\":1,\"documentId\":\"d\",\"revision\":9007199254740992,\"units\":\"mm\",\"features\":[]}", &roundTrip).ok); QByteArray deep("{\"a\":"); for (int i = 0; i < 64; ++i) deep += "{\"a\":"; deep += "0"; for (int i = 0; i < 65; ++i) deep += '}'; REQUIRE(!Document::parse(deep, &roundTrip).ok);
    bool invalidInitial = false; try { Document invalid({kDocumentSchemaVersion, {}, 0, QStringLiteral("mm"), {}}); } catch (const std::invalid_argument &) { invalidInitial = true; } REQUIRE(invalidInitial); Document maxRevision({kDocumentSchemaVersion, QStringLiteral("max"), kMaxDocumentRevision, QStringLiteral("mm"), {}}); REQUIRE(!maxRevision.addFeature(feature("x"), kMaxDocumentRevision).ok);
    QTemporaryDir temp; REQUIRE(temp.isValid()); const QString path = temp.filePath("model.pcad"); auto saveOne = DocumentStorage::save(path, d.record(), std::nullopt); REQUIRE(saveOne.ok); const auto stable = d.record(); auto changed = stable; changed.revision += 10; auto saveTwo = DocumentStorage::save(path, changed, stable.revision); if (!saveTwo.ok) std::fprintf(stderr, "save error: %s\n", qPrintable(saveTwo.error)); REQUIRE(saveTwo.ok); REQUIRE(DocumentStorage::save(path, changed, changed.revision).ok); auto changedSameRevision = changed; changedSameRevision.units = QStringLiteral("in"); REQUIRE(!DocumentStorage::save(path, changedSameRevision, changed.revision).ok); REQUIRE(!DocumentStorage::save(path, stable, changed.revision).ok); auto differentDocument = changed; differentDocument.documentId = QStringLiteral("other"); differentDocument.revision++; REQUIRE(!DocumentStorage::save(path, differentDocument, changed.revision).ok); REQUIRE(!DocumentStorage::save(path, changed, stable.revision).ok);
    QFile corrupt(path); REQUIRE(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate)); REQUIRE(corrupt.write("not json") == 8); corrupt.close(); bool backup = false; DocumentRecord restored; REQUIRE(DocumentStorage::recover(path, &restored, &backup).ok); REQUIRE(backup && restored.revision == stable.revision);
    QLockFile lock(path + QStringLiteral(".lock")); lock.setStaleLockTime(0); REQUIRE(lock.tryLock()); REQUIRE(!DocumentStorage::save(path, restored, restored.revision).ok); lock.unlock();
    const QString largePath = temp.filePath("large.pcad"); QFile large(largePath); REQUIRE(large.open(QIODevice::WriteOnly)); REQUIRE(large.resize(16 * 1024 * 1024 + 1)); large.close(); REQUIRE(!DocumentStorage::load(largePath, &roundTrip).ok);
    QFile unknown(path); REQUIRE(unknown.open(QIODevice::WriteOnly | QIODevice::Truncate)); REQUIRE(unknown.write("{\"schemaVersion\":2}") > 0); unknown.close(); REQUIRE(!DocumentStorage::save(path, restored, restored.revision).ok);
    return 0;
}
