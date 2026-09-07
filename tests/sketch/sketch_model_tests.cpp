#include "sketch_model.h"

#include <QCoreApplication>
#include <cstdio>
#include <cmath>

using namespace precision::sketch;
#define REQUIRE(condition) do { if (!(condition)) { std::fprintf(stderr, "failed: %s at %d\n", #condition, __LINE__); return 1; } } while (false)

static SketchEntity point(SketchEntityId id, double u, double v) { return {id, SketchEntityKind::Point, QStringLiteral("point-%1").arg(id), u, v}; }
static SketchEntity line(SketchEntityId id, SketchEntityId start, SketchEntityId end) { SketchEntity item; item.id = id; item.kind = SketchEntityKind::Line; item.startPointId = start; item.endPointId = end; return item; }
static SketchEntity circle(SketchEntityId id, SketchEntityId center, double radius) { SketchEntity item; item.id = id; item.kind = SketchEntityKind::Circle; item.centerPointId = center; item.radius = radius; return item; }
static SketchConstraint constraint(SketchConstraintId id, SketchConstraintKind kind, SketchEntityId first, SketchEntityId second = 0, double value = 0.0) { return {id, kind, QStringLiteral("constraint-%1").arg(id), first, second, value}; }
static const SolvedPoint *findPoint(const SketchSolveResult &result, SketchEntityId id) { for (const auto &point : result.points) if (point.id == id) return &point; return nullptr; }

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    SketchModel rectangleWithHole; rectangleWithHole.id = QStringLiteral("rectangle-hole");
    rectangleWithHole.entities = {point(1, 0, 0), point(2, 80, 0), point(3, 80, 40), point(4, 0, 40), point(5, 40, 20), circle(6, 5, 8), line(10, 1, 2), line(11, 2, 3), line(12, 3, 4), line(13, 4, 1)};
    rectangleWithHole.constraints = {constraint(1, SketchConstraintKind::Fixed, 1), constraint(2, SketchConstraintKind::Fixed, 2), constraint(3, SketchConstraintKind::Fixed, 3), constraint(4, SketchConstraintKind::Fixed, 4), constraint(5, SketchConstraintKind::Fixed, 5), constraint(6, SketchConstraintKind::Horizontal, 10), constraint(7, SketchConstraintKind::Vertical, 11), constraint(8, SketchConstraintKind::Horizontal, 12), constraint(9, SketchConstraintKind::Vertical, 13), constraint(10, SketchConstraintKind::Radius, 6, 0, 8)};
    const auto fullyConstrained = SketchSolver::solve(rectangleWithHole); REQUIRE(fullyConstrained.status == SolveStatus::Solved); REQUIRE(fullyConstrained.remainingDof == 0); REQUIRE(fullyConstrained.conflictingConstraints.isEmpty());
    const auto json = rectangleWithHole.toJson(); SketchModel restored; QString parseError; REQUIRE(SketchModel::fromJson(json, &restored, &parseError)); REQUIRE(restored.toJson() == json);

    SketchModel editable; editable.id = QStringLiteral("editable-dimension"); editable.entities = {point(1, 0, 0), point(2, 30, 0), line(10, 1, 2)}; editable.constraints = {constraint(1, SketchConstraintKind::Fixed, 1), constraint(2, SketchConstraintKind::Horizontal, 10), constraint(3, SketchConstraintKind::Distance, 1, 2, 30)};
    auto first = SketchSolver::solve(editable); REQUIRE(first.status == SolveStatus::Solved); REQUIRE(first.remainingDof == 0); REQUIRE(std::abs(findPoint(first, 2)->u - 30.0) < 1e-6);
    editable.constraints[2].value = 57.25; auto changed = SketchSolver::solve(editable); REQUIRE(changed.status == SolveStatus::Solved); REQUIRE(changed.remainingDof == 0); REQUIRE(std::abs(findPoint(changed, 2)->u - 57.25) < 1e-6);

    SketchModel under = editable; under.id = QStringLiteral("under"); under.constraints.removeLast(); const auto underResult = SketchSolver::solve(under); REQUIRE(underResult.status == SolveStatus::UnderConstrained); REQUIRE(underResult.remainingDof > 0);
    SketchModel over = editable; over.id = QStringLiteral("over"); over.constraints.push_back(constraint(4, SketchConstraintKind::Distance, 1, 2, 20)); const auto overResult = SketchSolver::solve(over); REQUIRE(overResult.status == SolveStatus::OverConstrained); REQUIRE(!overResult.conflictingConstraints.isEmpty());
    REQUIRE(fullyConstrained.points.size() == 5); REQUIRE(fullyConstrained.radii.size() == 1); return 0;
}
