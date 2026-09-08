#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>
#include <cmath>

namespace precision::core {
// Pure record construction, deliberately independent of libslvs and the UI thread.
inline QJsonObject rectangleHoleModel(const QString &id, const QString &plane,
                                     double width, double height, double radius, double u, double v) {
    if ((plane != "xy" && plane != "yz" && plane != "zx") || id.isEmpty()) return {};
    for (double n : {width,height,radius,u,v}) if (!std::isfinite(n) || std::abs(n)>1e9) return {};
    if (width<=0 || height<=0 || radius<=0 || u-radius<=0 || v-radius<=0 || u+radius>=width || v+radius>=height) return {};
    QJsonArray entities, constraints;
    auto entity=[&](int n,const char *kind,double x,double y,double r,int a,int b,int c) {
        entities.append(QJsonObject{{"id",QString::number(n)},{"kind",kind},{"label",""},{"u",x},{"v",y},{"radius",r},
            {"startPointId",QString::number(a)},{"endPointId",QString::number(b)},{"centerPointId",QString::number(c)}});
    };
    entity(1,"point",0,0,0,0,0,0); entity(2,"point",width,0,0,0,0,0);
    entity(3,"point",width,height,0,0,0,0); entity(4,"point",0,height,0,0,0,0);
    entity(5,"point",u,v,0,0,0,0); entity(6,"circle",0,0,radius,0,0,5);
    entity(10,"line",0,0,0,1,2,0); entity(11,"line",0,0,0,2,3,0);
    entity(12,"line",0,0,0,3,4,0); entity(13,"line",0,0,0,4,1,0);
    auto constraint=[&](int n,const char *kind,int a,int b,double value) {
        constraints.append(QJsonObject{{"id",QString::number(n)},{"kind",kind},{"label",""},{"first",QString::number(a)},
            {"second",b?QString::number(b):QString()},{"value",value}});
    };
    constraint(1,"fixed",1,0,0); constraint(2,"distance",1,2,width); constraint(3,"distance",2,3,height);
    constraint(4,"fixed",5,0,0); constraint(5,"horizontal",10,0,0); constraint(6,"vertical",11,0,0);
    constraint(7,"horizontal",12,0,0); constraint(8,"vertical",13,0,0); constraint(9,"radius",6,0,radius);
    return {{"schemaVersion",1},{"id",id},{"units","mm"},
        {"plane",QJsonObject{{"id",plane},{"originX",0},{"originY",0},{"originZ",0},
            {"normalX",plane=="yz"?1:0},{"normalY",plane=="zx"?1:0},{"normalZ",plane=="xy"?1:0}}},
        {"entities",entities},{"constraints",constraints}};
}
inline QVariantMap rectangleHoleDimensions(const QJsonObject &model) {
    const auto constraints=model.value("constraints").toArray(), entities=model.value("entities").toArray();
    if(constraints.size()!=9 || entities.size()!=10) return {{"editable",false}};
    const double w=constraints[1].toObject().value("value").toDouble(), h=constraints[2].toObject().value("value").toDouble();
    const double r=constraints[8].toObject().value("value").toDouble(), u=entities[4].toObject().value("u").toDouble(), v=entities[4].toObject().value("v").toDouble();
    const QString plane=model.value("plane").toObject().value("id").toString();
    // Exact comparison refuses mislabeled or altered imported models rather than editing another constraint.
    if(model!=rectangleHoleModel(model.value("id").toString(),plane,w,h,r,u,v)) return {{"editable",false}};
    return {{"editable",true},{"plane",plane},{"width",w},{"height",h},{"holeRadius",r},{"holeU",u},{"holeV",v}};
}
}
