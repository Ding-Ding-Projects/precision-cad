#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace precision::sketch {

using SketchEntityId = quint64;
using SketchConstraintId = quint64;

inline constexpr int kSketchSchemaVersion = 1;

struct DatumPlane {
    QString id = QStringLiteral("xy");
    double originX = 0.0;
    double originY = 0.0;
    double originZ = 0.0;
    double normalX = 0.0;
    double normalY = 0.0;
    double normalZ = 1.0;
};

enum class SketchEntityKind { Point, Line, Circle, Arc };

struct SketchEntity {
    SketchEntityId id = 0;
    SketchEntityKind kind = SketchEntityKind::Point;
    QString label;
    // Point stores (u, v). Line and arc refer to stable point IDs. Circle uses centerPointId and radius.
    double u = 0.0;
    double v = 0.0;
    double radius = 0.0;
    SketchEntityId startPointId = 0;
    SketchEntityId endPointId = 0;
    SketchEntityId centerPointId = 0;
};

enum class SketchConstraintKind {
    Coincident,
    Horizontal,
    Vertical,
    Parallel,
    Perpendicular,
    Tangent,
    Equal,
    Distance,
    Radius,
    Angle,
    Fixed
};

struct SketchConstraint {
    SketchConstraintId id = 0;
    SketchConstraintKind kind = SketchConstraintKind::Coincident;
    QString label;
    SketchEntityId first = 0;
    SketchEntityId second = 0;
    double value = 0.0;
};

struct SketchModel {
    int schemaVersion = kSketchSchemaVersion;
    QString id;
    QString units = QStringLiteral("mm");
    DatumPlane plane;
    QVector<SketchEntity> entities;
    QVector<SketchConstraint> constraints;

    [[nodiscard]] QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &json, SketchModel *out, QString *error = nullptr);
};

enum class SolveStatus { Solved, UnderConstrained, OverConstrained, DidNotConverge, InvalidModel };

struct SolvedPoint {
    SketchEntityId id = 0;
    double u = 0.0;
    double v = 0.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct SolvedRadius {
    SketchEntityId id = 0;
    double radius = 0.0;
};

struct SketchSolveResult {
    SolveStatus status = SolveStatus::InvalidModel;
    int remainingDof = 0;
    QVector<SketchConstraintId> conflictingConstraints;
    QVector<SolvedPoint> points;
    QVector<SolvedRadius> radii;
    QString diagnostic;
    [[nodiscard]] bool solved() const noexcept { return status == SolveStatus::Solved || status == SolveStatus::UnderConstrained; }
};

class SketchSolver final {
public:
    // Uses the canonical SolveSpace 3.2 libslvs C API with checked dense handles.
    // The immutable input retains persistent IDs; result records map back to those IDs.
    [[nodiscard]] static SketchSolveResult solve(const SketchModel &model);
};

[[nodiscard]] QString entityKindName(SketchEntityKind kind);
[[nodiscard]] QString constraintKindName(SketchConstraintKind kind);

} // namespace precision::sketch
