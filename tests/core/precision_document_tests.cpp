#include "precision_document.h"
#include "model_evaluator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

using namespace precision::core;
#define REQUIRE(condition) do { if (!(condition)) { std::fprintf(stderr, "failed: %s at %d\n", #condition, __LINE__); return 1; } } while (false)

static DocumentRecord initial() { return {kDocumentSchemaVersion, QStringLiteral("doc-0001"), 7, QStringLiteral("mm"), {}}; }
static Feature feature(QString id, QVector<QString> refs = {}) { return {std::move(id), QStringLiteral("Sketch"), QStringLiteral("Base profile"), std::move(refs), QJsonObject{{QStringLiteral("radius"), 12.5}}, false}; }
static Feature box(QString id, bool suppressed = false) { return {std::move(id), QStringLiteral("box"), QStringLiteral("Box"), {}, QJsonObject{{QStringLiteral("dx"), 1.0}, {QStringLiteral("dy"), 2.0}, {QStringLiteral("dz"), 3.0}}, suppressed}; }
static Feature boolean(QString id, QVector<QString> refs) { return {std::move(id), QStringLiteral("union"), QStringLiteral("Union"), std::move(refs), {}, false}; }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    Document d(initial()); REQUIRE(Document::validate(d.record()).ok); REQUIRE(d.addFeature(feature("a"), 7).ok); const auto afterAdd = d.record().revision;
    REQUIRE(!d.addFeature(feature("a"), afterAdd).ok); REQUIRE(d.record().features.size() == 1); REQUIRE(!d.addFeature(feature("b", {"missing"}), afterAdd).ok); REQUIRE(d.record().features.size() == 1);
    REQUIRE(d.addFeature(feature("b", {"a"}), afterAdd).ok); const auto afterB = d.record().revision; REQUIRE(!d.updateFeature(feature("a", {"b"}), afterB).ok); REQUIRE(d.record().revision == afterB);
    REQUIRE(!d.suppressFeature("a", true, afterB - 1).ok); REQUIRE(d.undo(afterB).ok); REQUIRE(d.record().revision == afterB + 1 && d.record().features.size() == 1); REQUIRE(d.redo(d.record().revision).ok && d.record().features.size() == 2);
    const auto one = Document::serialize(d.record()); DocumentRecord roundTrip; const auto parsedOne = Document::parse(one, &roundTrip); if (!parsedOne.ok) std::fprintf(stderr, "parse error: %s\n", qPrintable(parsedOne.error)); REQUIRE(parsedOne.ok); REQUIRE(one == Document::serialize(roundTrip)); REQUIRE(!Document::parse("{\"schemaVersion\":2}", &roundTrip).ok); REQUIRE(!Document::parse("{\"schemaVersion\":1.5,\"documentId\":\"d\",\"revision\":0,\"units\":\"mm\",\"features\":[]}", &roundTrip).ok); REQUIRE(!Document::parse("{\"schemaVersion\":1,\"documentId\":\"d\",\"revision\":0,\"revision\":1,\"units\":\"mm\",\"features\":[]}", &roundTrip).ok); const QByteArray escapedDuplicate = R"({"schemaVersion":1,"documentId":"d","revision":0,"\u0072evision":1,"units":"mm","features":[]})"; REQUIRE(escapedDuplicate.contains("\\u0072")); REQUIRE(!Document::parse(escapedDuplicate, &roundTrip).ok); REQUIRE(!Document::parse("{\"schemaVersion\":1,\"documentId\":\"d\",\"revision\":9007199254740992,\"units\":\"mm\",\"features\":[]}", &roundTrip).ok); QByteArray deep("{\"a\":"); for (int i = 0; i < 64; ++i) deep += "{\"a\":"; deep += "0"; for (int i = 0; i < 65; ++i) deep += '}'; REQUIRE(!Document::parse(deep, &roundTrip).ok);
    QJsonObject deepest{{QStringLiteral("v"), 0}}; for (int i = 0; i < 15; ++i) deepest = QJsonObject{{QStringLiteral("n"), deepest}}; auto deepestFeature = feature("deep"); deepestFeature.parameters = deepest; DocumentRecord deepestRecord{kDocumentSchemaVersion, QStringLiteral("deep-doc"), 0, QStringLiteral("mm"), {deepestFeature}}; REQUIRE(Document::validate(deepestRecord).ok); const auto deepestBytes = Document::serialize(deepestRecord); REQUIRE(Document::parse(deepestBytes, &roundTrip).ok); REQUIRE(deepestBytes == Document::serialize(roundTrip)); deepest = QJsonObject{{QStringLiteral("n"), deepest}}; deepestFeature.parameters = deepest; deepestRecord.features = {deepestFeature}; REQUIRE(!Document::validate(deepestRecord).ok);
    bool invalidInitial = false; try { Document invalid({kDocumentSchemaVersion, {}, 0, QStringLiteral("mm"), {}}); } catch (const std::invalid_argument &) { invalidInitial = true; } REQUIRE(invalidInitial); Document maxRevision({kDocumentSchemaVersion, QStringLiteral("max"), kMaxDocumentRevision, QStringLiteral("mm"), {}}); REQUIRE(!maxRevision.addFeature(feature("x"), kMaxDocumentRevision).ok);
    QTemporaryDir temp; REQUIRE(temp.isValid()); const QString path = temp.filePath("model.pcad"); auto saveOne = DocumentStorage::save(path, d.record(), std::nullopt); REQUIRE(saveOne.ok); const auto stable = d.record(); auto changed = stable; changed.revision += 10; auto saveTwo = DocumentStorage::save(path, changed, stable.revision); if (!saveTwo.ok) std::fprintf(stderr, "save error: %s\n", qPrintable(saveTwo.error)); REQUIRE(saveTwo.ok); REQUIRE(DocumentStorage::save(path, changed, changed.revision).ok); auto changedSameRevision = changed; changedSameRevision.units = QStringLiteral("in"); REQUIRE(!DocumentStorage::save(path, changedSameRevision, changed.revision).ok); REQUIRE(!DocumentStorage::save(path, stable, changed.revision).ok); auto differentDocument = changed; differentDocument.documentId = QStringLiteral("other"); differentDocument.revision++; REQUIRE(!DocumentStorage::save(path, differentDocument, changed.revision).ok); REQUIRE(!DocumentStorage::save(path, changed, stable.revision).ok);
    QFile corrupt(path); REQUIRE(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate)); REQUIRE(corrupt.write("not json") == 8); corrupt.close(); bool backup = false; DocumentRecord restored; REQUIRE(DocumentStorage::recover(path, &restored, &backup).ok); REQUIRE(backup && restored.revision == stable.revision);
    QLockFile lock(path + QStringLiteral(".lock")); lock.setStaleLockTime(0); REQUIRE(lock.tryLock()); REQUIRE(!DocumentStorage::save(path, restored, restored.revision).ok); lock.unlock();
    const QString largePath = temp.filePath("large.pcad"); QFile large(largePath); REQUIRE(large.open(QIODevice::WriteOnly)); REQUIRE(large.resize(16 * 1024 * 1024 + 1)); large.close(); REQUIRE(!DocumentStorage::load(largePath, &roundTrip).ok);
    QFile unknown(path); REQUIRE(unknown.open(QIODevice::WriteOnly | QIODevice::Truncate)); REQUIRE(unknown.write("{\"schemaVersion\":2}") > 0); unknown.close(); REQUIRE(!DocumentStorage::save(path, restored, restored.revision).ok);

    DocumentRecord ordered{kDocumentSchemaVersion, QStringLiteral("evaluate-order"), 3, QStringLiteral("mm"), {boolean(QStringLiteral("c"), {QStringLiteral("a"), QStringLiteral("b")}), box(QStringLiteral("a")), box(QStringLiteral("b"))}};
    const auto evaluation = ModelEvaluator::evaluate(ordered); REQUIRE(evaluation.result.ok); REQUIRE(evaluation.ordered.size() == 3); REQUIRE(evaluation.ordered[0].feature.id == QStringLiteral("a")); REQUIRE(evaluation.ordered[1].feature.id == QStringLiteral("b")); REQUIRE(evaluation.ordered[2].feature.id == QStringLiteral("c"));
    auto suppressed = ordered; suppressed.features[1].suppressed = true; const auto suppression = ModelEvaluator::evaluate(suppressed); REQUIRE(suppression.result.ok); REQUIRE(suppression.isSuppressed(QStringLiteral("a"))); REQUIRE(!suppression.isSuppressed(QStringLiteral("b"))); REQUIRE(suppression.isSuppressed(QStringLiteral("c"))); REQUIRE(suppression.ordered[2].suppressionSource == QStringLiteral("a"));
    auto unknownReference = ordered; unknownReference.features[0].inputRefs[1] = QStringLiteral("unknown"); const auto unknownEvaluation = ModelEvaluator::evaluate(unknownReference); REQUIRE(!unknownEvaluation.result.ok); REQUIRE(unknownEvaluation.ordered.isEmpty());
    auto cycle = ordered; cycle.features[1].inputRefs = {QStringLiteral("c")}; const auto cycleEvaluation = ModelEvaluator::evaluate(cycle); REQUIRE(!cycleEvaluation.result.ok); REQUIRE(cycleEvaluation.ordered.isEmpty());
    auto invalidSchema = ordered; invalidSchema.features[1].parameters.remove(QStringLiteral("dy")); const auto invalidEvaluation = ModelEvaluator::evaluate(invalidSchema); REQUIRE(!invalidEvaluation.result.ok); REQUIRE(invalidEvaluation.ordered.isEmpty());
    Document rollback(DocumentRecord{kDocumentSchemaVersion, QStringLiteral("rollback"), 0, QStringLiteral("mm"), {box(QStringLiteral("valid"))}}); auto invalidFeature = box(QStringLiteral("invalid")); invalidFeature.parameters.insert(QStringLiteral("dx"), std::numeric_limits<double>::infinity()); REQUIRE(!rollback.addFeature(invalidFeature, 0).ok); REQUIRE(rollback.record().revision == 0 && rollback.record().features.size() == 1 && rollback.record().features[0].id == QStringLiteral("valid"));
    return 0;
}
