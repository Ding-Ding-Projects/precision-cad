#include "sketch_model.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

extern "C" {
#include <slvs.h>
}

namespace precision::sketch {
namespace {
constexpr Slvs_hGroup kGroup = 1;
constexpr Slvs_hEntity kPlaneOrigin = 10;
constexpr Slvs_hEntity kPlaneNormal = 11;
constexpr Slvs_hEntity kWorkplane = 12;
constexpr Slvs_hParam kPlaneParamBase = 10;
constexpr quint64 kPointEntityBase = 1000;
constexpr quint64 kLineEntityBase = 100000000;
constexpr quint64 kCircleEntityBase = 200000000;
constexpr quint64 kArcEntityBase = 300000000;
constexpr quint64 kRadiusEntityBase = 400000000;
constexpr quint64 kParamBase = 500000000;
constexpr quint64 kConstraintBase = 1000;

bool toHandle(quint64 input, uint32_t *out) {
    if (input == 0 || input > std::numeric_limits<uint32_t>::max()) return false;
    *out = static_cast<uint32_t>(input);
    return true;
}

QString idString(quint64 id) { return QString::number(id); }
bool readId(const QJsonObject &object, const char *key, quint64 *out) {
    bool ok = false;
    const auto parsed = object.value(QLatin1String(key)).toString().toULongLong(&ok);
    if (!ok || parsed == 0) return false;
    *out = parsed;
    return true;
}

QString statusName(SolveStatus value) {
    switch (value) {
    case SolveStatus::Solved: return QStringLiteral("solved");
    case SolveStatus::UnderConstrained: return QStringLiteral("underConstrained");
    case SolveStatus::OverConstrained: return QStringLiteral("overConstrained");
    case SolveStatus::DidNotConverge: return QStringLiteral("didNotConverge");
    case SolveStatus::InvalidModel: return QStringLiteral("invalidModel");
    }
    return QStringLiteral("invalidModel");
}

std::optional<SketchEntityKind> entityKindFromName(const QString &value) {
    if (value == QStringLiteral("point")) return SketchEntityKind::Point;
    if (value == QStringLiteral("line")) return SketchEntityKind::Line;
    if (value == QStringLiteral("circle")) return SketchEntityKind::Circle;
    if (value == QStringLiteral("arc")) return SketchEntityKind::Arc;
    return std::nullopt;
}

std::optional<SketchConstraintKind> constraintKindFromName(const QString &value) {
    static const QHash<QString, SketchConstraintKind> kinds{
        {QStringLiteral("coincident"), SketchConstraintKind::Coincident}, {QStringLiteral("horizontal"), SketchConstraintKind::Horizontal},
        {QStringLiteral("vertical"), SketchConstraintKind::Vertical}, {QStringLiteral("parallel"), SketchConstraintKind::Parallel},
        {QStringLiteral("perpendicular"), SketchConstraintKind::Perpendicular}, {QStringLiteral("tangent"), SketchConstraintKind::Tangent},
        {QStringLiteral("equal"), SketchConstraintKind::Equal}, {QStringLiteral("distance"), SketchConstraintKind::Distance},
        {QStringLiteral("radius"), SketchConstraintKind::Radius}, {QStringLiteral("angle"), SketchConstraintKind::Angle},
        {QStringLiteral("fixed"), SketchConstraintKind::Fixed}};
    const auto it = kinds.constFind(value);
    return it == kinds.cend() ? std::nullopt : std::optional<SketchConstraintKind>(*it);
}

bool finite(double value) { return std::isfinite(value); }

struct Handles {
    std::unordered_map<SketchEntityId, Slvs_hEntity> entity;
    std::unordered_map<SketchEntityId, Slvs_hEntity> radius;
    std::unordered_map<SketchEntityId, std::pair<Slvs_hParam, Slvs_hParam>> points;
    std::unordered_map<SketchEntityId, SketchEntityKind> kind;
};

bool addConstraint(const SketchConstraint &constraint, const Handles &handles, std::vector<Slvs_Constraint> *output, QString *error) {
    uint32_t rawHandle = 0;
    if (!toHandle(kConstraintBase + constraint.id, &rawHandle)) { *error = QStringLiteral("Constraint ID is outside libslvs handle range."); return false; }
    const auto first = handles.entity.find(constraint.first);
    const auto second = handles.entity.find(constraint.second);
    const auto requireFirst = [&]() { if (first == handles.entity.end()) { *error = QStringLiteral("Constraint %1 refers to an unknown first entity.").arg(idString(constraint.id)); return false; } return true; };
    const auto requireSecond = [&]() { if (second == handles.entity.end()) { *error = QStringLiteral("Constraint %1 refers to an unknown second entity.").arg(idString(constraint.id)); return false; } return true; };
    int type = 0;
    Slvs_hEntity pointA = 0, pointB = 0, entityA = 0, entityB = 0;
    double value = constraint.value;
    switch (constraint.kind) {
    case SketchConstraintKind::Coincident: if (!requireFirst() || !requireSecond()) return false; type = SLVS_C_POINTS_COINCIDENT; pointA = first->second; pointB = second->second; break;
    case SketchConstraintKind::Horizontal: if (!requireFirst() || handles.kind.at(constraint.first) != SketchEntityKind::Line) { *error = QStringLiteral("Horizontal constraints need a line."); return false; } type = SLVS_C_HORIZONTAL; entityA = first->second; break;
    case SketchConstraintKind::Vertical: if (!requireFirst() || handles.kind.at(constraint.first) != SketchEntityKind::Line) { *error = QStringLiteral("Vertical constraints need a line."); return false; } type = SLVS_C_VERTICAL; entityA = first->second; break;
    case SketchConstraintKind::Parallel: if (!requireFirst() || !requireSecond() || handles.kind.at(constraint.first) != SketchEntityKind::Line || handles.kind.at(constraint.second) != SketchEntityKind::Line) { *error = QStringLiteral("Parallel constraints need two lines."); return false; } type = SLVS_C_PARALLEL; entityA = first->second; entityB = second->second; break;
    case SketchConstraintKind::Perpendicular: if (!requireFirst() || !requireSecond() || handles.kind.at(constraint.first) != SketchEntityKind::Line || handles.kind.at(constraint.second) != SketchEntityKind::Line) { *error = QStringLiteral("Perpendicular constraints need two lines."); return false; } type = SLVS_C_PERPENDICULAR; entityA = first->second; entityB = second->second; break;
    case SketchConstraintKind::Tangent: if (!requireFirst() || !requireSecond()) return false; if ((handles.kind.at(constraint.first) == SketchEntityKind::Arc && handles.kind.at(constraint.second) == SketchEntityKind::Line) || (handles.kind.at(constraint.second) == SketchEntityKind::Arc && handles.kind.at(constraint.first) == SketchEntityKind::Line)) type = SLVS_C_ARC_LINE_TANGENT; else if (handles.kind.at(constraint.first) != SketchEntityKind::Point && handles.kind.at(constraint.second) != SketchEntityKind::Point) type = SLVS_C_CURVE_CURVE_TANGENT; else { *error = QStringLiteral("Tangent constraints need supported curve entities."); return false; } entityA = first->second; entityB = second->second; break;
    case SketchConstraintKind::Equal: if (!requireFirst() || !requireSecond()) return false; if (handles.kind.at(constraint.first) == SketchEntityKind::Line && handles.kind.at(constraint.second) == SketchEntityKind::Line) type = SLVS_C_EQUAL_LENGTH_LINES; else if ((handles.kind.at(constraint.first) == SketchEntityKind::Circle || handles.kind.at(constraint.first) == SketchEntityKind::Arc) && (handles.kind.at(constraint.second) == SketchEntityKind::Circle || handles.kind.at(constraint.second) == SketchEntityKind::Arc)) type = SLVS_C_EQUAL_RADIUS; else { *error = QStringLiteral("Equal constraints need two lines or two circular entities."); return false; } entityA = first->second; entityB = second->second; break;
    case SketchConstraintKind::Distance: if (!requireFirst() || !requireSecond() || handles.kind.at(constraint.first) != SketchEntityKind::Point || handles.kind.at(constraint.second) != SketchEntityKind::Point || !finite(value) || value < 0.0) { *error = QStringLiteral("Distance constraints need two points and a finite non-negative value."); return false; } type = SLVS_C_PT_PT_DISTANCE; pointA = first->second; pointB = second->second; break;
    case SketchConstraintKind::Radius: if (!requireFirst() || handles.kind.at(constraint.first) != SketchEntityKind::Circle || !finite(value) || value <= 0.0) { *error = QStringLiteral("Radius constraints need a circle and a finite positive value."); return false; } type = SLVS_C_DIAMETER; entityA = first->second; value *= 2.0; break;
    case SketchConstraintKind::Angle: if (!requireFirst() || !requireSecond() || handles.kind.at(constraint.first) != SketchEntityKind::Line || handles.kind.at(constraint.second) != SketchEntityKind::Line || !finite(value)) { *error = QStringLiteral("Angle constraints need two lines and a finite degree value."); return false; } type = SLVS_C_ANGLE; entityA = first->second; entityB = second->second; break;
    case SketchConstraintKind::Fixed: if (!requireFirst() || handles.kind.at(constraint.first) != SketchEntityKind::Point) { *error = QStringLiteral("Fixed constraints need a point."); return false; } type = SLVS_C_WHERE_DRAGGED; pointA = first->second; break;
    }
    output->push_back(Slvs_MakeConstraint(rawHandle, kGroup, type, kWorkplane, value, pointA, pointB, entityA, entityB));
    return true;
}
} // namespace

QString entityKindName(SketchEntityKind kind) { switch (kind) { case SketchEntityKind::Point: return QStringLiteral("point"); case SketchEntityKind::Line: return QStringLiteral("line"); case SketchEntityKind::Circle: return QStringLiteral("circle"); case SketchEntityKind::Arc: return QStringLiteral("arc"); } return {}; }
QString constraintKindName(SketchConstraintKind kind) { switch (kind) { case SketchConstraintKind::Coincident: return QStringLiteral("coincident"); case SketchConstraintKind::Horizontal: return QStringLiteral("horizontal"); case SketchConstraintKind::Vertical: return QStringLiteral("vertical"); case SketchConstraintKind::Parallel: return QStringLiteral("parallel"); case SketchConstraintKind::Perpendicular: return QStringLiteral("perpendicular"); case SketchConstraintKind::Tangent: return QStringLiteral("tangent"); case SketchConstraintKind::Equal: return QStringLiteral("equal"); case SketchConstraintKind::Distance: return QStringLiteral("distance"); case SketchConstraintKind::Radius: return QStringLiteral("radius"); case SketchConstraintKind::Angle: return QStringLiteral("angle"); case SketchConstraintKind::Fixed: return QStringLiteral("fixed"); } return {}; }

QJsonObject SketchModel::toJson() const {
    QJsonArray entitiesJson, constraintsJson;
    for (const auto &entity : entities) entitiesJson.append(QJsonObject{{QStringLiteral("id"), idString(entity.id)}, {QStringLiteral("kind"), entityKindName(entity.kind)}, {QStringLiteral("label"), entity.label}, {QStringLiteral("u"), entity.u}, {QStringLiteral("v"), entity.v}, {QStringLiteral("radius"), entity.radius}, {QStringLiteral("startPointId"), idString(entity.startPointId)}, {QStringLiteral("endPointId"), idString(entity.endPointId)}, {QStringLiteral("centerPointId"), idString(entity.centerPointId)}});
    for (const auto &constraint : constraints) constraintsJson.append(QJsonObject{{QStringLiteral("id"), idString(constraint.id)}, {QStringLiteral("kind"), constraintKindName(constraint.kind)}, {QStringLiteral("label"), constraint.label}, {QStringLiteral("first"), idString(constraint.first)}, {QStringLiteral("second"), constraint.second ? idString(constraint.second) : QString()}, {QStringLiteral("value"), constraint.value}});
    return {{QStringLiteral("schemaVersion"), schemaVersion}, {QStringLiteral("id"), id}, {QStringLiteral("units"), units}, {QStringLiteral("plane"), QJsonObject{{QStringLiteral("id"), plane.id}, {QStringLiteral("originX"), plane.originX}, {QStringLiteral("originY"), plane.originY}, {QStringLiteral("originZ"), plane.originZ}, {QStringLiteral("normalX"), plane.normalX}, {QStringLiteral("normalY"), plane.normalY}, {QStringLiteral("normalZ"), plane.normalZ}}}, {QStringLiteral("entities"), entitiesJson}, {QStringLiteral("constraints"), constraintsJson}};
}

bool SketchModel::fromJson(const QJsonObject &json, SketchModel *out, QString *error) {
    if (!out) { if (error) *error = QStringLiteral("Output model is required."); return false; }
    SketchModel parsed; parsed.schemaVersion = json.value(QStringLiteral("schemaVersion")).toInt(-1); parsed.id = json.value(QStringLiteral("id")).toString(); parsed.units = json.value(QStringLiteral("units")).toString();
    const auto planeJson = json.value(QStringLiteral("plane")).toObject(); parsed.plane = {planeJson.value(QStringLiteral("id")).toString(), planeJson.value(QStringLiteral("originX")).toDouble(), planeJson.value(QStringLiteral("originY")).toDouble(), planeJson.value(QStringLiteral("originZ")).toDouble(), planeJson.value(QStringLiteral("normalX")).toDouble(), planeJson.value(QStringLiteral("normalY")).toDouble(), planeJson.value(QStringLiteral("normalZ")).toDouble()};
    for (const auto value : json.value(QStringLiteral("entities")).toArray()) { const auto object = value.toObject(); SketchEntity entity; if (!readId(object, "id", &entity.id)) { if (error) *error = QStringLiteral("Entity IDs must be non-zero decimal strings."); return false; } const auto kind = entityKindFromName(object.value(QStringLiteral("kind")).toString()); if (!kind) { if (error) *error = QStringLiteral("Unknown entity kind."); return false; } entity.kind = *kind; entity.label = object.value(QStringLiteral("label")).toString(); entity.u = object.value(QStringLiteral("u")).toDouble(); entity.v = object.value(QStringLiteral("v")).toDouble(); entity.radius = object.value(QStringLiteral("radius")).toDouble(); readId(object, "startPointId", &entity.startPointId); readId(object, "endPointId", &entity.endPointId); readId(object, "centerPointId", &entity.centerPointId); parsed.entities.push_back(entity); }
    for (const auto value : json.value(QStringLiteral("constraints")).toArray()) { const auto object = value.toObject(); SketchConstraint constraint; if (!readId(object, "id", &constraint.id) || !readId(object, "first", &constraint.first)) { if (error) *error = QStringLiteral("Constraint IDs and first entity IDs must be non-zero decimal strings."); return false; } const auto kind = constraintKindFromName(object.value(QStringLiteral("kind")).toString()); if (!kind) { if (error) *error = QStringLiteral("Unknown constraint kind."); return false; } constraint.kind = *kind; constraint.label = object.value(QStringLiteral("label")).toString(); readId(object, "second", &constraint.second); constraint.value = object.value(QStringLiteral("value")).toDouble(); parsed.constraints.push_back(constraint); }
    *out = std::move(parsed); return true;
}

SketchSolveResult SketchSolver::solve(const SketchModel &model) {
    SketchSolveResult result;
    if (model.schemaVersion != kSketchSchemaVersion || model.id.isEmpty() || model.units.isEmpty() || !finite(model.plane.originX) || !finite(model.plane.originY) || !finite(model.plane.originZ) || !finite(model.plane.normalX) || !finite(model.plane.normalY) || !finite(model.plane.normalZ)) { result.diagnostic = QStringLiteral("The sketch record has invalid schema, identity, units, or datum-plane coordinates."); return result; }
    const double normalLength = std::sqrt(model.plane.normalX * model.plane.normalX + model.plane.normalY * model.plane.normalY + model.plane.normalZ * model.plane.normalZ);
    if (normalLength < 1e-12) { result.diagnostic = QStringLiteral("The datum-plane normal is zero."); return result; }
    std::vector<Slvs_Param> params;
    std::vector<Slvs_Entity> entities;
    std::vector<Slvs_Constraint> constraints;
    Handles handles;
    auto addParam = [&](Slvs_hParam handle, double value, Slvs_hGroup group = kGroup) { params.push_back(Slvs_MakeParam(handle, group, value)); };
    for (int i = 0; i < 3; ++i) addParam(kPlaneParamBase + i, (&model.plane.originX)[i], 0);
    addParam(kPlaneParamBase + 3, 1.0, 0); addParam(kPlaneParamBase + 4, 0.0, 0); addParam(kPlaneParamBase + 5, 0.0, 0); addParam(kPlaneParamBase + 6, 0.0, 0);
    entities.push_back(Slvs_MakePoint3d(kPlaneOrigin, 0, kPlaneParamBase, kPlaneParamBase + 1, kPlaneParamBase + 2));
    entities.push_back(Slvs_MakeNormal3d(kPlaneNormal, 0, kPlaneParamBase + 3, kPlaneParamBase + 4, kPlaneParamBase + 5, kPlaneParamBase + 6));
    entities.push_back(Slvs_MakeWorkplane(kWorkplane, 0, kPlaneOrigin, kPlaneNormal));
    for (const auto &entity : model.entities) {
        if (entity.id == 0 || handles.entity.contains(entity.id)) { result.diagnostic = QStringLiteral("Entity IDs must be unique and non-zero."); return result; }
        uint32_t raw = 0; if (!toHandle(kPointEntityBase + entity.id, &raw)) { result.diagnostic = QStringLiteral("Entity ID is outside libslvs handle range."); return result; }
        if (entity.kind != SketchEntityKind::Point) continue;
        uint32_t u = 0, v = 0; if (!toHandle(kParamBase + entity.id * 2, &u) || !toHandle(kParamBase + entity.id * 2 + 1, &v) || !finite(entity.u) || !finite(entity.v)) { result.diagnostic = QStringLiteral("Point ID or coordinates are invalid."); return result; }
        addParam(u, entity.u); addParam(v, entity.v); entities.push_back(Slvs_MakePoint2d(raw, kGroup, kWorkplane, u, v)); handles.entity.emplace(entity.id, raw); handles.points.emplace(entity.id, std::make_pair(u, v)); handles.kind.emplace(entity.id, entity.kind);
    }
    for (const auto &entity : model.entities) {
        if (entity.kind == SketchEntityKind::Point) continue;
        uint32_t raw = 0; const quint64 base = entity.kind == SketchEntityKind::Line ? kLineEntityBase : entity.kind == SketchEntityKind::Circle ? kCircleEntityBase : kArcEntityBase;
        if (!toHandle(base + entity.id, &raw) || handles.entity.contains(entity.id)) { result.diagnostic = QStringLiteral("Non-point entity IDs must be unique and inside libslvs range."); return result; }
        const auto start = handles.entity.find(entity.startPointId), end = handles.entity.find(entity.endPointId), center = handles.entity.find(entity.centerPointId);
        if (entity.kind == SketchEntityKind::Line) { if (start == handles.entity.end() || end == handles.entity.end()) { result.diagnostic = QStringLiteral("A line requires known start and end points."); return result; } entities.push_back(Slvs_MakeLineSegment(raw, kGroup, kWorkplane, start->second, end->second)); }
        if (entity.kind == SketchEntityKind::Circle) { if (center == handles.entity.end() || !finite(entity.radius) || entity.radius <= 0.0) { result.diagnostic = QStringLiteral("A circle requires a known center and positive radius."); return result; } uint32_t radiusEntity = 0, radiusParam = 0; if (!toHandle(kRadiusEntityBase + entity.id, &radiusEntity) || !toHandle(kParamBase + entity.id * 2 + 1, &radiusParam)) { result.diagnostic = QStringLiteral("Circle ID is outside libslvs handle range."); return result; } addParam(radiusParam, entity.radius); entities.push_back(Slvs_MakeDistance(radiusEntity, kGroup, kWorkplane, radiusParam)); entities.push_back(Slvs_MakeCircle(raw, kGroup, kWorkplane, center->second, kPlaneNormal, radiusEntity)); handles.radius.emplace(entity.id, radiusEntity); }
        if (entity.kind == SketchEntityKind::Arc) { if (center == handles.entity.end() || start == handles.entity.end() || end == handles.entity.end()) { result.diagnostic = QStringLiteral("An arc requires known center, start, and end points."); return result; } entities.push_back(Slvs_MakeArcOfCircle(raw, kGroup, kWorkplane, kPlaneNormal, center->second, start->second, end->second)); }
        handles.entity.emplace(entity.id, raw); handles.kind.emplace(entity.id, entity.kind);
    }
    std::unordered_map<SketchConstraintId, bool> constraintIds;
    for (const auto &constraint : model.constraints) { if (constraint.id == 0 || constraintIds.contains(constraint.id)) { result.diagnostic = QStringLiteral("Constraint IDs must be unique and non-zero."); return result; } constraintIds.emplace(constraint.id, true); if (!addConstraint(constraint, handles, &constraints, &result.diagnostic)) return result; }
    std::vector<Slvs_hConstraint> failed(constraints.size());
    Slvs_System system{}; system.param = params.data(); system.params = static_cast<int>(params.size()); system.entity = entities.data(); system.entities = static_cast<int>(entities.size()); system.constraint = constraints.data(); system.constraints = static_cast<int>(constraints.size()); system.calculateFaileds = 1; system.failed = failed.data(); system.faileds = static_cast<int>(failed.size());
    Slvs_Solve(&system, kGroup);
    result.remainingDof = system.dof;
    if (system.result == SLVS_RESULT_OKAY || system.result == SLVS_RESULT_REDUNDANT_OKAY) result.status = system.dof == 0 ? SolveStatus::Solved : SolveStatus::UnderConstrained;
    else if (system.result == SLVS_RESULT_INCONSISTENT) result.status = SolveStatus::OverConstrained;
    else result.status = SolveStatus::DidNotConverge;
    for (int index = 0; index < system.faileds; ++index) { const auto found = std::find_if(model.constraints.cbegin(), model.constraints.cend(), [&](const auto &candidate) { return kConstraintBase + candidate.id == failed[index]; }); if (found != model.constraints.cend()) result.conflictingConstraints.push_back(found->id); }
    for (const auto &[id, param] : handles.points) result.points.push_back({id, params[std::find_if(params.cbegin(), params.cend(), [&](const auto &value) { return value.h == param.first; }) - params.cbegin()].val, params[std::find_if(params.cbegin(), params.cend(), [&](const auto &value) { return value.h == param.second; }) - params.cbegin()].val});
    for (const auto &[id, radius] : handles.radius) { const auto entity = std::find_if(entities.cbegin(), entities.cend(), [&](const auto &value) { return value.h == radius; }); if (entity != entities.cend()) { const auto param = std::find_if(params.cbegin(), params.cend(), [&](const auto &value) { return value.h == entity->param[0]; }); if (param != params.cend()) result.radii.push_back({id, param->val}); } }
    std::sort(result.points.begin(), result.points.end(), [](const auto &a, const auto &b) { return a.id < b.id; }); std::sort(result.radii.begin(), result.radii.end(), [](const auto &a, const auto &b) { return a.id < b.id; });
    result.diagnostic = QStringLiteral("libslvs result=%1, remainingDoF=%2").arg(system.result).arg(system.dof);
    return result;
}

} // namespace precision::sketch
