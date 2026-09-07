#include "sketch_profiles.h"

#include <QHash>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

namespace precision::sketch {
namespace {
constexpr int kMaxProfileEntities = 4096;

struct Point { double x = 0.0, y = 0.0; };
struct Edge { SketchProfileSegment segment; quint64 a = 0, b = 0; };
struct Tolerance { double length = 0.0; };
bool finite(double v) { return std::isfinite(v) && std::abs(v) <= 1e9; }
double cross(Point a, Point b, Point c) { return (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x); }
double distance(Point a, Point b) { return std::hypot(a.x-b.x, a.y-b.y); }
double crossTolerance(Point a, Point b, Point c, Tolerance tolerance) { return tolerance.length * (1.0 + distance(a,b) + distance(a,c)); }
bool onSegment(Point a, Point b, Point p, Tolerance tolerance) { return std::abs(cross(a,b,p)) <= crossTolerance(a,b,p,tolerance) && p.x >= std::min(a.x,b.x)-tolerance.length && p.x <= std::max(a.x,b.x)+tolerance.length && p.y >= std::min(a.y,b.y)-tolerance.length && p.y <= std::max(a.y,b.y)+tolerance.length; }
int sign(double value, double tolerance) { return value > tolerance ? 1 : value < -tolerance ? -1 : 0; }
bool segmentsTouch(Point a, Point b, Point c, Point d, Tolerance tolerance) {
    const int abC=sign(cross(a,b,c),crossTolerance(a,b,c,tolerance)), abD=sign(cross(a,b,d),crossTolerance(a,b,d,tolerance)), cdA=sign(cross(c,d,a),crossTolerance(c,d,a,tolerance)), cdB=sign(cross(c,d,b),crossTolerance(c,d,b,tolerance));
    if (abC*abD < 0 && cdA*cdB < 0) return true;
    return (abC==0 && onSegment(a,b,c,tolerance)) || (abD==0 && onSegment(a,b,d,tolerance)) || (cdA==0 && onSegment(c,d,a,tolerance)) || (cdB==0 && onSegment(c,d,b,tolerance));
}
bool lineCircleTouches(Point a, Point b, Point c, double radius, Tolerance tolerance) {
    const double dx=b.x-a.x, dy=b.y-a.y, length2=dx*dx+dy*dy;
    if (length2 <= tolerance.length*tolerance.length) return std::abs(distance(a,c)-radius) <= tolerance.length;
    const double fx=a.x-c.x, fy=a.y-c.y;
    const double coefficientB=2.0*(fx*dx+fy*dy), coefficientC=fx*fx+fy*fy-radius*radius;
    const double discriminant=coefficientB*coefficientB-4.0*length2*coefficientC;
    const double discriminantTolerance=tolerance.length*(1.0+std::abs(coefficientB*coefficientB)+std::abs(4.0*length2*coefficientC));
    if (discriminant < -discriminantTolerance) return false;
    const double root=std::sqrt(std::max(0.0,discriminant));
    const double parameterTolerance=tolerance.length/std::max(1.0,std::sqrt(length2));
    const double t1=(-coefficientB-root)/(2.0*length2), t2=(-coefficientB+root)/(2.0*length2);
    return (t1 >= -parameterTolerance && t1 <= 1.0+parameterTolerance) || (t2 >= -parameterTolerance && t2 <= 1.0+parameterTolerance);
}
QString stableId(const QVector<SketchEntityId> &ids, QStringView prefix) {
    QVector<SketchEntityId> sorted=ids; std::sort(sorted.begin(), sorted.end());
    QStringList parts; for (const auto id: sorted) parts.append(QString::number(id));
    return QString(prefix) + QLatin1Char(':') + parts.join(QLatin1Char(','));
}
void add(SketchProfileResult &out, ProfileDiagnosticKind kind, QString message, QVector<SketchEntityId> ids={}) { out.diagnostics.push_back({kind,std::move(message),std::move(ids)}); }
bool pointInPolygon(const QVector<Point> &polygon, Point p, Tolerance tolerance) {
    bool inside=false;
    for (int i=0,j=polygon.size()-1;i<polygon.size();j=i++) {
        if (onSegment(polygon[j],polygon[i],p,tolerance)) return false;
        const auto &a=polygon[i],&b=polygon[j];
        if ((a.y>p.y)!=(b.y>p.y) && p.x < (b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x) inside=!inside;
    }
    return inside;
}
Point interiorPoint(const SketchProfileLoop &loop, Tolerance tolerance) {
    if (loop.segments.size()==1 && loop.segments[0].kind==ProfileCurveKind::Circle) return {loop.segments[0].centerU + loop.segments[0].radius * 0.5, loop.segments[0].centerV};
    const auto &s=loop.segments.front(); const Point a{s.startU,s.startV},b{s.endU,s.endV};
    const double dx=b.x-a.x,dy=b.y-a.y, length=std::hypot(dx,dy);
    const double side=loop.clockwise ? -1.0 : 1.0;
    return {(a.x+b.x)/2-side*dy/length*tolerance.length*8, (a.y+b.y)/2+side*dx/length*tolerance.length*8};
}
bool contains(const SketchProfileLoop &loop, Point p, Tolerance tolerance) {
    if (loop.segments.size()==1 && loop.segments[0].kind==ProfileCurveKind::Circle) return distance({loop.segments[0].centerU,loop.segments[0].centerV},p) < loop.segments[0].radius-tolerance.length;
    QVector<Point> polygon; polygon.reserve(loop.segments.size()); for (const auto &s:loop.segments) polygon.push_back({s.startU,s.startV});
    return pointInPolygon(polygon,p,tolerance);
}
Tolerance toleranceFor(const QHash<SketchEntityId, Point> &points, const QHash<SketchEntityId, double> &radii) {
    double coordinate=1.0, minX=std::numeric_limits<double>::max(), minY=minX, maxX=-minX, maxY=-minX;
    for (const auto point:points) { coordinate=std::max({coordinate,std::abs(point.x),std::abs(point.y)}); minX=std::min(minX,point.x); minY=std::min(minY,point.y); maxX=std::max(maxX,point.x); maxY=std::max(maxY,point.y); }
    double extent=std::max(maxX-minX,maxY-minY); for (const auto radius:radii) extent=std::max(extent,2.0*radius);
    return {64.0*std::numeric_limits<double>::epsilon()*std::max({1.0,coordinate,extent})};
}
double signedArea(const SketchProfileLoop &loop) {
    if (loop.segments.isEmpty()) return 0.0;
    const Point origin{loop.segments.front().startU,loop.segments.front().startV}; double area=0;
    for(const auto &segment:loop.segments) { const Point a{segment.startU-origin.x,segment.startV-origin.y}, b{segment.endU-origin.x,segment.endV-origin.y}; area+=a.x*b.y-b.x*a.y; }
    return area;
}
double areaTolerance(const SketchProfileLoop &loop, Tolerance tolerance) { double perimeter=0; for(const auto &segment:loop.segments) perimeter+=distance({segment.startU,segment.startV},{segment.endU,segment.endV}); return tolerance.length*std::max(1.0,perimeter); }
void canonicalize(SketchProfileLoop &loop, bool clockwise) {
    if (loop.segments.size()==1) { loop.clockwise=clockwise; return; }
    if ((signedArea(loop)<0.0)!=clockwise) { std::reverse(loop.segments.begin(),loop.segments.end()); for(auto &segment:loop.segments) { std::swap(segment.startPointId,segment.endPointId); std::swap(segment.startU,segment.endU); std::swap(segment.startV,segment.endV); } }
    auto first=std::min_element(loop.segments.begin(),loop.segments.end(),[](const auto &left,const auto &right) { return left.sourceEntityId<right.sourceEntityId; });
    std::rotate(loop.segments.begin(),first,loop.segments.end()); loop.clockwise=clockwise;
}
}

QString profileDiagnosticKindName(ProfileDiagnosticKind kind) {
    switch(kind) { case ProfileDiagnosticKind::InvalidInput:return QStringLiteral("invalid-input"); case ProfileDiagnosticKind::MissingSolvedAssociation:return QStringLiteral("missing-solved-association"); case ProfileDiagnosticKind::UnsupportedCurve:return QStringLiteral("unsupported-curve"); case ProfileDiagnosticKind::Open:return QStringLiteral("open"); case ProfileDiagnosticKind::Dangling:return QStringLiteral("dangling"); case ProfileDiagnosticKind::Branched:return QStringLiteral("branched"); case ProfileDiagnosticKind::SelfIntersecting:return QStringLiteral("self-intersecting"); case ProfileDiagnosticKind::Touching:return QStringLiteral("touching"); case ProfileDiagnosticKind::AmbiguousNesting:return QStringLiteral("ambiguous-nesting"); } return {};
}

namespace {
SketchProfileResult extractSolvedProfiles(const SketchModel &model, const SketchSolveResult &solved) {
    SketchProfileResult out;
    if (!solved.solved() || model.entities.size()>kMaxProfileEntities || solved.points.size()>kMaxProfileEntities) { add(out,ProfileDiagnosticKind::InvalidInput,QStringLiteral("Profile extraction requires a bounded successful solver result.")); return out; }
    QHash<SketchEntityId, Point> points; QSet<SketchEntityId> modelPoints;
    for (const auto &entity:model.entities) if (entity.kind==SketchEntityKind::Point) modelPoints.insert(entity.id);
    for (const auto &point:solved.points) {
        if (!modelPoints.contains(point.id) || points.contains(point.id) || !finite(point.u) || !finite(point.v)) { add(out,ProfileDiagnosticKind::MissingSolvedAssociation,QStringLiteral("Solved points must map one-to-one to finite model point IDs."),{point.id}); return out; }
        points.insert(point.id,{point.u,point.v});
    }
    if (points.size()!=modelPoints.size()) { add(out,ProfileDiagnosticKind::MissingSolvedAssociation,QStringLiteral("Every model point requires one solved association.")); return out; }
    QHash<SketchEntityId, double> radii; QSet<SketchEntityId> modelCircles;
    for (const auto &entity:model.entities) if (entity.kind==SketchEntityKind::Circle) modelCircles.insert(entity.id);
    for (const auto &radius:solved.radii) { if (!modelCircles.contains(radius.id) || radii.contains(radius.id) || !finite(radius.radius) || radius.radius<=0.0) { add(out,ProfileDiagnosticKind::MissingSolvedAssociation,QStringLiteral("Solved radii must map one-to-one to positive circle IDs."),{radius.id}); return out; } radii.insert(radius.id,radius.radius); }
    if (radii.size()!=modelCircles.size()) { add(out,ProfileDiagnosticKind::MissingSolvedAssociation,QStringLiteral("Every circle requires one solved radius association.")); return out; }
    const Tolerance tolerance=toleranceFor(points,radii);

    QHash<SketchEntityId, SketchEntityId> parent; for (const auto id:modelPoints) parent[id]=id;
    const auto find=[&](SketchEntityId id) { auto root=id; while(parent[root]!=root) root=parent[root]; while(parent[id]!=id) { auto next=parent[id]; parent[id]=root; id=next; } return root; };
    const auto unite=[&](SketchEntityId a, SketchEntityId b) { a=find(a); b=find(b); if(a!=b) parent[b]=a; };
    for (const auto &constraint:model.constraints) if (constraint.kind==SketchConstraintKind::Coincident && parent.contains(constraint.first) && parent.contains(constraint.second)) unite(constraint.first,constraint.second);

    QVector<Edge> edges;
    for (const auto &entity:model.entities) {
        if (entity.kind==SketchEntityKind::Point) continue;
        if (entity.kind==SketchEntityKind::Arc) { add(out,ProfileDiagnosticKind::UnsupportedCurve,QStringLiteral("Arc profile extraction is unavailable until its directed sweep contract is explicit."),{entity.id}); continue; }
        if (entity.kind==SketchEntityKind::Circle) {
            const auto c=points.value(entity.centerPointId); const double r=radii.value(entity.id);
            SketchProfileSegment s{ProfileCurveKind::Circle,entity.id,0,0,0,0,0,0,c.x,c.y,r};
            out.loops.push_back({stableId({entity.id},u"loop"),{s},false}); continue;
        }
        if (!points.contains(entity.startPointId)||!points.contains(entity.endPointId)) { add(out,ProfileDiagnosticKind::MissingSolvedAssociation,QStringLiteral("Line endpoint lacks a solved association."),{entity.id}); continue; }
        const auto a=points.value(entity.startPointId),b=points.value(entity.endPointId);
        edges.push_back({{ProfileCurveKind::Line,entity.id,entity.startPointId,entity.endPointId,a.x,a.y,b.x,b.y,0,0,0},find(entity.startPointId),find(entity.endPointId)});
    }
    if (!out.diagnostics.isEmpty()) { out.loops.clear(); out.regions.clear(); return out; }
    QHash<quint64,QVector<int>> incident; for(int i=0;i<edges.size();++i) { incident[edges[i].a].push_back(i); incident[edges[i].b].push_back(i); }
    for(auto it=incident.cbegin();it!=incident.cend();++it) if(it.value().size()!=2) add(out,it.value().size()<2?ProfileDiagnosticKind::Dangling:ProfileDiagnosticKind::Branched,QStringLiteral("Profile graph vertices must have degree two."));
    if(!out.diagnostics.isEmpty()) { out.loops.clear(); out.regions.clear(); return out; }
    QSet<int> seen;
    for(int seed=0;seed<edges.size();++seed) if(!seen.contains(seed)) {
        QVector<SketchProfileSegment> loop; QVector<SketchEntityId> ids; int current=seed; quint64 at=edges[current].a;
        while(!seen.contains(current)) { seen.insert(current); const auto &edge=edges[current]; const bool forward=edge.a==at; auto s=edge.segment; if(!forward) { std::swap(s.startPointId,s.endPointId); std::swap(s.startU,s.endU); std::swap(s.startV,s.endV); } loop.push_back(s); ids.push_back(s.sourceEntityId); at=forward?edge.b:edge.a; const auto &choices=incident[at]; current=choices[0]==current?choices[1]:choices[0]; }
        if(at!=edges[seed].a) { add(out,ProfileDiagnosticKind::Open,QStringLiteral("Profile edge walk did not return to its starting topological endpoint."),ids); continue; }
        SketchProfileLoop candidate{stableId(ids,u"loop"),loop,false}; const double twiceArea=signedArea(candidate);
        if(std::abs(twiceArea)<=areaTolerance(candidate,tolerance)) { add(out,ProfileDiagnosticKind::SelfIntersecting,QStringLiteral("A profile loop has zero signed area."),ids); continue; }
        candidate.clockwise=twiceArea<0; out.loops.push_back(std::move(candidate));
    }
    for(int l=0;l<out.loops.size();++l) for(int i=0;i<out.loops[l].segments.size();++i) for(int j=i+1;j<out.loops[l].segments.size();++j) {
        if(out.loops[l].segments.size()>2 && (j==i+1 || (i==0 && j==out.loops[l].segments.size()-1))) continue;
        const auto&a=out.loops[l].segments[i],&b=out.loops[l].segments[j];
        if(a.kind==ProfileCurveKind::Line&&b.kind==ProfileCurveKind::Line&&segmentsTouch({a.startU,a.startV},{a.endU,a.endV},{b.startU,b.startV},{b.endU,b.endV},tolerance)) add(out,ProfileDiagnosticKind::SelfIntersecting,QStringLiteral("A profile loop self-intersects."),{a.sourceEntityId,b.sourceEntityId});
    }
    for(int a=0;a<out.loops.size();++a) for(int b=a+1;b<out.loops.size();++b) for(const auto&sa:out.loops[a].segments) for(const auto&sb:out.loops[b].segments) {
        bool touch=false;
        if(sa.kind==ProfileCurveKind::Circle&&sb.kind==ProfileCurveKind::Circle) { const double d=distance({sa.centerU,sa.centerV},{sb.centerU,sb.centerV}); touch=d<=sa.radius+sb.radius+tolerance.length && d>=std::abs(sa.radius-sb.radius)-tolerance.length; }
        else if(sa.kind==ProfileCurveKind::Circle) touch=lineCircleTouches({sb.startU,sb.startV},{sb.endU,sb.endV},{sa.centerU,sa.centerV},sa.radius,tolerance);
        else if(sb.kind==ProfileCurveKind::Circle) touch=lineCircleTouches({sa.startU,sa.startV},{sa.endU,sa.endV},{sb.centerU,sb.centerV},sb.radius,tolerance);
        else touch=segmentsTouch({sa.startU,sa.startV},{sa.endU,sa.endV},{sb.startU,sb.startV},{sb.endU,sb.endV},tolerance);
        if(touch) add(out,ProfileDiagnosticKind::Touching,QStringLiteral("Separate profile loops touch or intersect."),{sa.sourceEntityId,sb.sourceEntityId});
    }
    if(!out.diagnostics.isEmpty()) { out.loops.clear(); return out; }
    QVector<int> depth(out.loops.size());
    for(int i=0;i<out.loops.size();++i) { const Point p=interiorPoint(out.loops[i],tolerance); for(int j=0;j<out.loops.size();++j) if(i!=j && contains(out.loops[j],p,tolerance)) ++depth[i]; }
    for(int i=0;i<out.loops.size();++i) if(depth[i]%2==0) out.regions.push_back({QStringLiteral("region:")+out.loops[i].stableId,out.loops[i].stableId,{}});
    for(int i=0;i<out.loops.size();++i) if(depth[i]%2==1) { int container=-1; for(int j=0;j<out.loops.size();++j) if(depth[j]==depth[i]-1 && contains(out.loops[j],interiorPoint(out.loops[i],tolerance),tolerance)) { if(container>=0) { add(out,ProfileDiagnosticKind::AmbiguousNesting,QStringLiteral("A hole has multiple immediate containing loops.")); break; } container=j; } if(container<0) add(out,ProfileDiagnosticKind::AmbiguousNesting,QStringLiteral("A hole has no immediate containing loop.")); else for(auto &region:out.regions) if(region.outerLoopId==out.loops[container].stableId) region.holeLoopIds.push_back(out.loops[i].stableId); }
    for(int i=0;i<out.loops.size();++i) canonicalize(out.loops[i],depth[i]%2==1);
    std::sort(out.loops.begin(),out.loops.end(),[](const auto &left,const auto &right) { return left.stableId<right.stableId; });
    for(auto &region:out.regions) std::sort(region.holeLoopIds.begin(),region.holeLoopIds.end());
    std::sort(out.regions.begin(),out.regions.end(),[](const auto &left,const auto &right) { return left.stableId<right.stableId; });
    if(!out.diagnostics.isEmpty()) { out.loops.clear(); out.regions.clear(); }
    return out;
}
} // namespace

SketchProfileSolveResult solveAndExtractProfiles(const SketchModel &model) {
    SketchProfileSolveResult out;
    out.solve = SketchSolver::solve(model);
    if (!out.solve.solved()) {
        add(out.profiles, ProfileDiagnosticKind::InvalidInput,
            QStringLiteral("Profile extraction requires a successful solver state; failed solves produce no profile."));
        return out;
    }
    out.profiles = extractSolvedProfiles(model, out.solve);
    return out;
}
} // namespace precision::sketch
