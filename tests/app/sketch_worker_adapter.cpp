// Test-only dispatch bridge. The production parser and both actual numerical
// implementations are shared unchanged; no synthetic geometry is supplied.
#include "GeometryWorker.h"
#include "SketchOperations.h"
#include <QJsonArray>
namespace precision::geometry {
QJsonObject dispatchRequest(const QJsonObject &request) {
    auto reply=request.value("protocolVersion")==QJsonValue(2) ? executeSketchRequest(request) : executeRequest(request);
#ifdef PRECISION_FORGE_SKETCH_REPLY
    // Deliberately hostile transport fixture, never used as numerical proof.
    if(request.value("operation")==QJsonValue("sketch") && reply.value("ok")==QJsonValue(true)) {
        auto result=reply.value("result").toObject(); auto profiles=result.value("profiles").toObject();
        auto loops=profiles.value("loops").toArray(), regions=profiles.value("regions").toArray();
        auto loop=loops[0].toObject(); auto segments=loop.value("segments").toArray(); auto segment=segments[0].toObject();
        const int mode=request.value("parameters").toObject().value("model").toObject().value("constraints").toArray()[1].toObject().value("value").toInt();
        if(mode==81) segment.insert("sourceEntityId","999");
        if(mode==82) segment.insert("kind","circle");
        if(mode==83) segment.insert("startPointId","5");
        if(mode==84) segment.insert("startU",123.0);
        segments[0]=segment; loop.insert("segments",segments);
        if(mode==85) { loop.insert("stableId","loop:999"); auto region=regions[0].toObject(); region.insert("outerLoopId","loop:999"); region.insert("stableId","region:loop:999"); regions[0]=region; }
        if(mode==86) { auto region=regions[0].toObject(); region.insert("stableId","synthetic-region"); regions[0]=region; }
        if(mode==87) { segments.removeLast(); loop.insert("segments",segments); }
        loops[0]=loop; profiles.insert("loops",loops); profiles.insert("regions",regions); result.insert("profiles",profiles);
        QJsonArray preview; for(const auto &l:loops) for(const auto &s:l.toObject().value("segments").toArray()) preview.append(s);
        result.insert("preview",QJsonObject{{"segments",preview}});
        if(mode==88) { auto solve=result.value("solve").toObject(); auto points=solve.value("points").toArray(); auto p=points[0].toObject(); p.insert("id","999"); points[0]=p; solve.insert("points",points); result.insert("solve",solve); }
        if(mode==89) { auto solve=result.value("solve").toObject(); auto radii=solve.value("radii").toArray(); auto r=radii[0].toObject(); r.insert("radius",9); radii[0]=r; solve.insert("radii",radii); result.insert("solve",solve); }
        reply.insert("result",result);
    }
#endif
    return reply;
}
}
#define executeRequest dispatchRequest
#include "../../src/geometry/main.cpp"
#undef executeRequest
