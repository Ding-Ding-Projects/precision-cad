#include "sketch_model.h"

#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

extern "C" {
#include <slvs.h>
}

namespace precision::sketch {
namespace {
constexpr Slvs_hGroup kGroup = 1;
constexpr Slvs_hEntity kPlaneOrigin = 1, kPlaneNormal = 2, kWorkplane = 3;
constexpr qsizetype kMaxRecords = 4096;
constexpr double kMaxCoordinate = 1e9;
std::mutex solverMutex; // libslvs uses process-global model and temporary storage.

QString idString(quint64 id) { return QString::number(id); }
bool bounded(double value) { return std::isfinite(value) && std::abs(value) <= kMaxCoordinate; }
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }

bool readId(const QJsonObject &object, const char *key, quint64 *out, bool allowZero = false, bool allowEmpty = false) {
    const auto value = object.value(QLatin1String(key));
    if (!value.isString()) return false;
    const auto text = value.toString();
    if (allowEmpty && text.isEmpty()) { *out = 0; return true; }
    if (text.isEmpty() || text.size() > 20 || (text.size() > 1 && text.front() == QLatin1Char('0'))) return false;
    for (const auto ch : text) if (ch < QLatin1Char('0') || ch > QLatin1Char('9')) return false;
    bool ok = false;
    const auto id = text.toULongLong(&ok);
    if (!ok || (!allowZero && id == 0)) return false;
    *out = id;
    return true;
}

bool fields(const QJsonObject &object, std::initializer_list<const char *> names) {
    if (object.size() != static_cast<qsizetype>(names.size())) return false;
    for (const auto name : names) if (!object.contains(QLatin1String(name))) return false;
    return true;
}
bool number(const QJsonObject &object, const char *name, double *out) {
    const auto value = object.value(QLatin1String(name));
    if (!value.isDouble() || !std::isfinite(value.toDouble())) return false;
    *out = value.toDouble();
    return true;
}
bool textField(const QJsonObject &object, const char *name, QString *out) {
    const auto value = object.value(QLatin1String(name));
    if (!value.isString() || value.toString().size() > 1024) return false;
    *out = value.toString();
    return true;
}

using EntityMap = std::unordered_map<SketchEntityId, const SketchEntity *>;
bool circular(SketchEntityKind kind) { return kind == SketchEntityKind::Circle || kind == SketchEntityKind::Arc; }

// Tangency is defined only at one unambiguous, topologically shared endpoint.
bool tangentEnds(const SketchEntity &a, const SketchEntity &b, int *other, int *other2) {
    int matches = 0;
    const std::array<SketchEntityId, 2> ae{a.startPointId, a.endPointId}, be{b.startPointId, b.endPointId};
    for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) if (ae[i] == be[j]) {
        ++matches; *other = i; *other2 = j;
    }
    return matches == 1;
}

bool validate(const SketchModel &model, QString *error) {
    if (model.schemaVersion != kSketchSchemaVersion || model.id.isEmpty() || model.id.size() > 1024 || model.units != QStringLiteral("mm") || model.plane.id.isEmpty() || model.plane.id.size() > 1024)
        return fail(error, QStringLiteral("Sketch schema, identity, datum identity, or units are invalid; only mm is supported."));
    const auto &p = model.plane;
    if (!bounded(p.originX) || !bounded(p.originY) || !bounded(p.originZ) || !std::isfinite(p.normalX) || !std::isfinite(p.normalY) || !std::isfinite(p.normalZ) || std::max({std::abs(p.normalX), std::abs(p.normalY), std::abs(p.normalZ)}) == 0)
        return fail(error, QStringLiteral("The datum requires bounded finite origin coordinates and a finite non-zero normal."));
    if (model.entities.isEmpty() || model.entities.size() > kMaxRecords || model.constraints.size() > kMaxRecords)
        return fail(error, QStringLiteral("Sketch entity count must be 1..4096 and constraint count at most 4096."));
    EntityMap entities;
    for (const auto &e : model.entities) {
        if (!e.id || !entities.emplace(e.id, &e).second || entityKindName(e.kind).isEmpty() || e.label.size() > 1024 || !bounded(e.u) || !bounded(e.v) || !bounded(e.radius))
            return fail(error, QStringLiteral("Entity IDs, kinds, labels, or numeric values are invalid."));
    }
    const auto point = [&](SketchEntityId id) { auto it = entities.find(id); return it != entities.end() && it->second->kind == SketchEntityKind::Point; };
    const auto samePosition = [&](SketchEntityId a, SketchEntityId b) {
        const auto &pa = *entities.at(a), &pb = *entities.at(b);
        return std::hypot(pa.u - pb.u, pa.v - pb.v) < 1e-9;
    };
    for (const auto &e : model.entities) {
        if (e.kind == SketchEntityKind::Point) {
            if (e.startPointId || e.endPointId || e.centerPointId || e.radius != 0) return fail(error, QStringLiteral("Points cannot contain curve fields."));
        } else {
            if (e.u != 0 || e.v != 0) return fail(error, QStringLiteral("Only points store local coordinates."));
            if (e.kind == SketchEntityKind::Circle) {
                if (!point(e.centerPointId) || e.startPointId || e.endPointId || e.radius <= 0) return fail(error, QStringLiteral("Circles require a point center and positive radius only."));
            } else {
                if (!point(e.startPointId) || !point(e.endPointId) || e.startPointId == e.endPointId || samePosition(e.startPointId, e.endPointId) || e.radius != 0)
                    return fail(error, QStringLiteral("Lines and arcs require distinct, nondegenerate point endpoints and no radius field."));
                if (e.kind == SketchEntityKind::Line && e.centerPointId) return fail(error, QStringLiteral("Lines cannot contain a center."));
                if (e.kind == SketchEntityKind::Arc && (!point(e.centerPointId) || samePosition(e.centerPointId, e.startPointId) || samePosition(e.centerPointId, e.endPointId)))
                    return fail(error, QStringLiteral("Arcs require a nondegenerate point center."));
            }
        }
    }
    QSet<SketchConstraintId> ids;
    for (const auto &c : model.constraints) {
        if (!c.id || ids.contains(c.id) || c.label.size() > 1024 || constraintKindName(c.kind).isEmpty() || !bounded(c.value))
            return fail(error, QStringLiteral("Constraint IDs, kinds, labels, or values are invalid."));
        ids.insert(c.id);
        const auto first = entities.find(c.first), second = entities.find(c.second);
        if (first == entities.end()) return fail(error, QStringLiteral("Constraint first entity is unknown."));
        const auto a = first->second->kind;
        const bool unary = c.kind == SketchConstraintKind::Fixed || c.kind == SketchConstraintKind::Horizontal || c.kind == SketchConstraintKind::Vertical || c.kind == SketchConstraintKind::Radius;
        if ((unary && c.second != 0) || (!unary && (second == entities.end() || c.first == c.second))) return fail(error, QStringLiteral("Constraint arity or distinct entity references are invalid."));
        const auto b = unary ? a : second->second->kind;
        const bool dimensional = c.kind == SketchConstraintKind::Distance || c.kind == SketchConstraintKind::Radius || c.kind == SketchConstraintKind::Angle;
        if (!dimensional && c.value != 0) return fail(error, QStringLiteral("Non-dimensional constraints require value zero."));
        bool valid = false;
        switch (c.kind) {
        case SketchConstraintKind::Coincident: valid = a == SketchEntityKind::Point && b == SketchEntityKind::Point; break;
        case SketchConstraintKind::Fixed: valid = a == SketchEntityKind::Point; break;
        case SketchConstraintKind::Horizontal:
        case SketchConstraintKind::Vertical: valid = a == SketchEntityKind::Line; break;
        case SketchConstraintKind::Parallel:
        case SketchConstraintKind::Perpendicular: valid = a == SketchEntityKind::Line && b == SketchEntityKind::Line; break;
        case SketchConstraintKind::Angle: valid = a == SketchEntityKind::Line && b == SketchEntityKind::Line && c.value >= 0 && c.value <= 180; break;
        case SketchConstraintKind::Distance: valid = a == SketchEntityKind::Point && b == SketchEntityKind::Point && c.value >= 0; break;
        case SketchConstraintKind::Radius: valid = a == SketchEntityKind::Circle && c.value > 0; break;
        case SketchConstraintKind::Equal: valid = (a == SketchEntityKind::Line && b == SketchEntityKind::Line) || (circular(a) && circular(b)); break;
        case SketchConstraintKind::Tangent: {
            int endA = 0, endB = 0;
            valid = ((a == SketchEntityKind::Arc && (b == SketchEntityKind::Line || b == SketchEntityKind::Arc)) || (a == SketchEntityKind::Line && b == SketchEntityKind::Arc)) && tangentEnds(*first->second, *second->second, &endA, &endB);
            break;
        }
        }
        if (!valid) return fail(error, QStringLiteral("Constraint %1 has unsupported entity kinds, endpoints, or dimension.").arg(c.id));
    }
    return true;
}

struct Basis { std::array<double, 3> u, v; std::array<double, 4> q; };
Basis datumBasis(const DatumPlane &plane) {
    const double scale = std::max({std::abs(plane.normalX), std::abs(plane.normalY), std::abs(plane.normalZ)});
    std::array<double, 3> n{plane.normalX / scale, plane.normalY / scale, plane.normalZ / scale};
    const double length = std::hypot(n[0], n[1], n[2]);
    for (auto &value : n) value /= length;
    // Project +X into the plane, switching to +Y near parallel to avoid cancellation.
    const int axis = std::abs(n[0]) > 0.9 ? 1 : 0;
    Basis basis{};
    for (int i = 0; i < 3; ++i) basis.u[i] = (i == axis ? 1.0 : 0.0) - n[axis] * n[i];
    const double uLength = std::hypot(basis.u[0], basis.u[1], basis.u[2]);
    for (auto &value : basis.u) value /= uLength;
    basis.v = {n[1] * basis.u[2] - n[2] * basis.u[1], n[2] * basis.u[0] - n[0] * basis.u[2], n[0] * basis.u[1] - n[1] * basis.u[0]};
    Slvs_MakeQuaternion(basis.u[0], basis.u[1], basis.u[2], basis.v[0], basis.v[1], basis.v[2], &basis.q[0], &basis.q[1], &basis.q[2], &basis.q[3]);
    // Use the actual solver orientation also for the returned world-space coordinates.
    Slvs_QuaternionU(basis.q[0], basis.q[1], basis.q[2], basis.q[3], &basis.u[0], &basis.u[1], &basis.u[2]);
    Slvs_QuaternionV(basis.q[0], basis.q[1], basis.q[2], basis.q[3], &basis.v[0], &basis.v[1], &basis.v[2]);
    return basis;
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
    if (!out) return fail(error, QStringLiteral("Output model is required."));
    SketchModel parsed;
    double schema = 0;
    if (!fields(json, {"schemaVersion", "id", "units", "plane", "entities", "constraints"}) ||
        !number(json, "schemaVersion", &schema) || schema != kSketchSchemaVersion ||
        !textField(json, "id", &parsed.id) || !textField(json, "units", &parsed.units) ||
        !json[QStringLiteral("plane")].isObject() || !json[QStringLiteral("entities")].isArray() || !json[QStringLiteral("constraints")].isArray())
        return fail(error, QStringLiteral("Invalid sketch schema, fields, or JSON types."));
    const auto plane = json[QStringLiteral("plane")].toObject();
    if (!fields(plane, {"id", "originX", "originY", "originZ", "normalX", "normalY", "normalZ"}) ||
        !textField(plane, "id", &parsed.plane.id) || !number(plane, "originX", &parsed.plane.originX) ||
        !number(plane, "originY", &parsed.plane.originY) || !number(plane, "originZ", &parsed.plane.originZ) ||
        !number(plane, "normalX", &parsed.plane.normalX) || !number(plane, "normalY", &parsed.plane.normalY) || !number(plane, "normalZ", &parsed.plane.normalZ))
        return fail(error, QStringLiteral("Invalid datum fields or JSON types."));
    const auto entities = json[QStringLiteral("entities")].toArray(), constraints = json[QStringLiteral("constraints")].toArray();
    if (entities.size() > kMaxRecords || constraints.size() > kMaxRecords) return fail(error, QStringLiteral("Sketch record limit exceeded."));
    for (const auto value : entities) {
        if (!value.isObject()) return fail(error, QStringLiteral("Entity must be an object."));
        const auto object = value.toObject(); SketchEntity entity; QString kind;
        if (!fields(object, {"id", "kind", "label", "u", "v", "radius", "startPointId", "endPointId", "centerPointId"}) ||
            !readId(object, "id", &entity.id) || !textField(object, "kind", &kind) || !textField(object, "label", &entity.label) ||
            !number(object, "u", &entity.u) || !number(object, "v", &entity.v) || !number(object, "radius", &entity.radius) ||
            !readId(object, "startPointId", &entity.startPointId, true) || !readId(object, "endPointId", &entity.endPointId, true) || !readId(object, "centerPointId", &entity.centerPointId, true))
            return fail(error, QStringLiteral("Invalid entity fields or JSON types."));
        bool found = false;
        for (auto candidate : {SketchEntityKind::Point, SketchEntityKind::Line, SketchEntityKind::Circle, SketchEntityKind::Arc}) if (entityKindName(candidate) == kind) { entity.kind = candidate; found = true; break; }
        if (!found) return fail(error, QStringLiteral("Unknown entity kind."));
        parsed.entities.push_back(entity);
    }
    for (const auto value : constraints) {
        if (!value.isObject()) return fail(error, QStringLiteral("Constraint must be an object."));
        const auto object = value.toObject(); SketchConstraint constraint; QString kind;
        if (!fields(object, {"id", "kind", "label", "first", "second", "value"}) || !readId(object, "id", &constraint.id) ||
            !textField(object, "kind", &kind) || !textField(object, "label", &constraint.label) ||
            !readId(object, "first", &constraint.first) || !readId(object, "second", &constraint.second, false, true) || !number(object, "value", &constraint.value))
            return fail(error, QStringLiteral("Invalid constraint fields or JSON types."));
        bool found = false;
        for (auto candidate : {SketchConstraintKind::Coincident, SketchConstraintKind::Horizontal, SketchConstraintKind::Vertical, SketchConstraintKind::Parallel, SketchConstraintKind::Perpendicular, SketchConstraintKind::Tangent, SketchConstraintKind::Equal, SketchConstraintKind::Distance, SketchConstraintKind::Radius, SketchConstraintKind::Angle, SketchConstraintKind::Fixed}) if (constraintKindName(candidate) == kind) { constraint.kind = candidate; found = true; break; }
        if (!found) return fail(error, QStringLiteral("Unknown constraint kind."));
        parsed.constraints.push_back(constraint);
    }
    if (!validate(parsed, error)) return false;
    *out = std::move(parsed);
    if (error) error->clear();
    return true;
}

SketchSolveResult SketchSolver::solve(const SketchModel &model) {
    SketchSolveResult result;
    if (!validate(model, &result.diagnostic)) return result;
    // Each namespace is dense and independent of persistent IDs. Bounds are checked
    // before narrowing; the record cap makes the larger native limits unreachable.
    const quint64 maxEntities = 3 + 2 * static_cast<quint64>(model.entities.size());
    const quint64 maxParams = 7 + 2 * static_cast<quint64>(model.entities.size());
    if (std::max(maxEntities, maxParams) >= std::numeric_limits<Slvs_hEntity>::max() || maxParams > std::numeric_limits<int>::max()) {
        result.diagnostic = QStringLiteral("Sketch exceeds native handle capacity."); return result;
    }
    const std::lock_guard lock(solverMutex);
    const auto basis = datumBasis(model.plane);
    std::vector<Slvs_Param> params;
    std::vector<Slvs_Entity> entities;
    std::vector<Slvs_Constraint> constraints;
    std::unordered_map<SketchEntityId, Slvs_hEntity> handles;
    std::unordered_map<SketchEntityId, std::pair<Slvs_hParam, Slvs_hParam>> pointParams;
    std::unordered_map<SketchEntityId, Slvs_hParam> radiusParams;
    EntityMap modelEntities;
    Slvs_hEntity nextEntity = kWorkplane + 1;
    const auto parameter = [&](double value, Slvs_hGroup group = kGroup) {
        const auto handle = static_cast<Slvs_hParam>(params.size() + 1);
        params.push_back(Slvs_MakeParam(handle, group, value)); return handle;
    };
    parameter(model.plane.originX, 0); parameter(model.plane.originY, 0); parameter(model.plane.originZ, 0);
    for (double q : basis.q) parameter(q, 0);
    entities.push_back(Slvs_MakePoint3d(kPlaneOrigin, 0, 1, 2, 3));
    entities.push_back(Slvs_MakeNormal3d(kPlaneNormal, 0, 4, 5, 6, 7));
    entities.push_back(Slvs_MakeWorkplane(kWorkplane, 0, kPlaneOrigin, kPlaneNormal));
    // Reserve all entity handles first, then emit points before their consumers.
    for (const auto &e : model.entities) { handles.emplace(e.id, nextEntity++); modelEntities.emplace(e.id, &e); }
    for (const auto &e : model.entities) if (e.kind == SketchEntityKind::Point) {
        const auto u = parameter(e.u), v = parameter(e.v);
        pointParams.emplace(e.id, std::make_pair(u, v));
        entities.push_back(Slvs_MakePoint2d(handles.at(e.id), kGroup, kWorkplane, u, v));
    }
    for (const auto &e : model.entities) {
        const auto h = handles.at(e.id);
        switch (e.kind) {
        case SketchEntityKind::Point: break;
        case SketchEntityKind::Line: entities.push_back(Slvs_MakeLineSegment(h, kGroup, kWorkplane, handles.at(e.startPointId), handles.at(e.endPointId))); break;
        case SketchEntityKind::Circle: {
            const auto radius = parameter(e.radius); const auto distance = nextEntity++;
            radiusParams.emplace(e.id, radius);
            entities.push_back(Slvs_MakeDistance(distance, kGroup, kWorkplane, radius));
            entities.push_back(Slvs_MakeCircle(h, kGroup, kWorkplane, handles.at(e.centerPointId), kPlaneNormal, distance)); break;
        }
        case SketchEntityKind::Arc: entities.push_back(Slvs_MakeArcOfCircle(h, kGroup, kWorkplane, kPlaneNormal, handles.at(e.centerPointId), handles.at(e.startPointId), handles.at(e.endPointId))); break;
        }
    }
    for (const auto &c : model.constraints) {
        Slvs_Constraint native = Slvs_MakeConstraint(static_cast<Slvs_hConstraint>(constraints.size() + 1), kGroup, 0, kWorkplane, c.value, 0, 0, 0, 0);
        const auto first = handles.at(c.first), second = c.second ? handles.at(c.second) : 0;
        const auto &a = *modelEntities.at(c.first);
        switch (c.kind) {
        case SketchConstraintKind::Coincident: native.type = SLVS_C_POINTS_COINCIDENT; native.ptA = first; native.ptB = second; break;
        case SketchConstraintKind::Horizontal: native.type = SLVS_C_HORIZONTAL; native.entityA = first; break;
        case SketchConstraintKind::Vertical: native.type = SLVS_C_VERTICAL; native.entityA = first; break;
        case SketchConstraintKind::Parallel: native.type = SLVS_C_PARALLEL; native.entityA = first; native.entityB = second; break;
        case SketchConstraintKind::Perpendicular: native.type = SLVS_C_PERPENDICULAR; native.entityA = first; native.entityB = second; break;
        case SketchConstraintKind::Equal: native.type = a.kind == SketchEntityKind::Line ? SLVS_C_EQUAL_LENGTH_LINES : SLVS_C_EQUAL_RADIUS; native.entityA = first; native.entityB = second; break;
        case SketchConstraintKind::Distance: native.type = SLVS_C_PT_PT_DISTANCE; native.ptA = first; native.ptB = second; break;
        case SketchConstraintKind::Radius: native.type = SLVS_C_DIAMETER; native.entityA = first; native.valA *= 2; break;
        case SketchConstraintKind::Angle: native.type = SLVS_C_ANGLE; native.entityA = first; native.entityB = second; break;
        case SketchConstraintKind::Fixed: native.type = SLVS_C_WHERE_DRAGGED; native.ptA = first; break;
        case SketchConstraintKind::Tangent: {
            const auto &b = *modelEntities.at(c.second);
            int endA = 0, endB = 0; tangentEnds(a, b, &endA, &endB);
            if (a.kind == SketchEntityKind::Arc && b.kind == SketchEntityKind::Arc) {
                native.type = SLVS_C_CURVE_CURVE_TANGENT; native.entityA = first; native.entityB = second; native.other = endA; native.other2 = endB;
            } else {
                const bool arcFirst = a.kind == SketchEntityKind::Arc;
                native.type = SLVS_C_ARC_LINE_TANGENT; native.entityA = arcFirst ? first : second; native.entityB = arcFirst ? second : first; native.other = arcFirst ? endA : endB;
            }
            break;
        }
        }
        constraints.push_back(native);
    }
    std::vector<Slvs_hConstraint> failed(constraints.size());
    Slvs_System system{};
    system.param = params.data(); system.params = static_cast<int>(params.size());
    system.entity = entities.data(); system.entities = static_cast<int>(entities.size());
    system.constraint = constraints.data(); system.constraints = static_cast<int>(constraints.size());
    system.calculateFaileds = 1; system.failed = failed.data(); system.faileds = static_cast<int>(failed.size());
    Slvs_Solve(&system, kGroup);
    result.remainingDof = system.dof;
    if (system.result == SLVS_RESULT_OKAY || system.result == SLVS_RESULT_REDUNDANT_OKAY) result.status = system.dof == 0 ? SolveStatus::Solved : SolveStatus::UnderConstrained;
    else if (system.result == SLVS_RESULT_INCONSISTENT) result.status = SolveStatus::OverConstrained;
    else result.status = SolveStatus::DidNotConverge;
    for (int i = 0; i < std::min(system.faileds, static_cast<int>(failed.size())); ++i)
        if (failed[i] > 0 && failed[i] <= model.constraints.size()) result.conflictingConstraints.push_back(model.constraints[failed[i] - 1].id);
    // Failed solves never expose tentative geometry as a replacement for valid data.
    if (result.solved()) {
        for (const auto &[id, pair] : pointParams) {
            const double u = params[pair.first - 1].val, v = params[pair.second - 1].val;
            result.points.push_back({id, u, v, model.plane.originX + u * basis.u[0] + v * basis.v[0], model.plane.originY + u * basis.u[1] + v * basis.v[1], model.plane.originZ + u * basis.u[2] + v * basis.v[2]});
        }
        for (const auto &[id, handle] : radiusParams) result.radii.push_back({id, params[handle - 1].val});
        for (const auto &p : result.points) if (!std::isfinite(p.u) || !std::isfinite(p.v) || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) result.status = SolveStatus::DidNotConverge;
        for (const auto &r : result.radii) if (!std::isfinite(r.radius) || r.radius <= 0) result.status = SolveStatus::DidNotConverge;
        if (!result.solved()) { result.points.clear(); result.radii.clear(); }
    }
    std::sort(result.points.begin(), result.points.end(), [](const auto &a, const auto &b) { return a.id < b.id; });
    std::sort(result.radii.begin(), result.radii.end(), [](const auto &a, const auto &b) { return a.id < b.id; });
    result.diagnostic = QStringLiteral("libslvs result=%1, remainingDoF=%2").arg(system.result).arg(system.dof);
    return result;
}

} // namespace precision::sketch
