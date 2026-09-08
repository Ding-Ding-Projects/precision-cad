#pragma once
#include <QStringList>
#include <QVector>
#include <algorithm>
namespace precision::sketch {
inline QString profileLoopId(QVector<quint64> ids) {
    std::sort(ids.begin(),ids.end()); QStringList parts;
    for(auto id:ids) parts.append(QString::number(id));
    return QStringLiteral("loop:")+parts.join(QLatin1Char(','));
}
inline QString profileRegionId(const QString &outerLoopId) { return QStringLiteral("region:")+outerLoopId; }
}
