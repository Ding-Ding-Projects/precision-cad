#include "sketch_model.h"
#include "sketch_profiles.h"
#include <QCoreApplication>
#include <cstdio>
#include <cmath>

using namespace precision::sketch;
static int assertions=0;
#define REQUIRE(x) do { ++assertions; if(!(x)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while(false)
static SketchEntity point(SketchEntityId id,double u,double v) { SketchEntity e; e.id=id;e.kind=SketchEntityKind::Point;e.u=u;e.v=v;return e; }
static SketchEntity line(SketchEntityId id,SketchEntityId a,SketchEntityId b) { SketchEntity e; e.id=id;e.kind=SketchEntityKind::Line;e.startPointId=a;e.endPointId=b;return e; }
static SketchEntity circle(SketchEntityId id,SketchEntityId c,double r) { SketchEntity e; e.id=id;e.kind=SketchEntityKind::Circle;e.centerPointId=c;e.radius=r;return e; }
static SketchConstraint c(SketchConstraintId id,SketchConstraintKind k,SketchEntityId a,SketchEntityId b=0,double v=0) { return {id,k,{},a,b,v}; }
static SketchModel rectangleHole() { SketchModel m; m.id="rectangle-hole"; m.entities={point(1,0,0),point(2,80,0),point(3,80,40),point(4,0,40),point(5,40,20),circle(6,5,8),line(10,1,2),line(11,2,3),line(12,3,4),line(13,4,1)}; m.constraints={c(1,SketchConstraintKind::Fixed,1),c(2,SketchConstraintKind::Distance,1,2,80),c(3,SketchConstraintKind::Distance,2,3,40),c(4,SketchConstraintKind::Fixed,5),c(5,SketchConstraintKind::Horizontal,10),c(6,SketchConstraintKind::Vertical,11),c(7,SketchConstraintKind::Horizontal,12),c(8,SketchConstraintKind::Vertical,13),c(9,SketchConstraintKind::Radius,6,0,8)}; return m; }
static bool has(const SketchProfileResult&r,ProfileDiagnosticKind k) { for(const auto&d:r.diagnostics) if(d.kind==k) return true; return false; }
int main(int argc,char**argv) { QCoreApplication app(argc,argv);
  auto model=rectangleHole(); const auto solved=solveAndExtractProfiles(model); REQUIRE(solved.solve.status==SolveStatus::Solved); const auto first=solved.profiles; REQUIRE(first.valid()); REQUIRE(first.loops.size()==2); REQUIRE(first.regions.size()==1); REQUIRE(first.regions[0].holeLoopIds.size()==1); REQUIRE(first.loops[0].segments[0].sourceEntityId!=0); REQUIRE(first.loops[0].segments[0].kind==ProfileCurveKind::Circle || first.loops[0].segments[0].kind==ProfileCurveKind::Line);
  const auto ids=first.loops; model.constraints[1].value=100; const auto changed=solveAndExtractProfiles(model); REQUIRE(changed.solve.status==SolveStatus::Solved); const auto second=changed.profiles; REQUIRE(second.valid()); REQUIRE(second.loops.size()==ids.size()); REQUIRE(second.loops[0].stableId==ids[0].stableId); REQUIRE(second.loops[1].stableId==ids[1].stableId);
  auto open=rectangleHole(); open.entities.removeLast(); open.constraints.removeAt(7); auto openResult=solveAndExtractProfiles(open).profiles; REQUIRE(!openResult.valid()); REQUIRE(has(openResult,ProfileDiagnosticKind::Dangling));
  auto branch=rectangleHole(); branch.entities.push_back(point(7,0,20)); branch.entities.push_back(line(14,4,7)); branch.constraints.push_back(c(20,SketchConstraintKind::Fixed,7)); auto branchResult=solveAndExtractProfiles(branch).profiles; REQUIRE(!branchResult.valid()); REQUIRE(has(branchResult,ProfileDiagnosticKind::Branched));
  auto crossing=rectangleHole(); crossing.entities.push_back(line(14,1,3)); crossing.entities.push_back(line(15,2,4)); auto crossResult=solveAndExtractProfiles(crossing).profiles; REQUIRE(!crossResult.valid()); REQUIRE(has(crossResult,ProfileDiagnosticKind::Branched));
  auto touching=rectangleHole(); touching.entities[5].radius=20; touching.constraints.back().value=20; auto touchResult=solveAndExtractProfiles(touching).profiles; REQUIRE(!touchResult.valid()); REQUIRE(has(touchResult,ProfileDiagnosticKind::Touching));
  auto island=rectangleHole(); island.entities.push_back(point(7,40,20)); island.entities.push_back(circle(20,7,3)); island.constraints.push_back(c(20,SketchConstraintKind::Fixed,7)); island.constraints.push_back(c(21,SketchConstraintKind::Radius,20,0,3)); const auto islandResult=solveAndExtractProfiles(island); REQUIRE(islandResult.solve.status==SolveStatus::Solved); REQUIRE(islandResult.profiles.valid()); REQUIRE(islandResult.profiles.regions.size()==2); REQUIRE(islandResult.profiles.regions[0].holeLoopIds.size()+islandResult.profiles.regions[1].holeLoopIds.size()==1);
  SketchModel bow; bow.id="bow"; bow.entities={point(1,0,0),point(2,20,20),point(3,0,20),point(4,20,0),line(10,1,2),line(11,2,3),line(12,3,4),line(13,4,1)}; bow.constraints={c(1,SketchConstraintKind::Fixed,1),c(2,SketchConstraintKind::Fixed,2),c(3,SketchConstraintKind::Fixed,3),c(4,SketchConstraintKind::Fixed,4)}; const auto bowResult=solveAndExtractProfiles(bow).profiles; REQUIRE(!bowResult.valid()); REQUIRE(has(bowResult,ProfileDiagnosticKind::SelfIntersecting)); REQUIRE(bowResult.loops.isEmpty());
  std::printf("PASS: %d assertions, real solver profile loops, regions, provenance, and topology rejection\n",assertions); return 0;
}
