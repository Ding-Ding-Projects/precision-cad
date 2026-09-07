#include "precision_document.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

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
    const auto one = Document::serialize(d.record()); DocumentRecord roundTrip; REQUIRE(Document::parse(one, &roundTrip).ok); REQUIRE(one == Document::serialize(roundTrip)); REQUIRE(!Document::parse("{\"schemaVersion\":2}", &roundTrip).ok);
    QTemporaryDir temp; REQUIRE(temp.isValid()); const QString path = temp.filePath("model.pcad"); auto saveOne = DocumentStorage::save(path, d.record()); REQUIRE(saveOne.ok); const auto stable = d.record(); auto changed = stable; changed.revision++; auto saveTwo = DocumentStorage::save(path, changed); if (!saveTwo.ok) std::fprintf(stderr, "save error: %s\n", qPrintable(saveTwo.error)); REQUIRE(saveTwo.ok);
    QFile corrupt(path); REQUIRE(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate)); REQUIRE(corrupt.write("not json") == 8); corrupt.close(); bool backup = false; DocumentRecord restored; REQUIRE(DocumentStorage::recover(path, &restored, &backup).ok); REQUIRE(backup && restored.revision == stable.revision);
    QLockFile lock(path + QStringLiteral(".lock")); lock.setStaleLockTime(0); REQUIRE(lock.tryLock()); REQUIRE(!DocumentStorage::save(path, restored).ok); lock.unlock();
    return 0;
}
