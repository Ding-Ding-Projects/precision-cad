#pragma once
#include "../sketch/profile_identity.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QSet>
#include <cmath>

// Structural trust boundary only. Numerical correctness remains the isolated
// worker's responsibility; this validator never invokes libslvs on the UI thread.
inline bool validSketchPayload(const QJsonObject &result) {
  auto number=[](QJsonValue n) { return n.isDouble() && std::isfinite(n.toDouble()) && std::abs(n.toDouble())<=1e9; };
  auto id=[](QJsonValue v) { bool ok=false; const auto s=v.toString(); const auto n=s.toULongLong(&ok); return v.isString() && ok && n!=0 && QString::number(n)==s; };
  const auto solve=result.value("solve").toObject(); const QString status=solve.value("status").toString(); const double dof=solve.value("dof").toDouble(-1);
  if(solve.size()!=5 || (status!="solved" && status!="underConstrained") || !std::isfinite(dof) || dof<0 || dof>8192 || std::floor(dof)!=dof || (status=="solved")!=(dof==0) || !solve.value("conflicts").isArray() || !solve.value("conflicts").toArray().isEmpty() || !solve.value("points").isArray() || !solve.value("radii").isArray()) return false;
  QHash<QString,QJsonObject> entities, points; QHash<QString,QJsonValue> radii; QSet<QString> modelPoints, modelCircles, modelCurves; QHash<QString,QString> parent;
  const auto model=result.value("model").toObject();
  for(const auto &entry:model.value("entities").toArray()) {
    const auto e=entry.toObject(); const auto identity=e.value("id"); const QString key=identity.toString(),kind=e.value("kind").toString();
    if(!id(identity) || entities.contains(key)) return false; entities.insert(key,e);
    if(kind=="point") { modelPoints.insert(key); parent.insert(key,key); }
    else if(kind=="circle") { modelCircles.insert(key); modelCurves.insert(key); }
    else if(kind=="line") modelCurves.insert(key); else return false;
  }
  if(entities.isEmpty() || entities.size()>4096) return false;
  auto root=[&](QString key) { while(parent.value(key)!=key) key=parent.value(key); return key; };
  for(const auto &entry:model.value("constraints").toArray()) { const auto c=entry.toObject(); if(c.value("kind")==QJsonValue("coincident")) { const QString a=c.value("first").toString(),b=c.value("second").toString(); if(!parent.contains(a)||!parent.contains(b)) return false; parent[root(b)]=root(a); } }
  for(const auto &entry:solve.value("points").toArray()) {
    const auto p=entry.toObject(); const QString key=p.value("id").toString();
    if(p.size()!=6 || !modelPoints.contains(key) || points.contains(key)) return false;
    for(const char *k:{"u","v","x","y","z"}) if(!number(p.value(k))) return false;
    points.insert(key,p);
  }
  if(points.size()!=modelPoints.size()) return false;
  for(const auto &entry:solve.value("radii").toArray()) { const auto r=entry.toObject(); const QString key=r.value("id").toString(); if(r.size()!=2 || !modelCircles.contains(key) || radii.contains(key) || !number(r.value("radius")) || r.value("radius").toDouble()<=0) return false; radii.insert(key,r.value("radius")); }
  if(radii.size()!=modelCircles.size()) return false;
  const auto profiles=result.value("profiles").toObject(), preview=result.value("preview").toObject(); const auto loops=profiles.value("loops").toArray(), regions=profiles.value("regions").toArray();
  if(profiles.size()!=2 || loops.isEmpty() || loops.size()>4096 || regions.isEmpty() || regions.size()>4096 || preview.size()!=1 || !preview.value("segments").isArray()) return false;
  QHash<QString,bool> loopDirections; QSet<QString> sourceIds,assignedLoops; QJsonArray flattened;
  for(const auto &value:loops) {
    const auto loop=value.toObject(); const QString loopId=loop.value("stableId").toString(); const auto segments=loop.value("segments").toArray();
    if(loop.size()!=3 || loopDirections.contains(loopId) || !loop.value("clockwise").isBool() || segments.isEmpty() || segments.size()>4096) return false;
    QVector<quint64> sources;
    for(qsizetype i=0;i<segments.size();++i) {
      const auto s=segments[i].toObject(); const QString source=s.value("sourceEntityId").toString(),kind=s.value("kind").toString();
      if(s.size()!=11 || !modelCurves.contains(source) || sourceIds.contains(source)) return false;
      const auto entity=entities.value(source); if(entity.value("kind")!=s.value("kind")) return false;
      for(const char *k:{"startU","startV","endU","endV","centerU","centerV","radius"}) if(!number(s.value(k))) return false;
      if(kind=="line") {
        const QString a=s.value("startPointId").toString(),b=s.value("endPointId").toString();
        const QString originalA=entity.value("startPointId").toString(),originalB=entity.value("endPointId").toString();
        if(!((a==originalA && b==originalB)||(a==originalB && b==originalA)) || !points.contains(a) || !points.contains(b)) return false;
        if(s.value("startU")!=points[a].value("u") || s.value("startV")!=points[a].value("v") || s.value("endU")!=points[b].value("u") || s.value("endV")!=points[b].value("v") || s.value("centerU")!=QJsonValue(0) || s.value("centerV")!=QJsonValue(0) || s.value("radius")!=QJsonValue(0)) return false;
        const auto next=segments[(i+1)%segments.size()].toObject(); const QString nextStart=next.value("startPointId").toString();
        if(next.value("kind")!=QJsonValue("line") || !parent.contains(nextStart) || root(b)!=root(nextStart)) return false;
      } else if(kind=="circle") {
        const QString center=entity.value("centerPointId").toString();
        if(segments.size()!=1 || !points.contains(center) || s.value("startPointId")!=QJsonValue("0") || s.value("endPointId")!=QJsonValue("0") || s.value("centerU")!=points[center].value("u") || s.value("centerV")!=points[center].value("v") || s.value("radius")!=radii.value(source)) return false;
        for(const char *k:{"startU","startV","endU","endV"}) if(s.value(k)!=QJsonValue(0)) return false;
      } else return false;
      sourceIds.insert(source); sources.append(source.toULongLong()); flattened.append(segments[i]);
    }
    if(loopId!=precision::sketch::profileLoopId(sources)) return false;
    loopDirections.insert(loopId,loop.value("clockwise").toBool());
  }
  if(sourceIds!=modelCurves) return false;
  for(const auto &value:regions) {
    const auto region=value.toObject(); const QString outer=region.value("outerLoopId").toString();
    if(region.size()!=3 || region.value("stableId")!=QJsonValue(precision::sketch::profileRegionId(outer)) || !loopDirections.contains(outer) || loopDirections.value(outer) || assignedLoops.contains(outer) || !region.value("holeLoopIds").isArray()) return false;
    assignedLoops.insert(outer);
    for(const auto &hole:region.value("holeLoopIds").toArray()) { const QString h=hole.toString(); if(!hole.isString() || !loopDirections.contains(h) || !loopDirections.value(h) || assignedLoops.contains(h)) return false; assignedLoops.insert(h); }
  }
  return assignedLoops.size()==loopDirections.size() && flattened==preview.value("segments").toArray();
}
