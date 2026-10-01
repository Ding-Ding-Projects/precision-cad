#include "sketch_model.h"

#include <QCoreApplication>
#include <cstdio>
#include <cmath>
#include <QJsonArray>
#include <limits>
#include <array>
#include <algorithm>

using namespace precision::sketch;
static int assertions = 0;
#define REQUIRE(condition) do { ++assertions; if (!(condition)) { std::fprintf(stderr, "failed: %s at %d\n", #condition, __LINE__); return 1; } } while (false)

static SketchEntity point(SketchEntityId id, double u, double v) { return {id, SketchEntityKind::Point, QStringLiteral("point-%1").arg(id), u, v}; }
static SketchEntity line(SketchEntityId id, SketchEntityId start, SketchEntityId end) { SketchEntity item; item.id = id; item.kind = SketchEntityKind::Line; item.startPointId = start; item.endPointId = end; return item; }
static SketchEntity circle(SketchEntityId id, SketchEntityId center, double radius) { SketchEntity item; item.id = id; item.kind = SketchEntityKind::Circle; item.centerPointId = center; item.radius = radius; return item; }
static SketchEntity arc(SketchEntityId id, SketchEntityId center, SketchEntityId start, SketchEntityId end) { SketchEntity item; item.id = id; item.kind = SketchEntityKind::Arc; item.centerPointId = center; item.startPointId = start; item.endPointId = end; return item; }
static SketchConstraint constraint(SketchConstraintId id, SketchConstraintKind kind, SketchEntityId first, SketchEntityId second = 0, double value = 0.0) { return {id, kind, QStringLiteral("constraint-%1").arg(id), first, second, value}; }
static const SolvedPoint *findPoint(const SketchSolveResult &result, SketchEntityId id) { for (const auto &point : result.points) if (point.id == id) return &point; return nullptr; }

static bool near(double a, double b) { return std::abs(a - b) < 1e-6; }
static SketchModel baseModel() { SketchModel m; m.id = QStringLiteral("fixture"); return m; }
static void fixPoints(SketchModel &m) {
    for (const auto &e : m.entities) if (e.kind == SketchEntityKind::Point) m.constraints.push_back(constraint(100 + e.id, SketchConstraintKind::Fixed, e.id));
}

static int testConstraintKinds() {
    for (const auto kind : {SketchConstraintKind::Coincident, SketchConstraintKind::Horizontal, SketchConstraintKind::Vertical}) {
        auto moving = baseModel(); moving.entities = {point(1, 0, 0), point(2, 4, 3)};
        moving.constraints = {constraint(1, SketchConstraintKind::Fixed, 1)};
        if (kind == SketchConstraintKind::Coincident) moving.constraints.push_back(constraint(2, kind, 1, 2));
        else { moving.entities.push_back(line(10, 1, 2)); moving.constraints.push_back(constraint(2, kind, 10)); }
        const auto solved = SketchSolver::solve(moving); REQUIRE(solved.solved());
        const auto p = findPoint(solved, 2); REQUIRE(p);
        if (kind != SketchConstraintKind::Horizontal) REQUIRE(near(p->u, 0));
        if (kind != SketchConstraintKind::Vertical) REQUIRE(near(p->v, 0));
        REQUIRE(solved.remainingDof == (kind == SketchConstraintKind::Coincident ? 0 : 1));
    }
    for (const auto kind : {SketchConstraintKind::Parallel, SketchConstraintKind::Perpendicular, SketchConstraintKind::Equal, SketchConstraintKind::Angle}) {
        auto moving = baseModel(); moving.entities = {point(1, 0, 0), point(2, 10, 0), point(3, 0, 5), point(4, 3, 9), line(10, 1, 2), line(11, 3, 4)};
        moving.constraints = {constraint(1, SketchConstraintKind::Fixed, 1), constraint(2, SketchConstraintKind::Fixed, 2), constraint(3, SketchConstraintKind::Fixed, 3), constraint(4, kind, 10, 11, kind == SketchConstraintKind::Angle ? 60 : 0)};
        const auto solved = SketchSolver::solve(moving); REQUIRE(solved.status == SolveStatus::UnderConstrained); REQUIRE(solved.remainingDof == 1);
        const auto p = findPoint(solved, 4); REQUIRE(p); const double dx = p->u, dy = p->v - 5;
        if (kind == SketchConstraintKind::Parallel) REQUIRE(near(dy, 0));
        if (kind == SketchConstraintKind::Perpendicular) REQUIRE(near(dx, 0));
        if (kind == SketchConstraintKind::Equal) REQUIRE(near(std::hypot(dx, dy), 10));
        if (kind == SketchConstraintKind::Angle) REQUIRE(near(dx / std::hypot(dx, dy), 0.5));
    }
    // Each relation is exercised in the actual solver. A wrong entity kind is
    // then rejected both directly and at the persistence boundary.
    const std::array kinds{SketchConstraintKind::Coincident, SketchConstraintKind::Horizontal, SketchConstraintKind::Vertical, SketchConstraintKind::Parallel, SketchConstraintKind::Perpendicular, SketchConstraintKind::Equal, SketchConstraintKind::Distance, SketchConstraintKind::Radius, SketchConstraintKind::Angle, SketchConstraintKind::Fixed};
    for (const auto kind : kinds) {
        auto m = baseModel();
        m.entities = {point(1, 0, 0), point(2, 10, 0), point(3, 0, 5), point(4, 10, 5), line(10, 1, 2), line(11, 3, 4), circle(12, 3, 2)};
        SketchConstraint c = constraint(1, kind, 10);
        switch (kind) {
        case SketchConstraintKind::Coincident: m.entities[2].u = 0; m.entities[2].v = 0; c.first = 1; c.second = 3; break;
        case SketchConstraintKind::Horizontal: break;
        case SketchConstraintKind::Vertical: m.entities[1].u = 0; m.entities[1].v = 10; break;
        case SketchConstraintKind::Parallel: c.second = 11; break;
        case SketchConstraintKind::Perpendicular: m.entities[3].u = 0; m.entities[3].v = 15; c.second = 11; break;
        case SketchConstraintKind::Equal: c.second = 11; break;
        case SketchConstraintKind::Distance: c.first = 1; c.second = 2; c.value = 10; break;
        case SketchConstraintKind::Radius: c.first = 12; c.value = 2; break;
        case SketchConstraintKind::Angle: m.entities[3].u = 5; m.entities[3].v = 5 + 5 * std::sqrt(3.0); c.second = 11; c.value = 60; break;
        case SketchConstraintKind::Fixed: c.first = 1; break;
        case SketchConstraintKind::Tangent: break;
        }
        fixPoints(m); m.constraints.push_back(c);
        const auto before = m.toJson(); const auto result = SketchSolver::solve(m);
        if (!result.solved()) std::fprintf(stderr, "kind %s: %s\n", qPrintable(constraintKindName(kind)), qPrintable(result.diagnostic));
        REQUIRE(result.solved()); REQUIRE(m.toJson() == before); REQUIRE(result.points.size() == 4);
        for (const auto &p : result.points) { const auto &expected = m.entities[static_cast<int>(p.id) - 1]; REQUIRE(near(p.u, expected.u)); REQUIRE(near(p.v, expected.v)); }
        m.constraints.last().first = kind == SketchConstraintKind::Equal ? 1 : 12;
        if (kind == SketchConstraintKind::Radius) m.constraints.last().first = 10;
        REQUIRE(SketchSolver::solve(m).status == SolveStatus::InvalidModel);
        SketchModel retained = baseModel(); const auto retainedJson = retained.toJson(); QString error;
        REQUIRE(!SketchModel::fromJson(m.toJson(), &retained, &error)); REQUIRE(!error.isEmpty()); REQUIRE(retained.toJson() == retainedJson);
    }
    auto equalCircles = baseModel(); equalCircles.entities = {point(1, 0, 0), point(2, 20, 0), circle(10, 1, 2), circle(11, 2, 3)};
    fixPoints(equalCircles); equalCircles.constraints.push_back(constraint(1, SketchConstraintKind::Radius, 10, 0, 7)); equalCircles.constraints.push_back(constraint(2, SketchConstraintKind::Equal, 10, 11));
    const auto eq = SketchSolver::solve(equalCircles); REQUIRE(eq.status == SolveStatus::Solved); REQUIRE(eq.radii.size() == 2); REQUIRE(near(eq.radii[0].radius, 7)); REQUIRE(near(eq.radii[1].radius, 7));
    // Equal radii also accepts an arc and circle, and two arcs.
    auto mixed = baseModel(); mixed.entities = {point(1, 0, 0), point(2, 2, 0), point(3, 0, 2), point(4, 10, 0), arc(10, 1, 2, 3), circle(11, 4, 3)}; fixPoints(mixed);
    mixed.constraints.push_back(constraint(1, SketchConstraintKind::Equal, 10, 11));
    const auto mr = SketchSolver::solve(mixed); REQUIRE(mr.solved()); REQUIRE(near(mr.radii[0].radius, 2));
    mixed.entities.removeLast(); mixed.entities.push_back(point(5, 12, 0)); mixed.entities.push_back(point(6, 10, 2)); mixed.entities.push_back(arc(11, 4, 5, 6));
    mixed.constraints.clear(); fixPoints(mixed); mixed.constraints.push_back(constraint(1, SketchConstraintKind::Equal, 10, 11)); REQUIRE(SketchSolver::solve(mixed).solved());
    return 0;
}

static int testTangency() {
    for (bool atEnd : {false, true}) for (bool reversed : {false, true}) for (bool lineEnd : {false, true}) {
        auto m = baseModel();
        m.entities = {point(1, 0, 0), point(2, 1, 0), point(3, 0, 1), point(4, 1, 2), arc(10, 1, atEnd ? 3 : 2, atEnd ? 2 : 3), line(11, lineEnd ? 4 : 2, lineEnd ? 2 : 4)};
        fixPoints(m); m.constraints.push_back(constraint(1, SketchConstraintKind::Tangent, reversed ? 11 : 10, reversed ? 10 : 11));
        REQUIRE(SketchSolver::solve(m).solved());
        // At the other arc endpoint this vertical line is not tangent.
        m.entities[3].u = 2; m.entities[3].v = 1;
        const auto broken = SketchSolver::solve(m); REQUIRE(!broken.solved()); REQUIRE(broken.points.isEmpty());
    }
    for (bool aEnd : {false, true}) for (bool bEnd : {false, true}) {
        auto m = baseModel();
        m.entities = {point(1, 0, 0), point(2, 1, 0), point(3, 0, 1), point(4, 2, 0), point(5, 2, 1), arc(10, 1, aEnd ? 3 : 2, aEnd ? 2 : 3), arc(11, 4, bEnd ? 5 : 2, bEnd ? 2 : 5)};
        fixPoints(m); m.constraints.push_back(constraint(1, SketchConstraintKind::Tangent, 10, 11)); REQUIRE(SketchSolver::solve(m).solved());
        m.entities[3].u = 1; m.entities[3].v = 1; m.entities[4].u = 2; m.entities[4].v = 1;
        REQUIRE(!SketchSolver::solve(m).solved());
    }
    auto bad = baseModel(); bad.entities = {point(1, 0, 0), point(2, 1, 0), point(3, 0, 1), point(4, 2, 0), point(5, 3, 0), arc(10, 1, 2, 3), line(11, 4, 5), circle(12, 1, 1), circle(13, 4, 1), line(14, 2, 3)};
    for (const auto pair : {std::pair{10, 11}, std::pair{12, 11}, std::pair{12, 13}, std::pair{11, 14}, std::pair{10, 14}, std::pair{1, 11}}) {
        bad.constraints = {constraint(1, SketchConstraintKind::Tangent, pair.first, pair.second)};
        REQUIRE(SketchSolver::solve(bad).status == SolveStatus::InvalidModel);
        SketchModel preserved = bad; const auto before = preserved.toJson(); REQUIRE(!SketchModel::fromJson(before, &preserved)); REQUIRE(preserved.toJson() == before);
    }
    return 0;
}

static int testValidation() {
    auto m = baseModel(); m.entities = {point(1, 0, 0), point(2, 5, 0), line(10, 1, 2)}; m.constraints = {constraint(1, SketchConstraintKind::Distance, 1, 2, 5)};
    const auto valid = m.toJson();
    auto reject = [&](const QJsonObject &json) { SketchModel retained = m; QString error; const bool accepted = SketchModel::fromJson(json, &retained, &error); return !accepted && !error.isEmpty() && retained.toJson() == valid; };
    for (const auto &key : valid.keys()) { auto invalid = valid; invalid.remove(key); REQUIRE(reject(invalid)); invalid = valid; invalid[key] = QJsonValue::Null; REQUIRE(reject(invalid)); }
    auto invalid = valid; invalid[QStringLiteral("extra")] = true; REQUIRE(reject(invalid));
    for (const QString container : {QStringLiteral("plane"), QStringLiteral("entities"), QStringLiteral("constraints")}) {
        const bool plane = container == QStringLiteral("plane");
        const auto object = plane ? valid[container].toObject() : valid[container].toArray()[0].toObject();
        auto replace = [&](const QJsonObject &replacement) { auto json = valid; if (plane) json[container] = replacement; else { auto array = json[container].toArray(); array[0] = replacement; json[container] = array; } return json; };
        for (const auto &key : object.keys()) { auto malformed = object; malformed.remove(key); REQUIRE(reject(replace(malformed))); malformed = object; malformed[key] = QJsonValue::Null; REQUIRE(reject(replace(malformed))); }
        auto malformed = object; malformed[QStringLiteral("extra")] = 1; REQUIRE(reject(replace(malformed)));
    }
    for (auto badId : {QStringLiteral("+1"), QStringLiteral(" 1"), QStringLiteral("01"), QStringLiteral("-1"), QStringLiteral("1.0"), QStringLiteral("18446744073709551616"), QStringLiteral("0")}) {
        auto json = valid; auto array = json[QStringLiteral("entities")].toArray(); auto e = array[0].toObject(); e[QStringLiteral("id")] = badId; array[0] = e; json[QStringLiteral("entities")] = array; REQUIRE(reject(json));
    }
    for (int mutation = 0; mutation < 18; ++mutation) {
        auto bad = m;
        switch (mutation) {
        case 0: bad.schemaVersion = 2; break;
        case 1: bad.units = QStringLiteral("inches"); break;
        case 2: bad.entities[2].startPointId = 10; break;
        case 3: bad.entities[2].endPointId = 999; break;
        case 4: bad.entities[2].id = 1; break;
        case 5: bad.constraints.push_back(bad.constraints[0]); break;
        case 6: bad.constraints[0].second = 0; break;
        case 7: bad.constraints[0].value = -1; break;
        case 8: bad.constraints[0].first = 10; break;
        case 9: bad.entities[0].startPointId = 2; break;
        case 10: bad.entities[2].endPointId = 1; break;
        case 11: bad.plane.normalZ = 0; break;
        case 12: bad.entities[1].u = 1e10; break;
        case 13: bad.constraints[0].kind = SketchConstraintKind::Fixed; break;
        case 14: bad.constraints[0].second = bad.constraints[0].first; break;
        case 15: bad.entities[2].centerPointId = 1; break;
        case 16: bad.plane.id.clear(); break;
        case 17: bad.entities.clear(); break;
        }
        const auto before = bad.toJson(); const auto result = SketchSolver::solve(bad); REQUIRE(result.status == SolveStatus::InvalidModel); REQUIRE(result.points.isEmpty()); REQUIRE(bad.toJson() == before); REQUIRE(reject(before));
    }
    auto bad = m; bad.entities[0].kind = static_cast<SketchEntityKind>(999); REQUIRE(SketchSolver::solve(bad).status == SolveStatus::InvalidModel);
    bad = m; bad.constraints[0].kind = static_cast<SketchConstraintKind>(999); REQUIRE(SketchSolver::solve(bad).status == SolveStatus::InvalidModel);
    for (double value : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        bad = m; bad.entities[0].u = value; REQUIRE(SketchSolver::solve(bad).status == SolveStatus::InvalidModel);
        bad = m; bad.plane.normalZ = value; REQUIRE(SketchSolver::solve(bad).status == SolveStatus::InvalidModel);
        bad = m; bad.constraints[0].value = value; REQUIRE(SketchSolver::solve(bad).status == SolveStatus::InvalidModel);
    }
    auto arrays = valid; arrays[QStringLiteral("entities")] = QJsonArray{1}; REQUIRE(reject(arrays));
    arrays = valid; arrays[QStringLiteral("constraints")] = QJsonArray{false}; REQUIRE(reject(arrays));
    bad = m; bad.constraints[0].kind = SketchConstraintKind::Angle; bad.constraints[0].first = 10; bad.constraints[0].second = 11; bad.constraints[0].value = 181; REQUIRE(SketchSolver::solve(bad).status == SolveStatus::InvalidModel);
    return 0;
}

static int testDatumAndIds() {
    auto m = baseModel(); m.entities = {point(99999001, 2, 3), point(std::numeric_limits<quint64>::max(), 5, 3), line(1, 99999001, std::numeric_limits<quint64>::max())};
    m.constraints = {constraint(std::numeric_limits<quint64>::max(), SketchConstraintKind::Fixed, 99999001), constraint(2, SketchConstraintKind::Horizontal, 1), constraint(3, SketchConstraintKind::Distance, 99999001, std::numeric_limits<quint64>::max(), 3)};
    const auto xy = SketchSolver::solve(m); REQUIRE(xy.status == SolveStatus::Solved); REQUIRE(near(findPoint(xy, 99999001)->x, 2)); REQUIRE(near(findPoint(xy, 99999001)->y, 3));
    SketchModel restored; REQUIRE(SketchModel::fromJson(m.toJson(), &restored)); REQUIRE(restored.toJson() == m.toJson());
    m.plane.originX = 10; m.plane.originY = 20; m.plane.originZ = 30;
    for (const auto n : {std::array{0.0, 1.0, 0.0}, std::array{0.0, 0.0, -1.0}, std::array{1.0, 0.0, 0.0}, std::array{2.0, 3.0, 4.0}, std::array{1e300, 0.0, 0.0}, std::array{0.0, 0.0, 1e-300}}) {
        m.plane.normalX = n[0]; m.plane.normalY = n[1]; m.plane.normalZ = n[2];
        const auto result = SketchSolver::solve(m); REQUIRE(result.status == SolveStatus::Solved); const auto p = findPoint(result, 99999001); REQUIRE(p); REQUIRE(near(p->u, 2)); REQUIRE(near(p->v, 3));
        const double scale = std::max({std::abs(n[0]), std::abs(n[1]), std::abs(n[2])});
        REQUIRE(near((p->x - 10) * n[0] / scale + (p->y - 20) * n[1] / scale + (p->z - 30) * n[2] / scale, 0));
        REQUIRE(near(std::hypot(p->x - 10, p->y - 20, p->z - 30), std::sqrt(13.0)));
        if (n[1] == 1) { REQUIRE(near(p->x, 12)); REQUIRE(near(p->y, 20)); REQUIRE(near(p->z, 27)); }
        const auto again = SketchSolver::solve(m); REQUIRE(near(again.points[0].x, result.points[0].x));
    }
    // Circle IDs formerly collided with point parameter IDs after multiplication.
    m = baseModel(); m.entities = {point(1, 0, 0), point(100000001, 20, 0), circle(std::numeric_limits<quint64>::max(), 100000001, 2)}; fixPoints(m);
    m.constraints.push_back(constraint(std::numeric_limits<quint64>::max(), SketchConstraintKind::Radius, std::numeric_limits<quint64>::max(), 0, 5));
    const auto result = SketchSolver::solve(m); REQUIRE(result.status == SolveStatus::Solved); REQUIRE(near(result.radii[0].radius, 5));
    return 0;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    SketchModel rectangleWithHole; rectangleWithHole.id = QStringLiteral("rectangle-hole");
    rectangleWithHole.entities = {point(1, 0, 0), point(2, 80, 0), point(3, 80, 40), point(4, 0, 40), point(5, 40, 20), circle(6, 5, 8), line(10, 1, 2), line(11, 2, 3), line(12, 3, 4), line(13, 4, 1)};
    rectangleWithHole.constraints = {constraint(1, SketchConstraintKind::Fixed, 1), constraint(2, SketchConstraintKind::Distance, 1, 2, 80), constraint(3, SketchConstraintKind::Distance, 2, 3, 40), constraint(5, SketchConstraintKind::Fixed, 5), constraint(6, SketchConstraintKind::Horizontal, 10), constraint(7, SketchConstraintKind::Vertical, 11), constraint(8, SketchConstraintKind::Horizontal, 12), constraint(9, SketchConstraintKind::Vertical, 13), constraint(10, SketchConstraintKind::Radius, 6, 0, 8)};
    const auto fullyConstrained = SketchSolver::solve(rectangleWithHole); REQUIRE(fullyConstrained.status == SolveStatus::Solved); REQUIRE(fullyConstrained.remainingDof == 0); REQUIRE(fullyConstrained.conflictingConstraints.isEmpty());
    const auto json = rectangleWithHole.toJson(); SketchModel restored; QString parseError; REQUIRE(SketchModel::fromJson(json, &restored, &parseError)); REQUIRE(restored.toJson() == json);

    SketchModel editable; editable.id = QStringLiteral("editable-dimension"); editable.entities = {point(1, 0, 0), point(2, 30, 0), line(10, 1, 2)}; editable.constraints = {constraint(1, SketchConstraintKind::Fixed, 1), constraint(2, SketchConstraintKind::Horizontal, 10), constraint(3, SketchConstraintKind::Distance, 1, 2, 30)};
    auto first = SketchSolver::solve(editable); REQUIRE(first.status == SolveStatus::Solved); REQUIRE(first.remainingDof == 0); REQUIRE(std::abs(findPoint(first, 2)->u - 30.0) < 1e-6);
    editable.constraints[2].value = 57.25; auto changed = SketchSolver::solve(editable); REQUIRE(changed.status == SolveStatus::Solved); REQUIRE(changed.remainingDof == 0); REQUIRE(std::abs(findPoint(changed, 2)->u - 57.25) < 1e-6);

    SketchModel under = editable; under.id = QStringLiteral("under"); under.constraints.removeLast(); const auto underResult = SketchSolver::solve(under); REQUIRE(underResult.status == SolveStatus::UnderConstrained); REQUIRE(underResult.remainingDof > 0);
    SketchModel over = editable; over.id = QStringLiteral("over"); over.constraints.push_back(constraint(4, SketchConstraintKind::Distance, 1, 2, 20)); const auto overResult = SketchSolver::solve(over); REQUIRE(overResult.status == SolveStatus::OverConstrained); REQUIRE(!overResult.conflictingConstraints.isEmpty());
    REQUIRE(fullyConstrained.points.size() == 5); REQUIRE(fullyConstrained.radii.size() == 1);
    REQUIRE(testConstraintKinds() == 0);
    REQUIRE(testTangency() == 0);
    REQUIRE(testValidation() == 0);
    REQUIRE(testDatumAndIds() == 0);
    auto rectangleUnder = rectangleWithHole; rectangleUnder.constraints.removeLast();
    const auto ru = SketchSolver::solve(rectangleUnder); REQUIRE(ru.status == SolveStatus::UnderConstrained); REQUIRE(ru.remainingDof == 1);
    auto rectangleOver = rectangleWithHole; rectangleOver.constraints.push_back(constraint(99, SketchConstraintKind::Distance, 1, 2, 99));
    const auto overBefore = rectangleOver.toJson(); const auto ro = SketchSolver::solve(rectangleOver);
    REQUIRE(ro.status == SolveStatus::OverConstrained); REQUIRE(!ro.conflictingConstraints.isEmpty()); REQUIRE(ro.points.isEmpty()); REQUIRE(ro.radii.isEmpty()); REQUIRE(rectangleOver.toJson() == overBefore);
    std::printf("PASS: %d assertions, real libslvs constraint/JSON/datum/handle/status coverage\n", assertions);
    return 0;
}
