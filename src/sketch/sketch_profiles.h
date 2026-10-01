#pragma once

#include "sketch_model.h"

#include <QString>
#include <QVector>

namespace precision::sketch {

// The profile layer intentionally exposes sketch-space analytic records only.
// Geometry conversion owns all BRep and Open CASCADE representation later.
enum class ProfileCurveKind { Line, Circle, Arc };
enum class ProfileDiagnosticKind {
    InvalidInput, MissingSolvedAssociation, UnsupportedCurve, Open, Dangling,
    Branched, SelfIntersecting, Touching, AmbiguousNesting
};

struct ProfileDiagnostic {
    ProfileDiagnosticKind kind = ProfileDiagnosticKind::InvalidInput;
    QString message;
    QVector<SketchEntityId> sourceEntityIds;
};

struct SketchProfileSegment {
    ProfileCurveKind kind = ProfileCurveKind::Line;
    SketchEntityId sourceEntityId = 0;
    SketchEntityId startPointId = 0;
    SketchEntityId endPointId = 0;
    double startU = 0.0;
    double startV = 0.0;
    double endU = 0.0;
    double endV = 0.0;
    double centerU = 0.0;
    double centerV = 0.0;
    double radius = 0.0;
};

struct SketchProfileLoop {
    QString stableId;
    QVector<SketchProfileSegment> segments;
    bool clockwise = false;
};

struct SketchProfileRegion {
    QString stableId;
    QString outerLoopId;
    QVector<QString> holeLoopIds;
};

struct SketchProfileResult {
    QVector<SketchProfileLoop> loops;
    QVector<SketchProfileRegion> regions;
    QVector<ProfileDiagnostic> diagnostics;
    [[nodiscard]] bool valid() const noexcept { return diagnostics.isEmpty(); }
};

struct SketchProfileSolveResult {
    SketchSolveResult solve;
    SketchProfileResult profiles;
};

// Solves exactly this immutable model before extracting. The model/result pairing
// stays internal so stale or unrelated solved coordinates cannot form a profile.
[[nodiscard]] SketchProfileSolveResult solveAndExtractProfiles(const SketchModel &model);

[[nodiscard]] QString profileDiagnosticKindName(ProfileDiagnosticKind kind);

} // namespace precision::sketch
