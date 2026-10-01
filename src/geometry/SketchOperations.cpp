#include "SketchOperations.h"
#include "GeometryWorker.h"
#include "sketch_profiles.h"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <TopoDS_Wire.hxx>
#include <Standard_Failure.hxx>
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace precision::geometry {
namespace {
using namespace precision::sketch;
bool fields(const QJsonObject &o,std::initializer_list<const char*> names) {
    if(o.size()!=names.size()) return false;
    for(auto n:names) if(!o.contains(QLatin1String(n))) return false;
    return true;
}
QString statusName(SolveStatus s) {
    switch(s) { case SolveStatus::Solved:return "solved"; case SolveStatus::UnderConstrained:return "underConstrained";
    case SolveStatus::OverConstrained:return "overConstrained"; case SolveStatus::DidNotConverge:return "didNotConverge"; default:return "invalidModel"; }
}
QJsonObject segmentRecord(const SketchProfileSegment &s) {
    return {{"kind",s.kind==ProfileCurveKind::Line?"line":"circle"},{"sourceEntityId",QString::number(s.sourceEntityId)},
        {"startPointId",QString::number(s.startPointId)},{"endPointId",QString::number(s.endPointId)},
        {"startU",s.startU},{"startV",s.startV},{"endU",s.endU},{"endV",s.endV},
        {"centerU",s.centerU},{"centerV",s.centerV},{"radius",s.radius}};
}
QJsonObject sketchRecord(const QJsonObject &modelJson,const QString &producer,const QJsonObject &request, SketchModel *modelOut=nullptr, SketchProfileResult *profilesOut=nullptr) {
    SketchModel model; QString error;
    if(!SketchModel::fromJson(modelJson,&model,&error)) throw std::runtime_error(error.toStdString());
    const auto solved=solveAndExtractProfiles(model);
    if(!solved.solve.solved()) {
        QStringList ids; for(auto id:solved.solve.conflictingConstraints) ids.append(QString::number(id));
        throw std::runtime_error(QString("Sketch %1: %2; conflicting constraint IDs: %3").arg(statusName(solved.solve.status),solved.solve.diagnostic,ids.join(", ")).toStdString());
    }
    if(!solved.profiles.valid() || solved.profiles.regions.isEmpty()) {
        QStringList diagnostics; for(const auto &d:solved.profiles.diagnostics) diagnostics.append(d.message);
        throw std::runtime_error(QString("Sketch profile is not pad-ready: %1").arg(diagnostics.join("; ")).toStdString());
    }
    QJsonArray loops,regions,segments,conflicts,points,radii;
    for(const auto &p:solved.solve.points) points.append(QJsonObject{{"id",QString::number(p.id)},{"u",p.u},{"v",p.v},{"x",p.x},{"y",p.y},{"z",p.z}});
    for(const auto &r:solved.solve.radii) radii.append(QJsonObject{{"id",QString::number(r.id)},{"radius",r.radius}});
    for(const auto &l:solved.profiles.loops) {
        QJsonArray entries; for(const auto &s:l.segments) { if(s.kind==ProfileCurveKind::Arc) throw std::runtime_error("Arc profiles are not supported by this pad operation."); entries.append(segmentRecord(s)); segments.append(segmentRecord(s)); }
        loops.append(QJsonObject{{"stableId",l.stableId},{"clockwise",l.clockwise},{"segments",entries}});
    }
    for(const auto &r:solved.profiles.regions) { QJsonArray holes; for(const auto &h:r.holeLoopIds) holes.append(h); regions.append(QJsonObject{{"stableId",r.stableId},{"outerLoopId",r.outerLoopId},{"holeLoopIds",holes}}); }
    for(auto id:solved.solve.conflictingConstraints) conflicts.append(QString::number(id));
    if(modelOut) *modelOut=model; if(profilesOut) *profilesOut=solved.profiles;
    return {{"kind","sketch"},{"producerFeatureId",producer},{"documentId",request.value("documentId")},{"revision",request.value("revision")},
        {"model",modelJson},{"solve",QJsonObject{{"status",statusName(solved.solve.status)},{"dof",solved.solve.remainingDof},{"conflicts",conflicts},{"points",points},{"radii",radii}}},
        {"profiles",QJsonObject{{"loops",loops},{"regions",regions}}},{"preview",QJsonObject{{"segments",segments}}}};
}
// Match the solver's deterministic datum frame: projected +X, or +Y near parallel.
struct Frame {
    gp_Pnt origin; gp_Dir normal; gp_Vec u,v;
    static gp_Dir normalized(const DatumPlane &p) {
        const double scale=std::max({std::abs(p.normalX),std::abs(p.normalY),std::abs(p.normalZ)});
        return gp_Dir(p.normalX/scale,p.normalY/scale,p.normalZ/scale);
    }
    explicit Frame(const DatumPlane &p):origin(p.originX,p.originY,p.originZ),normal(normalized(p)) {
        const gp_Vec n(normal), axis=std::abs(normal.X())>0.9?gp_Vec(0,1,0):gp_Vec(1,0,0);
        u=axis-n.Multiplied(axis.Dot(n)); u.Normalize(); v=n.Crossed(u);
    }
    gp_Pnt point(double x,double y) const { return origin.Translated(u.Multiplied(x)+v.Multiplied(y)); }
};
TopoDS_Wire wire(const SketchProfileLoop &loop,const Frame &frame) {
    BRepBuilderAPI_MakeWire builder;
    for(const auto &s:loop.segments) {
        if(s.kind==ProfileCurveKind::Line) builder.Add(BRepBuilderAPI_MakeEdge(frame.point(s.startU,s.startV),frame.point(s.endU,s.endV)).Edge());
        else if(s.kind==ProfileCurveKind::Circle) {
            auto edge=BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(frame.point(s.centerU,s.centerV),frame.normal,gp_Dir(frame.u)),s.radius)).Edge();
            if(loop.clockwise) edge.Reverse(); builder.Add(edge);
        } else throw std::runtime_error("unsupported_profile_curve");
    }
    if(!builder.IsDone()) throw std::runtime_error("profile_wire_failed");
    return builder.Wire();
}
}
QJsonObject executeSketchRequest(const QJsonObject &request) {
    QJsonObject identity{{"protocolVersion",2},{"operationId",request.value("operationId")},{"documentId",request.value("documentId")},{"revision",request.value("revision")}};
    auto fail=[&](const QString &code,const QString &message) { auto out=identity; out.insert("ok",false); out.insert("error",QJsonObject{{"code",code},{"message",message}}); return out; };
    const double revision=request.value("revision").toDouble(-1);
    if(QJsonDocument(request).toJson(QJsonDocument::Compact).size()>4*1024*1024) return fail("request_too_large","Request exceeds the worker limit.");
    if(!fields(request,{"protocolVersion","operationId","documentId","revision","operation","parameters"}) || request.value("protocolVersion")!=QJsonValue(2) || !request.value("revision").isDouble() || !std::isfinite(revision) || revision<0 || revision>9007199254740991.0 || std::floor(revision)!=revision || !request.value("parameters").isObject()) return fail("invalid_envelope","Unsupported or incomplete sketch request.");
    for(const char *key:{"documentId","operationId"}) if(!request.value(key).isString() || request.value(key).toString().isEmpty() || request.value(key).toString().size()>128) return fail("invalid_envelope","Invalid request identity.");
    try {
        const auto p=request.value("parameters").toObject(); const QString operation=request.value("operation").toString();
        QJsonObject result;
        if(operation=="sketch") {
            if(!fields(p,{"model","producerFeatureId"}) || !p.value("model").isObject() || !p.value("producerFeatureId").isString() || p.value("producerFeatureId").toString().isEmpty() || p.value("producerFeatureId").toString().size()>128) throw std::runtime_error("invalid_sketch_parameters");
            result=sketchRecord(p.value("model").toObject(),p.value("producerFeatureId").toString(),request);
        } else if(operation=="pad") {
            const double length=p.value("length").toDouble(-1); const QString regionId=p.value("regionId").toString(), sketchId=p.value("sketchId").toString();
            if(!fields(p,{"length","regionId","sketchId","sketch"}) || !p.value("length").isDouble() || !std::isfinite(length) || length<=0 || length>1e9 || regionId.isEmpty() || regionId.size()>1024 || sketchId.isEmpty() || sketchId.size()>128 || !p.value("sketch").isObject()) throw std::runtime_error("invalid_pad_parameters");
            const auto source=p.value("sketch").toObject(); SketchModel model; SketchProfileResult profiles;
            const auto verified=sketchRecord(source.value("model").toObject(),sketchId,request,&model,&profiles);
            // Re-solving checks the full analytic result and its producer/document/revision binding.
            // This is consistency validation, not authentication of external document history.
            if(source!=verified) throw std::runtime_error("stale_or_mismatched_sketch_result");
            const SketchProfileRegion *region=nullptr; for(const auto &r:profiles.regions) if(r.stableId==regionId) { if(region) throw std::runtime_error("ambiguous_region"); region=&r; }
            if(!region) throw std::runtime_error("unknown_region");
            const auto loop=[&](const QString &id)->const SketchProfileLoop& { for(const auto &l:profiles.loops) if(l.stableId==id) return l; throw std::runtime_error("unknown_loop"); };
            const Frame frame(model.plane);
            BRepBuilderAPI_MakeFace face(gp_Pln(frame.origin,frame.normal),wire(loop(region->outerLoopId),frame),true);
            for(const auto &hole:region->holeLoopIds) face.Add(wire(loop(hole),frame));
            if(!face.IsDone()) throw std::runtime_error("profile_face_failed");
            const auto shape=BRepPrimAPI_MakePrism(face.Face(),gp_Vec(frame.normal).Multiplied(length)).Shape();
            if(shape.IsNull() || !BRepCheck_Analyzer(shape,true).IsValid()) throw std::runtime_error("pad_geometry_invalid");
            std::ostringstream stream; BRepTools::Write(shape,stream); const auto bytes=stream.str();
            if(bytes.empty() || bytes.size()>2*1024*1024) throw std::runtime_error("brep_too_large");
            auto native=request; native.insert("protocolVersion",1); native.insert("operation","tessellate");
            native.insert("parameters",QJsonObject{{"brep",QString::fromLatin1(QByteArray(bytes.data(),static_cast<int>(bytes.size())).toBase64())}});
            const auto solid=executeRequest(native);
            if(solid.value("ok")!=QJsonValue(true)) return fail("pad_tessellation_failed",solid.value("error").toObject().value("message").toString());
            result=solid.value("result").toObject();
        } else return fail("invalid_operation","Protocol v2 supports sketch and pad only.");
        auto out=identity; out.insert("ok",true); out.insert("result",result);
        if(QJsonDocument(out).toJson(QJsonDocument::Compact).size()>8*1024*1024) return fail("response_too_large","Result exceeds the worker limit.");
        return out;
    } catch(const Standard_Failure &e) { return fail("kernel_error",QString::fromLatin1(e.GetMessageString())); }
      catch(const std::exception &e) { return fail("sketch_error",QString::fromUtf8(e.what())); }
}
}
