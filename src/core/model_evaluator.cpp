#include "model_evaluator.h"

#include <QJsonArray>
#include <QQueue>

#include <cmath>

namespace precision::core {
namespace {

bool finiteNumber(const QJsonValue &value)
{
    return value.isDouble() && std::isfinite(value.toDouble());
}

bool boundedPositive(const QJsonValue &value)
{
    return finiteNumber(value) && value.toDouble() > 0.0 && value.toDouble() <= 1.0e9;
}

bool vector3(const QJsonValue &value, bool nonZero = false)
{
    const auto values = value.toArray();
    if (values.size() != 3) return false;
    double squareLength = 0.0;
    for (const auto &entry : values) {
        if (!finiteNumber(entry) || std::abs(entry.toDouble()) > 1.0e9) return false;
        squareLength += entry.toDouble() * entry.toDouble();
    }
    return !nonZero || squareLength > 1.0e-18;
}

Result requirePrimitive(const Feature &feature, std::initializer_list<const char *> required, std::initializer_list<const char *> optional)
{
    if (!feature.inputRefs.isEmpty()) return Result::failure(QStringLiteral("Primitive feature %1 cannot have inputs").arg(feature.id));
    if (feature.parameters.size() < static_cast<qsizetype>(required.size()) || feature.parameters.size() > static_cast<qsizetype>(required.size() + optional.size())) return Result::failure(QStringLiteral("Invalid parameter schema for feature %1").arg(feature.id));
    for (const char *name : required) {
        const auto value = feature.parameters.value(QLatin1StringView(name));
        if (!boundedPositive(value)) return Result::failure(QStringLiteral("Feature %1 requires finite positive bounded %2").arg(feature.id, QString::fromLatin1(name)));
    }
    for (const auto &key : feature.parameters.keys()) {
        bool allowed = false;
        for (const char *name : required) allowed = allowed || key == QLatin1StringView(name);
        for (const char *name : optional) allowed = allowed || key == QLatin1StringView(name);
        if (!allowed) return Result::failure(QStringLiteral("Unknown parameter %1 for feature %2").arg(key, feature.id));
    }
    return Result::success();
}

Result requireUnary(const Feature &feature, std::initializer_list<const char *> parameters)
{
    if (feature.inputRefs.size() != 1 || feature.parameters.size() != static_cast<qsizetype>(parameters.size())) return Result::failure(QStringLiteral("Feature %1 requires one input and its typed parameters").arg(feature.id));
    for (const char *name : parameters) if (!feature.parameters.contains(QLatin1StringView(name))) return Result::failure(QStringLiteral("Feature %1 is missing %2").arg(feature.id, QString::fromLatin1(name)));
    return Result::success();
}

Result validateFeature(const Feature &feature)
{
    if (feature.type == QStringLiteral("sketch")) {
        const auto model = feature.parameters.value(QStringLiteral("model")).toObject();
        if (!feature.inputRefs.isEmpty() || feature.parameters.size()!=1 || model.size()!=6 || model.value("schemaVersion")!=QJsonValue(1) || model.value("units")!=QJsonValue("mm") || model.value("id").toString().isEmpty() || !model.value("plane").isObject() || !model.value("entities").isArray() || !model.value("constraints").isArray() || model.value("entities").toArray().isEmpty() || model.value("entities").toArray().size()>4096 || model.value("constraints").toArray().size()>4096)
            return Result::failure(QStringLiteral("Sketch feature %1 requires one bounded sketch model; detailed validation runs in the isolated worker").arg(feature.id));
        return Result::success();
    }
    if (feature.type == QStringLiteral("pad")) {
        if (const auto result=requireUnary(feature,{"regionId","length"}); !result.ok) return result;
        const auto region=feature.parameters.value("regionId");
        return region.isString() && !region.toString().isEmpty() && region.toString().size()<=1024 && boundedPositive(feature.parameters.value("length")) ? Result::success() : Result::failure(QStringLiteral("Pad requires a stable profile region and positive bounded length"));
    }
    if (feature.type == QStringLiteral("box")) {
        if (const auto result = requirePrimitive(feature, {"dx", "dy", "dz"}, {"origin"}); !result.ok) return result;
        return !feature.parameters.contains(QStringLiteral("origin")) || vector3(feature.parameters.value(QStringLiteral("origin"))) ? Result::success() : Result::failure(QStringLiteral("Box feature %1 has an invalid origin").arg(feature.id));
    }
    if (feature.type == QStringLiteral("cylinder")) {
        if (const auto result = requirePrimitive(feature, {"radius", "height"}, {"origin", "axis"}); !result.ok) return result;
        if (feature.parameters.contains(QStringLiteral("origin")) && !vector3(feature.parameters.value(QStringLiteral("origin")))) return Result::failure(QStringLiteral("Cylinder feature %1 has an invalid origin").arg(feature.id));
        return !feature.parameters.contains(QStringLiteral("axis")) || vector3(feature.parameters.value(QStringLiteral("axis")), true) ? Result::success() : Result::failure(QStringLiteral("Cylinder feature %1 has an invalid axis").arg(feature.id));
    }
    if (feature.type == QStringLiteral("union") || feature.type == QStringLiteral("cut") || feature.type == QStringLiteral("intersection")) {
        if (feature.inputRefs.size() != 2 || !feature.parameters.isEmpty()) return Result::failure(QStringLiteral("Boolean feature %1 requires two inputs and no parameters").arg(feature.id));
        return Result::success();
    }
    if (feature.type == QStringLiteral("translate")) {
        if (const auto result = requireUnary(feature, {"vector"}); !result.ok) return result;
        return vector3(feature.parameters.value(QStringLiteral("vector"))) ? Result::success() : Result::failure(QStringLiteral("Translate feature %1 requires a finite vector").arg(feature.id));
    }
    if (feature.type == QStringLiteral("rotate")) {
        if (const auto result = requireUnary(feature, {"origin", "axis", "radians"}); !result.ok) return result;
        const auto radians = feature.parameters.value(QStringLiteral("radians"));
        return vector3(feature.parameters.value(QStringLiteral("origin"))) && vector3(feature.parameters.value(QStringLiteral("axis")), true) && finiteNumber(radians) && std::abs(radians.toDouble()) <= 1000.0 ? Result::success() : Result::failure(QStringLiteral("Rotate feature %1 has invalid typed parameters").arg(feature.id));
    }
    if (feature.type == QStringLiteral("fillet")) {
        if (const auto result = requireUnary(feature, {"radius"}); !result.ok) return result;
        return boundedPositive(feature.parameters.value(QStringLiteral("radius"))) ? Result::success() : Result::failure(QStringLiteral("Fillet feature %1 requires a finite positive bounded radius").arg(feature.id));
    }
    if (feature.type == QStringLiteral("extrude")) {
        if (!feature.inputRefs.isEmpty() || feature.parameters.size() != 2) return Result::failure(QStringLiteral("Extrude feature %1 requires polygon and vector parameters").arg(feature.id));
        const auto polygon = feature.parameters.value(QStringLiteral("polygon")).toArray();
        if (polygon.size() < 3 || polygon.size() > 4096 || !vector3(feature.parameters.value(QStringLiteral("vector")), true)) return Result::failure(QStringLiteral("Extrude feature %1 has invalid typed parameters").arg(feature.id));
        for (const auto &point : polygon) if (!vector3(point)) return Result::failure(QStringLiteral("Extrude feature %1 has a non-finite point").arg(feature.id));
        const double z = polygon.first().toArray().at(2).toDouble();
        for (const auto &point : polygon) if (std::abs(point.toArray().at(2).toDouble() - z) > 1.0e-7) return Result::failure(QStringLiteral("Extrude feature %1 must use finite coplanar points").arg(feature.id));
        return Result::success();
    }
    if (feature.type == QStringLiteral("validate") || feature.type == QStringLiteral("tessellate")) {
        return requireUnary(feature, {});
    }
    return Result::failure(QStringLiteral("Unsupported feature type: %1").arg(feature.type));
}

} // namespace

bool EvaluationResult::isSuppressed(const QString &id) const
{
    for (const auto &entry : ordered) if (entry.feature.id == id) return entry.state == EvaluationState::Suppressed;
    return false;
}

EvaluationResult ModelEvaluator::evaluate(const DocumentRecord &record)
{
    EvaluationResult output;
    if (const Result document = Document::validate(record); !document.ok) {
        output.result = document;
        return output;
    }

    QHash<QString, int> indexById;
    QVector<int> indegrees(record.features.size());
    QVector<QVector<int>> dependents(record.features.size());
    for (int index = 0; index < record.features.size(); ++index) indexById.insert(record.features[index].id, index);
    for (int index = 0; index < record.features.size(); ++index) {
        const auto &feature = record.features[index];
        if (const Result schema = validateFeature(feature); !schema.ok) { output.result = schema; return output; }
        for (const auto &input : feature.inputRefs) {
            const auto dependency = indexById.constFind(input);
            if (dependency == indexById.cend()) { output.result = Result::failure(QStringLiteral("Unknown feature reference: %1").arg(input)); return output; }
            const bool inputIsSketch=record.features[*dependency].type==QStringLiteral("sketch");
            if ((feature.type==QStringLiteral("pad")) != inputIsSketch) { output.result=Result::failure(QStringLiteral("Feature %1 requires %2 input geometry").arg(feature.id,feature.type==QStringLiteral("pad")?QStringLiteral("sketch"):QStringLiteral("solid"))); return output; }
            ++indegrees[index];
            dependents[*dependency].append(index);
        }
    }

    QQueue<int> ready;
    for (int index = 0; index < indegrees.size(); ++index) if (indegrees[index] == 0) ready.enqueue(index);
    QVector<int> order;
    while (!ready.isEmpty()) {
        const int index = ready.dequeue();
        order.append(index);
        for (const int dependent : dependents[index]) if (--indegrees[dependent] == 0) ready.enqueue(dependent);
    }
    if (order.size() != record.features.size()) { output.result = Result::failure(QStringLiteral("Cyclic feature dependency")); return output; }

    QHash<QString, QString> suppression;
    for (const int index : order) {
        const auto &feature = record.features[index];
        QString source;
        if (feature.suppressed) source = feature.id;
        for (const auto &input : feature.inputRefs) {
            if (suppression.contains(input)) { source = suppression.value(input); break; }
        }
        if (!source.isEmpty()) suppression.insert(feature.id, source);
        output.ordered.append({feature, source.isEmpty() ? EvaluationState::Ready : EvaluationState::Suppressed, source});
    }
    output.result = Result::success();
    return output;
}

} // namespace precision::core
